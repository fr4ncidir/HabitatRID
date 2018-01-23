/*
 * ridMain.c
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
gcc -Wall -I/usr/local/include ridMain.c RIDLib.c serial.c ../SEPA-C/sepa_producer.c ../SEPA-C/sepa_utilities.c ../SEPA-C/jsmn.c -o ridReader -lgsl -lgslcblas -lm -lcurl `pkg-config --cflags --libs glib-2.0`
 * G_MESSAGES_DEBUG=all
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include "RIDLib.h"

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
	uint8_t *id_array;
	intVector *rowOfSums,*rowOfDiffs;
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
	close(ridSerial.serial_fd);
	
	rowOfSums = gsl_vector_int_alloc(parameters.ANGLE_ITERATIONS);
	rowOfDiffs = gsl_vector_int_alloc(parameters.ANGLE_ITERATIONS);
	
	do {
		// serial reopening
		if (open_serial(usb_address,&ridSerial) == ERROR) return EXIT_FAILURE;
		// serial opening end
		
		sleep(1);
		ioctl(ridSerial.serial_fd, TCFLSH, 2); // flush both
		
		// sending '<'
		result = send_request();
		if (result==EXIT_FAILURE) {
			g_critical("send_request failure");
			break;
		}
		g_debug("Request packet sent");
		
		// must receive "<\n"
		result = receive_request_confirm();
		if (result==EXIT_FAILURE) {
			sleep(1);
			g_critical("receive_request failure");
			result = send_reset();
			if (result==EXIT_FAILURE) {
				g_critical("send_reset failure");
				return EXIT_FAILURE;
			}
			g_debug("Reset packet sent");
			continue;
		}
		g_debug("Confirmation packet received");
		
		// #id id_code_1 id_code_1 id_code_2 id_code_2 ...
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
			
			// 40 angles iterations
			result = angle_iterations(nID,id_array_size,id_array);
			if (result==EXIT_FAILURE) {
				g_critical("angle_iterations failure");
				break;
			}
			g_debug("Angle iterations ended");
			
			// send '>'
			result = send_detect();
			if (result==EXIT_FAILURE) {
				g_critical("send_detect failure");
				break;
			}
			g_debug("Detect packet sent");
			
			// must receive 40 10 time1 10 time2 10			
			result = receive_end_scan();
			if (result==EXIT_FAILURE) {
				g_critical("receive_end_scan failure");
				break;
			}
			g_message("End scan reached - iterations %d",iterations);
			
			g_debug("Location calculation started");
			for (j=0; j<nID; j++) {
				gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
				gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
				last_location = locateFromData(rowOfDiffs,rowOfSums,parameters.ANGLE_ITERATIONS);
				last_location.id = gsl_vector_int_get(idVector,j);
				printLocation(stdout,last_location);
				log_file_txt(idVector,rowOfDiffs,rowOfSums,j,parameters.ANGLE_ITERATIONS,last_location,logFileNameTXT);
				//sepaLocationUpdate(parameters.http_sepa_address,last_location);
				g_debug("Location calculation ended for ID%d",j);
			}
			
			usleep(1000*(parameters.sample_time));
			if (!continuousRead) {
				iterations--;
				g_message("%d iterations remaining",iterations);
			}
		}
		
		// send '+'
		result = send_reset();
		if (result==EXIT_FAILURE) {
			g_critical("send_reset failure");
			return EXIT_FAILURE;
		}
		g_debug("Reset packet sent");
		
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
