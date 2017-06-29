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
 * 
 */

// TODO dati della stanza in un file JSON
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "serial.h"
#include "RIDLib.h"

int continuousRead = FALSE;
intVector *idVector;
intMatrix *sumVectors;
intMatrix *diffVectors;
uint8_t nID;
SerialOptions ridSerial;

extern uint8_t request_packet[STD_PACKET_STRING_DIM];
extern uint8_t reset_packet[STD_PACKET_STRING_DIM];
extern uint8_t detect_packet[STD_PACKET_STRING_DIM];
extern size_t std_packet_size;

uint8_t param_check(const char * params);
int ridExecution(uint8_t execution_code,const char * usb_address,const char * SEPA_address,int iterations);
int third_param_check(const char * third_param);
int readAllAngles(int nAngles,size_t id_array_size);
void free_arrays(int count,...);

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	printf("Info: Caught Ctrl-C: stopping reading RID\n");
	continuousRead = FALSE;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char ** argv) {
	uint8_t execution_code = 0;
	int cmdlineOpt,force_missingOption=FALSE,my_optopt;
	int iterationNumber=0,execution_result,i;
	char *http_sepa_address=NULL,*usbportAddress;
	
	usbportAddress = strdup("/dev/ttyUSB0");
	if (usbportAddress==NULL) {
		fprintf(stderr,"malloc error in usbportAddress.\n");
		return EXIT_FAILURE;
	}
	
	opterr = 0;
	if ((argc==1) || ((argc==2) && (!strcmp(argv[1],"help")))) {
		free(usbportAddress);
		printUsage(NULL);
		return EXIT_SUCCESS;
	}
	while ((cmdlineOpt = getopt(argc, argv, "-rlbn:u:p:"))!=-1) {
		if (((cmdlineOpt=='n') || (cmdlineOpt=='u') || (cmdlineOpt=='p')) && (optarg[0]=='-')) {
			force_missingOption = TRUE;
			my_optopt = cmdlineOpt;
			cmdlineOpt = '?';
		}
		switch (cmdlineOpt) {
			case 'r':
				execution_code = execution_code | 0x01;
				break;
			case 'l':
				execution_code = execution_code | 0x02;
				break;
			case 'b':
				execution_code = execution_code | 0x04;
				break;
			case 'n':
				for (i=0; i<strlen(optarg); i++) {
					// optarg must be an integer number
					if (!isdigit(optarg[i])) {
						fprintf(stderr,"%s is not an integer: -n argument must be an integer.\n",optarg);
						free_arrays(2,usbportAddress,http_sepa_address);
						printUsage(NULL);
						return EXIT_FAILURE;
					}
				}
				sscanf(optarg,"%d",&iterationNumber);
				printf("Requested %d iterations.\n",iterationNumber);
				break;
			case 'u':
				http_sepa_address = strdup(optarg);
				if (http_sepa_address==NULL) {
					fprintf(stderr,"malloc error in http SEPA Address.\n");
					free(usbportAddress);
					return EXIT_FAILURE;
				}
				printf("Sepa address: %s\n",http_sepa_address);
				break;
			case 'p':
				free(usbportAddress);
				usbportAddress = strdup(optarg);
				if (usbportAddress==NULL) {
					fprintf(stderr,"malloc error in usbportAddress.\n");
					free_arrays(1,http_sepa_address);
					return EXIT_FAILURE;
				}
				printf("USB port address: %s\n",usbportAddress);
				break;
			default: // case '?'
				if (!force_missingOption) my_optopt = optopt;
				if ((my_optopt=='n') || (my_optopt=='u') || (my_optopt=='p')) fprintf(stderr,"Option -%c requires an argument.\n",my_optopt);
				else {
					if (isprint(my_optopt)) fprintf(stderr,"Unknown option -%c.\n",my_optopt);
					else fprintf(stderr,"Unknown option character \\x%x.\n",my_optopt);
				}
				free_arrays(2,usbportAddress,http_sepa_address);
				printUsage("Wrong syntax!\n");
				return EXIT_FAILURE;
		}
		if (((execution_code & 0x05)==0x05) || (!(execution_code & 0x07))){
			free_arrays(2,usbportAddress,http_sepa_address);
			printUsage("Wrong syntax! [-r and -b are not compatible]\n");
			return EXIT_FAILURE;
		}
	}
	execution_result = ridExecution(execution_code,usbportAddress,http_sepa_address,iterationNumber);
	if (execution_result==EXIT_FAILURE) printf("Execution ended with code EXIT_FAILURE.\n");
	else printf("Execution ended with code EXIT_SUCCESS.\n");
	free_arrays(2,usbportAddress,http_sepa_address);
	return execution_result;
}

