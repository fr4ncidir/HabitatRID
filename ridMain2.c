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
gcc -Wall -I/usr/local/include ridMain2.c RIDLib.c serial.c ../sepa-C-kpi/sepa_producer.c ../sepa-C-kpi/sepa_utilities.c ../sepa-C-kpi/jsmn.c -o ridReader -lgsl -lgslcblas -lm -lcurl `pkg-config --cflags --libs glib-2.0`
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <glib.h>
#include "serial.h"
#include "RIDLib.h"

volatile int continuousRead = FALSE;
intVector *idVector;
intMatrix *sumVectors;
intMatrix *diffVectors;
uint8_t nID;
SerialOptions ridSerial;

extern uint8_t request_packet[STD_PACKET_STRING_DIM];
extern uint8_t reset_packet[STD_PACKET_STRING_DIM];
extern uint8_t detect_packet[STD_PACKET_STRING_DIM];
extern size_t std_packet_size;
extern RidParams parameters;

int ridExecution(const char * usb_address,int iterations);
int readAllAngles(int nAngles,size_t id_array_size);
void printUsage(const char * error_message);

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	g_message("Caught Ctrl-C: stopping reading RID\n");
	continuousRead = FALSE;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char ** argv) {
	char *usbportAddress=NULL;
	uint8_t execution_code = 0;
	int cmdlineOpt,force_missingOption=0,my_optopt,i,iterationNumber=0,execution_result=0;
	
#ifdef VERBOSE_CALCULATION
	FILE * verbose;
	verbose = fopen("./readAllAnglesLog.txt","w");
	fclose(verbose);
#endif
	
	g_log_set_handler(NULL, G_LOG_LEVEL_MASK, g_log_default_handler,NULL);
	
	opterr = 0;
	if ((argc==1) || ((argc==2) && (!strcmp(argv[1],"help")))) {
		printUsage(NULL);
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
						printUsage(NULL);
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
				printUsage("Wrong syntax!\n");
				return EXIT_FAILURE;
		}
	}
	
	switch (execution_code) {
		case 0x07:
			g_debug("Requested %d iterations.\n",iterationNumber);
			break;
		case 0x06:
			g_debug("Requested infinite iterations.\n");
			break;
		default:
			printUsage("Wrong syntax!\n");
			g_error("Error 2 %04x!\n",execution_code);
			return EXIT_FAILURE;
	}
	g_debug("USB port address: %s\n",usbportAddress);
	g_debug("Sepa address: %s\n",parameters.http_sepa_address);
	
	execution_result = ridExecution(usbportAddress,iterationNumber);
	if (execution_result==EXIT_FAILURE) g_critical("Execution ended with code EXIT_FAILURE.\n");
	else g_message("Execution ended with code EXIT_SUCCESS.\n");
	return execution_result;
}

