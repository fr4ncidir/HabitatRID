/*
 * ridMain.c
 * 
 * Copyright 2017 Francesco Work <francesco.antoniazzi@unibo.it>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
#include "RIDLib.h"
#include "producer.h"

#define TRUE						1
#define FALSE						0
#define COMMAND_NAME				argv[0]
#define EXECUTION_PARAMS			argv[1]
#define N_ITERATIONS_OR_USB			argv[2]
#define FOUR_PARAMS_USB				argv[3]
#define SEPA_ADDRESS				argv[4]
#define CUSTOM_USB					1
#define CUSTOM_ITERATIONS			0
#define CONTINUOUS_ITERATION		0
#define SEPA_UPDATE_UNBOUNDED		390
#define SEPA_UPDATE_BOUNDED			500

int continuousRead;
uint8_t *sum_diff_array;
intVector *idVector;
intMatrix *sumVectors;
intMatrix *diffVectors;
uint8_t nID;
SerialOptions ridSerial;
char *http_sepa_address;

extern uint8_t request_packet[STD_PACKET_STRING_DIM];
extern uint8_t reset_packet[STD_PACKET_STRING_DIM];
extern uint8_t detect_packet[STD_PACKET_STRING_DIM];
extern size_t std_packet_size;

uint8_t request_confirm[STD_PACKET_STRING_DIM] = {'x','x','\0'};

uint8_t param_check(const char * params);
int ridExecution(uint8_t execution_code,const char * usb_address,int iterations);
int third_param_check(const char * third_param);
int readAllAngles(int nAngles,size_t id_array_size);
void sepafree(int code,char * sparql_unbounded,char * sparql_bounded);
void sepaLocationUpdate(int code,coord location,const char * unbounded_sparql);

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	printf("Info: Caught Ctrl-C: stopping reading RID\n");
	continuousRead = FALSE;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char **argv) {
	uint8_t execution_code;
	char default_usbPort[15] = "/dev/ttyUSB0";
	int iterationNumber;
	
	continuousRead = FALSE;
	
	switch (argc) {
	case 1:
		printUsage(NULL);
		break;
	case 2:
		if (!strcmp(EXECUTION_PARAMS,"help")) printUsage(NULL);
		execution_code = param_check(EXECUTION_PARAMS);
		if (!execution_code) printUsage("Wrong parameters!\n");
		continuousRead = TRUE;
		return ridExecution(execution_code,default_usbPort,CONTINUOUS_ITERATION);
	case 3:
		execution_code = param_check(EXECUTION_PARAMS);
		if (!execution_code) printUsage("Wrong parameters!\n");
		if (third_param_check(N_ITERATIONS_OR_USB)==CUSTOM_ITERATIONS) {
			sscanf(N_ITERATIONS_OR_USB,"-n%d",&iterationNumber);
			return ridExecution(execution_code,default_usbPort,iterationNumber);
		}
		continuousRead = TRUE;
		return ridExecution(execution_code,N_ITERATIONS_OR_USB,CONTINUOUS_ITERATION);
	case 4:
		execution_code = param_check(EXECUTION_PARAMS);
		if (!execution_code) printUsage("Wrong parameters!\n");
		if (third_param_check(N_ITERATIONS_OR_USB)!=CUSTOM_ITERATIONS) printUsage("Wrong spelling of third param!\n");
		sscanf(N_ITERATIONS_OR_USB,"-n%d",&iterationNumber);
		return ridExecution(execution_code,FOUR_PARAMS_USB,iterationNumber);
	case 5:
		execution_code = param_check(EXECUTION_PARAMS);
		if (!execution_code) printUsage("Wrong parameters!\n");
		execution_code = execution_code | 0x08;
		if (third_param_check(N_ITERATIONS_OR_USB)!=CUSTOM_ITERATIONS) printUsage("Wrong spelling of third param!\n");
		sscanf(N_ITERATIONS_OR_USB,"-n%d",&iterationNumber);
		if (strstr(SEPA_ADDRESS,"-u")!=SEPA_ADDRESS) printUsage("Wrong SEPA address format!\n");
		http_sepa_address = (char *) malloc(strlen(SEPA_ADDRESS)*sizeof(char));
		sscanf(SEPA_ADDRESS,"-u%s",http_sepa_address);
		return ridExecution(execution_code,FOUR_PARAMS_USB,iterationNumber);
	default:
		printUsage("Wrong parameter number!\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

uint8_t param_check(const char * params) {
	uint8_t execution_code = 0;
	if (params[0]!='-') return 0;
	if (strchr(params,'r')!=NULL) execution_code = execution_code | 0x01;
	if (strchr(params,'l')!=NULL) execution_code = execution_code | 0x02;
	if (strchr(params,'b')!=NULL) execution_code = execution_code | 0x04;
	
	if ((execution_code & 0x05)==0x05) return 0; //r e b non possono essere contemporanei
	// printf("execution_code=%u\n",execution_code);
	return execution_code;
}

int ridExecution(uint8_t code,const char * usb_address,int iterations) {
	coord *locations;
	coord last_location;
	int locationDim;
	int i,j,result;
	char logFileName[50];
	FILE *customOutput = stdout;
	uint8_t *id_array;
	intVector *rowOfSums;
	intVector *rowOfDiffs;
	size_t id_array_size;
	char *sparqlUpdate_unbounded,*sparqlUpdate_bounded;
	
	if ((code & 0x08)==0x08) { // u
		sparqlUpdate_unbounded = (char *) malloc(SEPA_UPDATE_UNBOUNDED*sizeof(char));
		sparqlUpdate_bounded = (char *) malloc(SEPA_UPDATE_BOUNDED*sizeof(char));
		if ((sparqlUpdate_unbounded==NULL) || (sparqlUpdate_bounded==NULL)){
			fprintf(stderr,"SPARQL malloc problem\n");
			return EXIT_FAILURE;
		}
		strcpy(sparqlUpdate_unbounded,"PREFIX rdf:<http://www.w3.org/1999/02/22-rdf-syntax-ns#> PREFIX hbt:<http://www.unibo.it/Habitat#> DELETE {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateY ?oldY} INSERT {%s rdf:type hbt:ID. %d hbt:hasPosition %s. %s rdf:type hbt:Position. %s hbt:hasCoordinateX \"%lf\". %s hbt:hasCoordinateY \"%lf\"} WHERE {{} UNION {OPTIONAL {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateX ?oldY}}}");
	}
	
	if ((code & 0x01)==0x01) { // r
		printf("Info: Please insert the name of the binary logfile: \n-> ");
		scanf("%s",logFileName);
		locations = locateFromFile(logFileName,&locationDim);
		if (locations==NULL) {
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			return EXIT_FAILURE;
		}
		if ((code & 0x02)==0x02) { // rl
			strcat(logFileName,".txt");
			printf("Location logged on %s\n",logFileName);
			customOutput = fopen(logFileName,"w");
			if (customOutput==NULL) {
				fprintf(stderr,"log on %s generated an error.\n",logFileName);
				free(locations);
				sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
				return EXIT_FAILURE;
			}
		}
		for (i=0; i<GSL_MIN_INT(iterations,locationDim); i++) {
			printLocation(customOutput,locations[i]);
			sepaLocationUpdate(code,locations[i],sparqlUpdate_unbounded);
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
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			return EXIT_FAILURE;
		}
		// serial opening end
		
		// writes to serial "+\n": rid reset
		if (send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure") == EXIT_FAILURE) {
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Reset packet sent\n");
		sleep(1);
		// rid reset end
		
		// writes to serial "<\n": start transmission request
		if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Request packet sent\n");
		// end transmission request
		
		// reads from serial "<\n" request confirm and check
		result = read_until_terminator(ridSerial.serial_fd,std_packet_size,request_confirm,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"request confirm command read_until_terminator failure\n");
			send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure");
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		if (strcmp((char*) request_packet,(char*) request_confirm)) {
			fprintf(stderr,"Received unexpected %s instead of %s\n",(char*) request_confirm,(char*) request_packet);
			send_packet(ridSerial.serial_fd,reset_packet,std_packet_size,"Reset packet send failure");
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Confirmation received\n");
		// read request confirm end
		
		// reads from serial the number of ids
		result = read_nbyte(ridSerial.serial_fd,sizeof(uint8_t),&nID);
		if (result == EXIT_FAILURE) {
			fprintf(stderr,"nID number read_nbyte failure\n");
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		if (nID==WRONG_NID) {
			fprintf(stderr,"nId == 255 error\n");
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
			close(ridSerial.serial_fd);
			return EXIT_FAILURE;
		}
		printf("Info: Number of id received: %d\n",nID);
		// read number of id end
		
		// reads from serial the ids
		id_array_size = (2*nID+1)*sizeof(uint8_t);
		id_array = (uint8_t*) malloc(id_array_size);
		result = read_until_terminator(ridSerial.serial_fd,id_array_size,id_array,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"id list command read_until_terminator failure\n");
			free(id_array);
			sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
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
		sum_diff_array = (uint8_t*) malloc(id_array_size);
		sumVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);
		diffVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);

		printf("Info: use Ctrl-c to stop reading\n\n");
		signal(SIGINT,interruptHandler);

		rowOfSums = gsl_vector_int_alloc(ANGLE_ITERATIONS);
		rowOfDiffs = gsl_vector_int_alloc(ANGLE_ITERATIONS);
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
				sepaLocationUpdate(code,last_location,sparqlUpdate_unbounded);
			}
			
			iterations--;
		} while ((continuousRead) || (iterations>0));
		gsl_vector_int_free(rowOfSums);
		gsl_vector_int_free(rowOfDiffs);
		gsl_matrix_int_free(sumVectors);
		gsl_matrix_int_free(diffVectors);
		gsl_vector_int_free(idVector);
		free(sum_diff_array);
	}
	sepafree(code,sparqlUpdate_unbounded,sparqlUpdate_bounded);
	close(ridSerial.serial_fd);
	return EXIT_SUCCESS;
}

int third_param_check(const char * third_param) {
	if (strstr(third_param,"-n")==third_param) return CUSTOM_ITERATIONS;
	return CUSTOM_USB;
}

void sepafree(int code,char * sparql_unbounded,char * sparql_bounded) {
	if ((code & 0x08)==0x08) {
		free(http_sepa_address);
		free(sparql_unbounded);
		free(sparql_bounded);
	}
}

void sepaLocationUpdate(int code,coord location,const char * unbounded_sparql) {
	char posUid[10];
	char ridUid[10];
	char bounded_sparql[SEPA_UPDATE_BOUNDED];
	if ((code & 0x08)==0x08) {
		sprintf(ridUid,"rid%d",location.id);
		sprintf(posUid,"pos%d",location.id);
		sprintf(bounded_sparql,unbounded_sparql, 
			posUid,posUid,				//DELETE {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateY ?oldY} 
			ridUid,ridUid,posUid, 		//INSERT {%s rdf:type hbt:ID. %s hbt:hasPosition %s.
			posUid,posUid,location.x,	//        %s rdf:type hbt:Position. %s hbt:hasCoordinateX \"%lf\".
			posUid,location.y,			//		  %s hbt:hasCoordinateY \"%lf\"}
			posUid,posUid);				//WHERE {{} UNION {OPTIONAL {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateX ?oldY}}}
		kpProduce(bounded_sparql,http_sepa_address);
	}
}

int readAllAngles(int nAngles,size_t id_array_size) {
	char error_message[50];
	int i,j,result;
	printf("Info: Angle iterations");
	for (i=0; i<nAngles; i++) {
		printf(".");
		// writes to serial "<\n"
		sprintf(error_message,"\nSending request packet for the %d-th angle failure",i+1);
		if (send_packet(ridSerial.serial_fd,request_packet,std_packet_size,error_message) == EXIT_FAILURE) {
			return EXIT_FAILURE;
		}

		// reads sum and diff values
		result = read_until_terminator(ridSerial.serial_fd,id_array_size,sum_diff_array,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"\nReading sum-diff vector for %d-th angle failure\n",i+1);
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
	return EXIT_SUCCESS;
}