int ridExecution(uint8_t code,const char * usb_address,const char * SEPA_address,int iterations) {
	// functioning core
	coord *locations;
	coord last_location;
	int locationDim,i,j,result,protocol_error=0;
	char logFileName[50];
	FILE *customOutput = stdout;
	uint8_t *id_array;
	intVector *rowOfSums,*rowOfDiffs;
	size_t id_array_size;
	char *sparqlUpdate_unbounded=NULL,*sparqlUpdate_bounded=NULL;
	uint8_t request_confirm[STD_PACKET_STRING_DIM] = {'x','x','\0'};
	
	if (SEPA_address!=NULL) { // u
		sparqlUpdate_unbounded = strdup("PREFIX rdf:<http://www.w3.org/1999/02/22-rdf-syntax-ns#> PREFIX hbt:<http://www.unibo.it/Habitat#> DELETE {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateY ?oldY} INSERT {%s rdf:type hbt:ID. %s hbt:hasPosition %s. %s rdf:type hbt:Position. %s hbt:hasCoordinateX \"%lf\". %s hbt:hasCoordinateY \"%lf\"} WHERE {{} UNION {OPTIONAL {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateX ?oldY}}}");
		sparqlUpdate_bounded = (char *) malloc(SEPA_UPDATE_BOUNDED*sizeof(char));
		if ((sparqlUpdate_unbounded==NULL) || (sparqlUpdate_bounded==NULL)){
			fprintf(stderr,"SPARQL malloc problem\n");
			return EXIT_FAILURE;
		}
	}
	
	if ((code & 0x01)==0x01) { // r
		printf("Info: Please insert the name of the binary logfile: \n-> ");
		scanf("%s",logFileName);
		locations = locateFromFile(logFileName,&locationDim);
		if (locations==NULL) {
			fprintf(stderr,"Failed in retrieving locations from %s\n",logFileName);
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			return EXIT_FAILURE;
		}
		if ((code & 0x02)==0x02) { // rl
			strcat(logFileName,".txt");
			printf("Location logged on %s\n",logFileName);
			customOutput = fopen(logFileName,"w");
			if (customOutput==NULL) {
				fprintf(stderr,"log on %s generated an error.\n",logFileName);
				free_arrays(3,locations,sparqlUpdate_unbounded,sparqlUpdate_bounded);
				return EXIT_FAILURE;
			}
		}
		for (i=0; i<GSL_MIN_INT(iterations,locationDim); i++) {
			printLocation(customOutput,locations[i]);
			sepaLocationUpdate(SEPA_address,locations[i],sparqlUpdate_unbounded);
		}
		free(locations);
		fclose(customOutput);
	}
	else {
		// serial opening
		ridSerial.baudRate = B115200;
		ridSerial.dataBits = CS8;
		ridSerial.parityBit = NO_PARITY;
		ridSerial.stopbits = ONE_STOP;
		if (open_serial(usb_address,&ridSerial) == ERROR) {
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			return EXIT_FAILURE;
		}
		// serial opening end
		
		// writes to serial "+\n": rid reset
		if (send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure") == EXIT_FAILURE) {
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Reset packet sent\n");
		sleep(1);
		// rid reset end
		
		// writes to serial "<\n": start transmission request
		if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
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
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Confirmation received\n");
		// read request confirm end
		
		// reads from serial the number of ids
		result = read_nbyte(ridSerial.serial_fd,sizeof(uint8_t),&nID);
		if ((result==EXIT_FAILURE) || (nID==WRONG_NID)) {
			fprintf(stderr,"nID number read_nbyte failure: result=%d, nID=%d\n",result,nID);
			free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
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
			free_arrays(3,id_array,sparqlUpdate_unbounded,sparqlUpdate_bounded);
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
		sumVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);
		diffVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);

		printf("Info: use Ctrl-c to stop reading\n\n");
		signal(SIGINT,interruptHandler);

		rowOfSums = gsl_vector_int_alloc(ANGLE_ITERATIONS);
		rowOfDiffs = gsl_vector_int_alloc(ANGLE_ITERATIONS);
		if (!iterations) continuousRead = TRUE;
		do {
			result = readAllAngles(ANGLE_ITERATIONS,id_array_size);
			if (result == EXIT_FAILURE) break;

			// log files
			if ((code & 0x02)==0x02) log_file_txt(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,0);
			if ((code & 0x04)==0x04) log_file_bin(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,logFileName);

			for (j=0; j<nID; j++) {
				gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
				gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
				last_location = locateFromData(rowOfSums,rowOfDiffs,ANGLE_ITERATIONS);
				last_location.id = gsl_vector_int_get(idVector,j);
				printLocation(stdout,last_location);
				sepaLocationUpdate(SEPA_address,last_location,sparqlUpdate_unbounded);
			}
			
			iterations--;
		} while ((continuousRead) || (iterations>0));
		gsl_vector_int_free(rowOfSums);
		gsl_vector_int_free(rowOfDiffs);
		gsl_matrix_int_free(sumVectors);
		gsl_matrix_int_free(diffVectors);
		gsl_vector_int_free(idVector);
		close(ridSerial.serial_fd);
	}
	
	free_arrays(2,sparqlUpdate_unbounded,sparqlUpdate_bounded);
	return EXIT_SUCCESS;
}

void free_arrays(int count,...) {
	va_list ap;
    int j;
    void *mypointer;

    va_start(ap, count);
    for (j=0; j<count; j++) {
		mypointer = va_arg(ap, void*);
		if (mypointer!=NULL) free(mypointer);
    }
    va_end(ap);
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
		// writes to serial "<\n"
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
	send_packet(ridSerial.serial_fd,detect_packet,std_packet_size,"Detect packet send failure");
	free(sum_diff_array);
	return EXIT_SUCCESS;
}