int ridExecution(const char * usb_address,int iterations) {
	// functioning core
	coord last_location;
	int i,j,result,protocol_error=0;
	char logFileNameTXT[100]="";
	uint8_t *id_array;
	intVector *rowOfSums,*rowOfDiffs;
	size_t id_array_size;
	uint8_t request_confirm[STD_PACKET_STRING_DIM] = {'x','x','\0'};
	
	// serial opening
	ridSerial.baudRate = B115200;
	ridSerial.dataBits = CS8;
	ridSerial.parityBit = NO_PARITY;
	ridSerial.stopbits = ONE_STOP;
	if (open_serial(usb_address,&ridSerial) == ERROR) return EXIT_FAILURE;
	// serial opening end
		
	// writes to serial "+\n": rid reset
	if (send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure") == EXIT_FAILURE) {
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	g_message("Reset packet sent");
	sleep(1);
	// rid reset end
		
	// writes to serial "<\n": start transmission request
	if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	g_message("Request packet sent");
	// end transmission request
		
	// reads from serial "<\n" request confirm and check
	result = read_until_terminator(ridSerial.serial_fd,std_packet_size,request_confirm,SCHWARZENEGGER);
	if (result == ERROR) g_critical("request confirm command read_until_terminator failure");
	else {
		protocol_error = strcmp((char*) request_packet,(char*) request_confirm);
		if (protocol_error) g_critical("Received unexpected %s instead of %s\n",(char*) request_confirm,(char*) request_packet);
	}
	if ((result==ERROR) || (protocol_error)) {
		send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure");
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	g_message("Confirmation received");
	// read request confirm end
		
	// reads from serial the number of ids
	result = read_nbyte(ridSerial.serial_fd,sizeof(uint8_t),&nID);
	if ((result==EXIT_FAILURE) || (nID==WRONG_NID)) {
		g_critical("nID number read_nbyte failure: result=%d, nID=%d",result,nID);
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	g_message("Number of id received: %d",nID);
	// read number of id end
		
	// reads from serial the ids
	id_array_size = (2*nID+1)*sizeof(uint8_t);
	id_array = (uint8_t*) malloc(id_array_size);
	// id_array is checked !=NULL in read_until_terminator
	result = read_until_terminator(ridSerial.serial_fd,id_array_size,id_array,SCHWARZENEGGER);
	if (result == ERROR) {
		g_critical("id list command read_until_terminator failure");
		free(id_array);
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	idVector = gsl_vector_int_alloc(nID);
	j=0;
	for (i=0; i<2*nID; i=i+2) {
		gsl_vector_int_set(idVector,j,id_array[i]);
		j++;
	}
	free(id_array);
	g_debug("Id list received");
	// read ids end
		
	// retrieves sum and diff vectors
	sumVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
	diffVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);

	g_message("Use Ctrl-c to stop reading");
	signal(SIGINT,interruptHandler);

	rowOfSums = gsl_vector_int_alloc(parameters.ANGLE_ITERATIONS);
	rowOfDiffs = gsl_vector_int_alloc(parameters.ANGLE_ITERATIONS);
	
	if (http_client_init()==EXIT_FAILURE) return EXIT_FAILURE;
	if (!iterations) continuousRead = TRUE;
	do {
		result = readAllAngles(parameters.ANGLE_ITERATIONS,id_array_size);
		if (result == EXIT_FAILURE) break;

		for (j=0; j<nID; j++) {
			gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
			gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
			last_location = locateFromData(rowOfSums,rowOfDiffs,parameters.ANGLE_ITERATIONS);
			last_location.id = gsl_vector_int_get(idVector,j);
			printLocation(stdout,last_location);
			log_file_txt(idVector,rowOfSums,rowOfDiffs,j,parameters.ANGLE_ITERATIONS,last_location,logFileNameTXT);
			sepaLocationUpdate(parameters.http_sepa_address,last_location);
		}
		
		iterations--;
		usleep(1000*(parameters.sample_time));
	} while ((continuousRead) || (iterations>0));
	
	send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Detect packet send failure");
	http_client_free();
	gsl_vector_int_free(rowOfSums);
	gsl_vector_int_free(rowOfDiffs);
	gsl_matrix_int_free(sumVectors);
	gsl_matrix_int_free(diffVectors);
	gsl_vector_int_free(idVector);
	close(ridSerial.serial_fd);
	return EXIT_SUCCESS;
}

int readAllAngles(int nAngles,size_t id_array_size) {
	// one-complete-read core function
	uint8_t *sum_diff_array;
	char error_message[50];
	int i,j,result;
	GTimer *timer;
#ifdef VERBOSE_CALCULATION
	FILE * verbose;
	verbose = fopen("./readAllAnglesLog.txt","a");
	if (verbose==NULL) {
		g_error("Verbose failure");
		return EXIT_FAILURE;
	}
#endif
	
	sum_diff_array = (uint8_t*) malloc(id_array_size);
	if (sum_diff_array==NULL) {
		g_critical("malloc error in readAllAngles");
		return EXIT_FAILURE;
	}
	
	fprintf(stderr,"Angle iterations");
	timer = g_timer_new();
	for (i=0; i<nAngles; i++) {
		fprintf(stderr,".");
		// writes to serial "<\n"
		sprintf(error_message,"Sending request packet '%u' for the %d-th angle failure",request_packet[0],i+1);
#ifdef VERBOSE_CALCULATION		
		fprintf(verbose,"Angolo %d: %s",i+1,request_packet);
#endif
		if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,error_message) == EXIT_FAILURE) {
			free(sum_diff_array);
			return EXIT_FAILURE;
		}

		// reads sum and diff values
		result = read_until_terminator(ridSerial.serial_fd,id_array_size,sum_diff_array,SCHWARZENEGGER);
		if (result == ERROR) {
			g_critical("Reading sum-diff vector for %d-th angle failure",i+1);
			free(sum_diff_array);
			return EXIT_FAILURE;
		}

		// puts data in vectors
		for (j=0; j<nID; j++) {
#ifdef VERBOSE_CALCULATION
			fprintf(verbose,"sum_diff_array:\nS\tD\n");
			fprintf(verbose,"%u\t%u\n",sum_diff_array[2*j],sum_diff_array[2*j+1]);
			fprintf(verbose,"%d\t%d\n\n",sum_diff_array[2*j]-CENTRE_RESCALE,sum_diff_array[2*j+1]-CENTRE_RESCALE);
#endif
			gsl_matrix_int_set(sumVectors,j,i,sum_diff_array[2*j]-CENTRE_RESCALE);
			gsl_matrix_int_set(diffVectors,j,i,sum_diff_array[2*j+1]-CENTRE_RESCALE);
		}
	}
	g_timer_stop(timer);
#ifdef VERBOSE_CALCULATION
	fclose(verbose);
#endif
	fprintf(stderr,"completed in %lf ms\n",g_timer_elapsed(timer,NULL)*1000);
	g_timer_destroy(timer);
	free(sum_diff_array);
	return EXIT_SUCCESS;
}

void printUsage(const char * error_message) {
	if (error_message!=NULL) g_critical("%s",error_message);
	execlp("cat","cat","./manpage.txt",NULL);
	perror("Couldn't print manpage - ");
	exit(EXIT_FAILURE);
}
