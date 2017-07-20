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
gcc -Wall -I/usr/local/include ridMain2.c RIDLib.c serial.c zf_log.c ../sepa-C-kpi/sepa_producer.c ../sepa-C-kpi/sepa_utilities.c ../sepa-C-kpi/jsmn.c -o ridReader -lgsl -lgslcblas -lm -lcurl `pkg-config --cflags --libs glib-2.0`
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
#include "RIDLib.h"

#define LOG_YES
#ifdef LOG_YES
#define ZF_LOG_LEVEL ZF_LOG_INFO
#include "zf_log.h"
#endif

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

int ridExecution(const char * usb_address,const char * SEPA_address,int iterations);
int readAllAngles(int nAngles,size_t id_array_size);

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	printf("Info: Caught Ctrl-C: stopping reading RID\n");
	continuousRead = FALSE;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char ** argv) {
	char *usbportAddress=NULL;
	uint8_t execution_code = 0;
	int cmdlineOpt,force_missingOption=0,my_optopt,i,iterationNumber=0,execution_result=0;
	
	zf_log_set_tag_prefix("HabitatRID");
	
	opterr = 0;
	if ((argc==1) || ((argc==2) && (!strcmp(argv[1],"help")))) {
		printUsage(NULL);
		return EXIT_SUCCESS;
	}
	
	while ((cmdlineOpt = getopt(argc, argv, "n:u:f:"))!=-1) {
		if (((cmdlineOpt=='n') || (cmdlineOpt=='u') || (cmdlineOpt=='f')) && (!strcmp(optarg,""))) {
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
						fprintf(stderr,"%s is not an integer: -n argument must be an integer.\n",optarg);
						free_arrays(1,usbportAddress);
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
					fprintf(stderr,"malloc error in usbportAddress.\n");
					return EXIT_FAILURE;
				}
				break;
			case 'f':
				// this parameter specifies where the configuration parameters JSON is stored
				execution_code |= 0x04;
				if (parametrize(optarg)==EXIT_SUCCESS) break;
				fprintf(stderr,"Parametrization from %s failed.\n",optarg);
				break;
			default: // case '?'
				if (!force_missingOption) my_optopt = optopt;
				if ((my_optopt=='n') || (my_optopt=='u') || (my_optopt=='f')) fprintf(stderr,"Option -%c requires an argument.\n",my_optopt);
				else {
					if (isprint(my_optopt)) fprintf(stderr,"Unknown option -%c.\n",my_optopt);
					else fprintf(stderr,"Unknown option character \\x%x.\n",my_optopt);
				}
				free_arrays(1,usbportAddress);
				printUsage("Wrong syntax!\n");
				return EXIT_FAILURE;
		}
	}
	
	switch (execution_code) {
		case 0x07:
			printf("Requested %d iterations.\n",iterationNumber);
			break;
		case 0x06:
			printf("Requested infinite iterations.\n");
			break;
		default:
			printUsage("Wrong syntax!\n");
			printf("Error 2 %04x!\n",execution_code);
			return EXIT_FAILURE;
	}
	printf("USB port address: %s\n",usbportAddress);
	printf("Sepa address: %s\n",parameters.http_sepa_address);
	
	execution_result = ridExecution(usbportAddress,iterationNumber);
	if (execution_result==EXIT_FAILURE) printf("Execution ended with code EXIT_FAILURE.\n");
	else printf("Execution ended with code EXIT_SUCCESS.\n");
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
	printf("Info: Reset packet sent\n");
	sleep(1);
	// rid reset end
		
	// writes to serial "<\n": start transmission request
	if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	printf("Info: Request packet sent\n");
	// end transmission request
		
	// reads from serial "<\n" request confirm and check
	result = read_until_terminator(ridSerial.serial_fd,std_packet_size,request_confirm,SCHWARZENEGGER);
	if (result == ERROR) fprintf(stderr,"request confirm command read_until_terminator failure\n");
	else {
		protocol_error = strcmp((char*) request_packet,(char*) request_confirm);
		if (protocol_error) fprintf(stderr,"Received unexpected %s instead of %s\n",(char*) request_confirm,(char*) request_packet);
	}
	if ((result==ERROR) || (protocol_error)) {
		send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure");
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	printf("Info: Confirmation received\n");
	// read request confirm end
		
	// reads from serial the number of ids
	result = read_nbyte(ridSerial.serial_fd,sizeof(uint8_t),&nID);
	if ((result==EXIT_FAILURE) || (nID==WRONG_NID)) {
		fprintf(stderr,"nID number read_nbyte failure: result=%d, nID=%d\n",result,nID);
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	printf("Info: Number of id received: %d\n",nID);
	// read number of id end
		
	// reads from serial the ids
	id_array_size = (2*nID+1)*sizeof(uint8_t);
	id_array = (uint8_t*) malloc(id_array_size);
	// id_array is checked !=NULL in read_until_terminator
	result = read_until_terminator(ridSerial.serial_fd,id_array_size,id_array,SCHWARZENEGGER);
	if (result == ERROR) {
		fprintf(stderr,"id list command read_until_terminator failure\n");
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
	printf("Info: Id list received\n");
	// read ids end
		
	// retrieves sum and diff vectors
	sumVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
	diffVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);

	printf("Info: use Ctrl-c to stop reading\n\n");
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
			log_file_txt(idVector,sumVectors,diffVectors,j,parameters.ANGLE_ITERATIONS,last_location,logFileNameTXT);
			sepaLocationUpdate(parameters.http_sepa_address,last_location,sparqlUpdate_unbounded);
		}
		
		iterations--;
		usleep(1000*(parameters.sample_time));
	} while ((continuousRead) || (iterations>0));
	
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
	
	sum_diff_array = (uint8_t*) malloc(id_array_size);
	if (sum_diff_array==NULL) {
		fprintf(stderr,"malloc error in readAllAngles\n");
		return EXIT_FAILURE;
	}
	
	printf("Info: Angle iterations");
	for (i=0; i<nAngles; i++) {
		printf(".");
		// writes to serial "+\n"
		sprintf(error_message,"\nSending request packet for the %d-th angle failure",i+1);
		if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,error_message) == EXIT_FAILURE) {
			free(sum_diff_array);
			return EXIT_FAILURE;
		}

		// reads sum and diff values
		result = read_until_terminator(ridSerial.serial_fd,id_array_size,sum_diff_array,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"\nReading sum-diff vector for %d-th angle failure\n",i+1);
			free(sum_diff_array);
			return EXIT_FAILURE;
		}
		// puts data in vectors
		for (j=0; j<nID; j++) {
			gsl_matrix_int_set(sumVectors,j,i,sum_diff_array[2*j]-CENTRE_RESCALE);
			gsl_matrix_int_set(diffVectors,j,i,sum_diff_array[2*j+1]-CENTRE_RESCALE);
		}
	}
	printf("completed\n");
	send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Detect packet send failure");
	free(sum_diff_array);
	return EXIT_SUCCESS;
}

