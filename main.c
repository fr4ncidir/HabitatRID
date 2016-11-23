/*
 * main.c
 * RFID protocol C simulation: equivalent to RID_Elaborazione
 *
 *  Created on: 18 ott 2016
 *      Author: Francesco Antoniazzi
 *
 *  (1) $ cmd
 *  	checks the serial and performs 40 angle reads. Writes results on a binary file and then retrieves the location
 *
 *  (2) $ cmd -l
 *  	same as (1) but saves data also in txt file
 *
 *  (3) $ cmd -r
 *  	asks for a file name, and retrieves the location. The file must be binary
 *
 *  (4) $ cmd -s
 *  	same as (1), but does not retrieve location
 *
 *  (5) $ cmd -sl
 *  	same as (4) and (2)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
#include "RIDLib.h"

#define RESET_COMMAND				'+'
#define REQUEST_COMMAND				'<'
#define DETECT_COMMAND				'>'
#define SCHWARZENEGGER				'\n' // line feed packet terminator
#define STD_PACKET_DIM				2
#define STD_PACKET_STRING_DIM		3
#define WRONG_NID					255
#define CENTRE_RESCALE				256
#define TRUE						1
#define FALSE						0

int main(int argc, char ** argv) {
	int ridSerial_descriptor;
	uint8_t request_packet[STD_PACKET_STRING_DIM] = {REQUEST_COMMAND,SCHWARZENEGGER,'\0'};
	uint8_t reset_packet[STD_PACKET_STRING_DIM] = {RESET_COMMAND,SCHWARZENEGGER,'\0'};
	uint8_t detect_packet[STD_PACKET_STRING_DIM] = {DETECT_COMMAND,SCHWARZENEGGER,'\0'};
	uint8_t request_confirm[STD_PACKET_STRING_DIM] = {'x','x','\0'};
	uint8_t nID;
	uint8_t * id_array;
	uint8_t * sum_diff_array;
	size_t std_packet_size = STD_PACKET_DIM*sizeof(uint8_t);
	size_t id_array_size;

	char error_message[50],logFileName[50];
	int result,i,j,textLogFlag=FALSE,slowMode=FALSE,bypassSerial=FALSE;
	SerialOptions ridSerial;

	intVector * idVector;
	intMatrix * sumVectors;
	intMatrix * diffVectors;
	coord location;

	switch (argc) {
	case 1:
		printf("Results will be saved only in binary log file\n");
		break;
	case 2:
		// -l
		if (!strcmp(argv[1],"-l")) {
			printf("Results will be saved in txt and bin log file\n");
			textLogFlag = TRUE;
			break;
		}
		else {
			// cmd -r
			// only read from log file
			if (!strcmp(argv[1],"-r")) {
				printf("Read only mode\n");
				bypassSerial = TRUE;
				break;
			}

			slowMode = TRUE;
			// cmd -s
			// binary log file. Every read from RID is then calculated and printed in stdout
			if (!strcmp(argv[1],"-s")) {
				printf("Slow mode: the read is not immediately processed, only written on a binary log file\n");
				break;
			}
			// cmd -ls or cmd -sl
			// binary and text log files are written. Also, every read from RID is then calculated and printed in stdout
			if ((!strcmp(argv[1],"-sl")) || (!strcmp(argv[1],"-ls"))) {
				printf("Slow mode: every read is not immediately processed, written on bin and txt log file\n");
				textLogFlag = TRUE;
				break;
			}
		}
		return EXIT_FAILURE;
	default:
		printf("Usage: \n-s: reads from RID and calculates one by one\n-l: saves on txt logfile.\n-sl both\n-r read only\n");
		return EXIT_SUCCESS;
	}

	if (!bypassSerial) {
		// serial opening
		ridSerial.baudRate = B115200;
		ridSerial.dataBits = CS8;
		ridSerial.parityBit = NO_PARITY;
		ridSerial.stopbits = ONE_STOP;
		ridSerial_descriptor = open_serial("/dev/ttyUSB0",ridSerial);
		if (ridSerial_descriptor == ERROR) return EXIT_FAILURE;

		// writes to serial "+\n"
		if (send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure") == EXIT_FAILURE) {
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Reset packet sent\n");
		sleep(1);

		// writes to serial "<\n"
		if (send_packet(ridSerial_descriptor,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Request packet sent\n");

		// reads from serial "<\n"
		result = read_until_terminator(ridSerial_descriptor,std_packet_size,request_confirm,SCHWARZENEGGER);
		if (result == ERROR) {
			printf("request confirm command read_until_terminator failure\n");
			send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		if (strcmp((char*) request_packet,(char*) request_confirm)) {
			printf("Received unexpected %s instead of %s\n",(char*) request_confirm,(char*) request_packet);
			send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Confirmation received\n");

		// reads from serial the number of ids
		result = read_nbyte(ridSerial_descriptor,sizeof(uint8_t),&nID);
		if (result == EXIT_FAILURE) {
			printf("nID number read_nbyte failure\n");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		if (nID==WRONG_NID) {
			printf("nId == 255 error\n");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Number of id received: %d\n",nID);

		// reads from serial the ids
		id_array_size = (2*nID+1)*sizeof(uint8_t);
		id_array = (uint8_t*) malloc(id_array_size);
		result = read_until_terminator(ridSerial_descriptor,id_array_size,id_array,SCHWARZENEGGER);
		if (result == ERROR) {
			printf("id list command read_until_terminator failure\n");
			free(id_array);
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		idVector = gsl_vector_int_alloc(nID);
		j=0;
		for (i=0; i<2*nID; i=i+2) {
			gsl_vector_int_set(idVector,j,id_array[i]);
			j++;
		}
		free(id_array);
		printf("Id list received\n");

		// retrieves sum and diff vectors
		sum_diff_array = (uint8_t*) malloc(id_array_size);
		sumVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);
		diffVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);
		printf("Angle iterations");
		for (i=1; i<=ANGLE_ITERATIONS; i++) {
			printf(".");
			sprintf(error_message,"\nSending request packet for the %d-th angle failure",i);
			if (send_packet(ridSerial_descriptor,request_packet,std_packet_size,error_message) == EXIT_FAILURE) {
				free(sum_diff_array);
				gsl_matrix_int_free(sumVectors);
				gsl_matrix_int_free(diffVectors);
				gsl_vector_int_free(idVector);
				close(ridSerial_descriptor);
				return EXIT_FAILURE;
			}
			result = read_until_terminator(ridSerial_descriptor,id_array_size,sum_diff_array,SCHWARZENEGGER);
			if (result == ERROR) {
				printf("\nReading sum-diff vector for %d-th angle failure\n",i);
				free(sum_diff_array);
				gsl_matrix_int_free(sumVectors);
				gsl_matrix_int_free(diffVectors);
				gsl_vector_int_free(idVector);
				close(ridSerial_descriptor);
				return EXIT_FAILURE;
			}
			for (j=0; j<nID; j++) {
				gsl_matrix_int_set(sumVectors,j,i-1,sum_diff_array[2*j]-CENTRE_RESCALE);
				gsl_matrix_int_set(diffVectors,j,i-1,sum_diff_array[2*j+1]-CENTRE_RESCALE);
			}
		}
		free(sum_diff_array);
		printf("completed\n");

		// log files
		if (textLogFlag) log_file_txt(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,0);
		log_file_bin(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,logFileName);
		printf("Log file(s) written\n");

		send_packet(ridSerial_descriptor,detect_packet,std_packet_size,"Detect packet send failure");
	}
	else {
		// cmd -r passes here
		printf("Please insert the name of the binary logfile: \n-> ");
		scanf("%s",logFileName);
	}
	if (!slowMode) {
		location = localization(logFileName);
		printf("\nLocation calculated: \n(x,y)=(%lf,%lf)\n",location.x,location.y);
	}

	gsl_matrix_int_free(sumVectors);
	gsl_matrix_int_free(diffVectors);
	gsl_vector_int_free(idVector);
	close(ridSerial_descriptor);

	printf("Main has successfully terminated! Good game!\n");
	return EXIT_SUCCESS;
}
