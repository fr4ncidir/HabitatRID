/*
 * ridMain2D.c
 * 
 * Copyright 2017 Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
gcc -Wall -I/usr/local/include ridMain2D.c RIDLib.c serial.c ../SEPA-C/sepa_producer.c ../SEPA-C/sepa_utilities.c ../SEPA-C/jsmn.c -o ridReader -lgsl -lgslcblas -lm -lcurl `pkg-config --cflags --libs glib-2.0`
 * G_MESSAGES_DEBUG=all
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include "RIDLib.h"

// TODO use glib assert

volatile int continuousRead = 0;
int id_array_size;

extern RidParams parameters;
extern SerialOptions ridSerial;
extern intVector *idVector;
extern intMatrix *sumVectors;
extern intMatrix *diffVectors;

int ridExecution(const char *usb_address,int iterations);
void printUsage();

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	g_critical("Caught Ctrl-C: stopping reading RID\n");
	continuousRead = 0;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char **argv) {
	char *usbportAddress=NULL;
	int cmdlineOpt,force_missingOption=0,my_optopt,i,iterationNumber=0,execution_result=0;
	uint8_t execution_code = 0;
	
#ifdef VERBOSE_CALCULATION
	FILE *verbose;
	verbose = fopen("./readAllAnglesLog.txt","w");
	fclose(verbose);
#endif
	
	g_log_set_handler(NULL, G_LOG_LEVEL_MASK, g_log_default_handler,NULL);
	
	opterr = 0;
	if ((argc==1) || ((argc==2) && (!strcmp(argv[1],"help")))) {
		printUsage();
		return EXIT_SUCCESS;
	}
	
	while ((cmdlineOpt = getopt(argc, argv, "n:u:f:"))!=-1) {
		if (((cmdlineOpt=='n') || (cmdlineOpt=='u') || (cmdlineOpt=='f')) && 
		((!strcmp(optarg,"")) || (optarg[0]=='-'))) {
			// check if parameters n,u,s,f that must have specification do not have one. In that case, we switch to '?' case
			force_missingOption = 1;
			my_optopt = cmdlineOpt;
			cmdlineOpt = '?';
		}
		switch (cmdlineOpt) {
			case 'n':
				// the number of iterations
				execution_code |= 0x01;
				for (i=0; i<strlen(optarg); i++) {
					// optarg must be an integer number
					if (!isdigit(optarg[i])) {
						g_critical("%s is not an integer: -n argument must be an integer.\n",optarg);
						free(usbportAddress);
						printUsage();
						return EXIT_FAILURE;
					}
				}
				sscanf(optarg,"%d",&iterationNumber);
				break;
			case 'u':
				// the path to usb port
				execution_code |= 0x02;
				usbportAddress = strdup(optarg);
				if (usbportAddress==NULL) {
					g_error("malloc error in usbportAddress.");
					return EXIT_FAILURE;
				}
				break;
			case 'f':
				// this parameter specifies where the configuration parameters JSON is stored
				execution_code |= 0x04;
				if (parametrize(optarg)==EXIT_SUCCESS) break;
				g_critical("Parametrization from %s failed.",optarg);
				break;
			default: // case '?'
				if (!force_missingOption) my_optopt = optopt;
				if ((my_optopt=='n') || (my_optopt=='u') || (my_optopt=='f')) g_critical("Option -%c requires an argument.",my_optopt);
				else {
					if (isprint(my_optopt)) g_critical("Unknown option -%c.",my_optopt);
					else g_critical("Unknown option character \\x%x.",my_optopt);
				}
				free(usbportAddress);
				g_critical("Wrong syntax!\n");
				printUsage();
				return EXIT_FAILURE;
		}
	}
	
	switch (execution_code) {
		case 0x07:
			if (iterationNumber) {
			g_debug("Requested %d iterations.\n",iterationNumber);
			break;
		}
		case 0x06:
			g_debug("Requested infinite iterations.\n");
			continuousRead = 1;
			break;
		default:
			g_error("Error 2 %04x!\n",execution_code);
			g_critical("Wrong syntax!\n");
			printUsage();
			return EXIT_FAILURE;
	}
	g_debug("USB port address: %s\n",usbportAddress);
	g_debug("Sepa address: %s\n",parameters.http_sepa_address);
	
	g_message("Use Ctrl-c to stop reading");
	signal(SIGINT,interruptHandler);
	
	execution_result = ridExecution(usbportAddress,iterationNumber);
	if (execution_result==EXIT_FAILURE) g_critical("Execution ended with code EXIT_FAILURE.\n");
	else g_message("Execution ended with code EXIT_SUCCESS.\n");
	
	free(usbportAddress);
	return execution_result;
}

int ridExecution(const char *usb_address,int iterations) {
	int result,read_bytes,nID,j,iter_done=0;
	uint8_t id_info_result[ALLOC_ID_MAX];
	uint8_t *id_array,*h_scan,*v_scan;
	intVector *rowOfSums,*rowOfDiffs,*rowOfDiffs_v;
	coord last_location;
	char logFileNameTXT[100]="";
	
	// serial opening
	ridSerial.baudRate = B115200;
	ridSerial.dataBits = CS8;
	ridSerial.parityBit = NO_PARITY;
	ridSerial.stopbits = ONE_STOP;
	if (open_serial(usb_address,&ridSerial) == ERROR) return EXIT_FAILURE;
	// serial opening end
	
	sleep(1);
	ioctl(ridSerial.serial_fd, TCFLSH, 2); // flush both
	
	result = send_reset();
	if (result==EXIT_FAILURE) {
		g_critical("send_reset failure");
		return EXIT_FAILURE;
	}
	g_debug("Reset packet sent");
	
	do {
		
		sleep(1);
		ioctl(ridSerial.serial_fd, TCFLSH, 2); // flush both
		
		// sending 'R'
		result = send_request_R();
		if (result==EXIT_FAILURE) {
			g_critical("send_request failure");
			break;
		}
		g_debug("R packet sent");
		
		// must receive "R\n"
		result = receive_request_confirm('R');
		if (result==EXIT_FAILURE) {
			sleep(1);
			g_critical("receive_request_confirm failure");
			return EXIT_FAILURE;
		}
		g_debug("R confirmation packet received");
		
		// sending row number
		result = write_serial(ridSerial.serial_fd,ONE_BYTE,(void*) &(parameters.row));
		if (result==EXIT_FAILURE) {
			g_critical("Error in sending row number");
			break;
		}
		g_debug("Row number %u sent",parameters.row);
		
		// must receive back the same row number and '\n'
		result = receive_request_confirm((char) parameters.row);
		if (result==EXIT_FAILURE) {
			sleep(1);
			g_critical("receive_request_confirm failure");
			return EXIT_FAILURE;
		}
		g_debug("Confirmation packet received");
		
		// sending again 'R'
		result = send_request_R();
		if (result==EXIT_FAILURE) {
			g_critical("send_request failure");
			break;
		}
		g_debug("R packet sent");
		
		// #id id_code_1 id_code_2 ...
		result = receive_id_info(id_info_result,&read_bytes);
		if (result==EXIT_FAILURE) {
			g_critical("receive_id failure");
			break;
		}
		nID = (int) id_info_result[0];
		g_message("Received %d id-info",nID);
		
		if (nID>0) {
			// if #id > 0, else it's useless to scan
			id_array_size = read_bytes-1;
			id_array = id_info_result+1;
			
			idVector = gsl_vector_int_alloc(nID);
			for (i=0; i<nID; i++) {
				g_message("ID%d: %d",i,id_array[i]);
				gsl_vector_int_set(idVector,i,id_array[i]);
			}
			
			// sending again 'R'
			result = send_request_R();
			if (result==EXIT_FAILURE) {
				g_critical("send_request failure");
				break;
			}
			g_debug("R packet sent");
			
			// read horizontal scan result
			h_scan = (uint8_t*) malloc((4*nID+2)*sizeof(uint8_t));
			if (h_scan==NULL) {
				g_critical("malloc error in h_scan");
				return EXIT_FAILURE;
			}
			result = scan_results(h_scan,&read_bytes,nID);
			
			sumVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
			diffVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
			for (j=0; j<nID; j++) {
				for (i=0; i<parameters.ANGLE_ITERATIONS; i++) {
					gsl_matrix_int_set(sumVectors,j,i,h_scan[4*(nID*i+j)]);
					gsl_matrix_int_set(diffVectors,j,i,h_scan[4*(nID*i+j)+1]);
				}
			}
			
			g_debug("x-y location calculation started");
			for (j=0; j<nID; j++) {
				gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
				gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
				last_location = locateFromData_XY(rowOfDiffs,rowOfSums,parameters.ANGLE_ITERATIONS);
				last_location.id = gsl_vector_int_get(idVector,j);
				g_debug("x-y location calculation ended for ID%d",j);
			}
			
			// sending 'C'
			result = send_request_C();
			if (result==EXIT_FAILURE) {
				g_critical("send_request failure");
				break;
			}
			g_debug("C packet sent");
			
			// must receive "C\n"
			result = receive_request_confirm('C');
			if (result==EXIT_FAILURE) {
				sleep(1);
				g_critical("receive_request_confirm failure");
				return EXIT_FAILURE;
			}
			g_debug("C confirmation packet received");
			
			// sending col number
			result = write_serial(ridSerial.serial_fd,ONE_BYTE,(void*) &(parameters.col));
			if (result==EXIT_FAILURE) {
				g_critical("Error in sending row number");
				break;
			}
			g_debug("Col number %u sent",parameters.col);
			
			// must receive back the same col number and '\n'
			result = receive_request_confirm((char) parameters.col);
			if (result==EXIT_FAILURE) {
				sleep(1);
				g_critical("receive_request_confirm failure");
				return EXIT_FAILURE;
			}
			g_debug("Confirmation packet received");
			
			// sending again 'C'
			result = send_request_C();
			if (result==EXIT_FAILURE) {
				g_critical("send_request failure");
				break;
			}
			g_debug("C packet sent");
			
			// #id id_code_1 id_code_2 ...
			result = receive_id_info(id_info_result,&read_bytes);
			if (result==EXIT_FAILURE) {
				g_critical("receive_id failure");
				break;
			}
				
			// sending again 'C'
			result = send_request_C();
			if (result==EXIT_FAILURE) {
				g_critical("send_request failure");
				break;
			}
			g_debug("R packet sent");
				
			// read vertical scan result
			v_scan = (uint8_t*) malloc((4*nID+2)*sizeof(uint8_t));
			if (v_scan==NULL) {
				g_critical("malloc error in v_scan");
				return EXIT_FAILURE;
			}
			result = scan_results(v_scan,&read_bytes,nID);
				
			sumVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
			diffVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
			for (j=0; j<nID; j++) {
				for (i=0; i<parameters.ANGLE_ITERATIONS; i++) {
					gsl_matrix_int_set(sumVectors,j,i,h_scan[4*(nID*i+j)]);
					gsl_matrix_int_set(diffVectors,j,i,h_scan[4*(nID*i+j)+2]);
				}
			}
			
			g_debug("x-y-h location calculation started");
			for (j=0; j<nID; j++) {
				gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
				gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
				last_location = locateFromData_H(rowOfDiffs,rowOfSums,parameters.ANGLE_ITERATIONS);
				printf("Location of id %d: (x,y,h)=(%lf,%lf,%lf)\n",last_location.id,last_location.x,last_location.y,last_location.h);
				log_file_txt(idVector,rowOfDiffs,rowOfSums,j,nID,parameters.ANGLE_ITERATIONS,last_location,logFileNameTXT);
				sepaLocationUpdate(parameters.http_sepa_address,parameters.rid_identifier,last_location);
				g_debug("Location calculation ended for ID%d",j);
			}
			
			usleep(1000*(parameters.sample_time));
			if (!continuousRead) {
				iterations--;
				g_message("%d iterations remaining",iterations);
			}
		}
		
		iter_done++;
	} while ((continuousRead) || (iterations>0));
	g_message("Done %d iterations",iter_done);
	
	gsl_vector_int_free(idVector);
	gsl_vector_int_free(rowOfSums);
	gsl_vector_int_free(rowOfDiffs);
	gsl_matrix_int_free(sumVectors);
	gsl_matrix_int_free(diffVectors);
	close(ridSerial.serial_fd);
	return EXIT_SUCCESS;
}

void printUsage() {
	execlp("less","less","./manpage.txt",NULL);
	perror("Couldn't print manpage - ");
	exit(EXIT_FAILURE);
}
