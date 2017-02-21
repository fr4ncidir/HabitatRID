/*
 * main.c
 * RFID protocol C simulation: equivalent to RID_Elaborazione
 *
 *  Created on: 18 ott 2016
 *      Author: Francesco Antoniazzi
 * 	francesco.antoniazzi@unibo.it
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

int continuousRead,ridSerial_descriptor;
intVector * rowOfSums;
intVector * rowOfDiffs;
uint8_t * sum_diff_array;
intVector * idVector;
intMatrix * sumVectors;
intMatrix * diffVectors;
uint8_t nID;

uint8_t request_packet[STD_PACKET_STRING_DIM] = {REQUEST_COMMAND,SCHWARZENEGGER,'\0'};
uint8_t reset_packet[STD_PACKET_STRING_DIM] = {RESET_COMMAND,SCHWARZENEGGER,'\0'};
uint8_t detect_packet[STD_PACKET_STRING_DIM] = {DETECT_COMMAND,SCHWARZENEGGER,'\0'};
uint8_t request_confirm[STD_PACKET_STRING_DIM] = {'x','x','\0'};
size_t std_packet_size = STD_PACKET_DIM*sizeof(uint8_t);
size_t id_array_size;

void printUsage(FILE * outchannel);
int readInterval();
void safeExit();
int readAllAngles(int nAngles);

void interruptHandler(int signalCode) {
	signal(signalCode,SIG_IGN);
	printf("Info: Caught Ctrl-C: stopping reading RID\n");
	continuousRead = FALSE;
	signal(signalCode,SIG_DFL);
}

int main(int argc, char ** argv) {
	uint8_t * id_array;
	char logFileName[50];
	int result,i,j,textLogFlag=FALSE,slowMode=FALSE,bypassSerial=FALSE;
	int continuousTiming,continuousSlowFlag = FALSE;
	SerialOptions ridSerial;

	continuousRead = FALSE;

	switch (argc) {
	case 1: // only the command
		printf("Info: One read, stored in bin file; one calculation of location\n");
		break;
	case 2: // command + one argument
		switch (strlen(argv[1])) {
		case 2:
			if (!strcmp(argv[1],"-l")) {
				printf("Info: Results will be stored in txt and bin log file\n");
				textLogFlag = TRUE;
				break;
			}
			if (!strcmp(argv[1],"-r")) {
				printf("Info: Read only mode: read from bin log file\n");
				bypassSerial = TRUE;
				break;
			}
			if (!strcmp(argv[1],"-c")) {
				printf("Info: Continuous read and location decode\n");
				continuousRead = TRUE;
				continuousTiming = readInterval();
				break;
			}
			if (!strcmp(argv[1],"-s")) {
				printf("Info: Slow mode: the read is not immediately processed, only written on a binary log file\n");
				slowMode = TRUE;
				break;
			}
			printUsage(stderr);
			return EXIT_FAILURE;
		case 3:
			// only -lc,-ls,-cs are allowed
			if ((strchr(argv[1],'l')!=NULL) && (strchr(argv[1],'c')!=NULL)) {
				printf("Info: Continuous read and location decode, storing data on bin and txt log file\n");
				textLogFlag = TRUE;
				continuousRead = TRUE;
				continuousTiming = readInterval();
				break;
			}
			if ((strchr(argv[1],'l')!=NULL) && (strchr(argv[1],'s')!=NULL)) {
				printf("Info: Slow mode: every read is not immediately processed, written on bin and txt log file\n");
				textLogFlag = TRUE;
				slowMode = TRUE;
				break;
			}
			if ((strchr(argv[1],'s')!=NULL) && (strchr(argv[1],'c')!=NULL)) {
				printf("Info: Continuous read of data in slow mode: data is stored in bin file; location is not calculated");
				slowMode = TRUE;
				continuousRead = TRUE;
				continuousTiming = readInterval();
				break;
			}
			printUsage(stderr);
			return EXIT_FAILURE;
		case 4:
			// only -lcs is allowed
			if ((strchr(argv[1],'l')!=NULL) && (strchr(argv[1],'c')!=NULL) && (strchr(argv[1],'s')!=NULL)) {
				printf("Info: Continuous read of data in slow mode, storing data on bin and txt log file; location is not calculated\n");
				slowMode = TRUE;
				continuousRead = TRUE;
				textLogFlag = TRUE;
				continuousTiming = readInterval();
				break;
			}
			printUsage(stderr);
			return EXIT_FAILURE;
		default:
			printUsage(stderr);
			return EXIT_FAILURE;
		}
		break;
	default:
		printUsage(stdout);
		return EXIT_FAILURE;
	}

	if (!bypassSerial) {
		// serial opening
		ridSerial.baudRate = B115200;
		ridSerial.dataBits = CS8;
		ridSerial.parityBit = NO_PARITY;
		ridSerial.stopbits = ONE_STOP;
		// ridSerial_descriptor = open_serial("/dev/ttyUSB0",ridSerial);
		ridSerial_descriptor = open_serial("/dev/ttyUSB0",ridSerial);
		if (ridSerial_descriptor == ERROR) return EXIT_FAILURE;

		// writes to serial "+\n"
		if (send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure") == EXIT_FAILURE) {
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Info: Reset packet sent\n");
		sleep(1);

		// writes to serial "<\n"
		if (send_packet(ridSerial_descriptor,request_packet,std_packet_size,"Request id packet send failure") == EXIT_FAILURE) {
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Info: Request packet sent\n");

		// reads from serial "<\n"
		result = read_until_terminator(ridSerial_descriptor,std_packet_size,request_confirm,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"request confirm command read_until_terminator failure\n");
			send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		if (strcmp((char*) request_packet,(char*) request_confirm)) {
			fprintf(stderr,"Received unexpected %s instead of %s\n",(char*) request_confirm,(char*) request_packet);
			send_packet(ridSerial_descriptor,reset_packet,std_packet_size,"Reset packet send failure");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Info: Confirmation received\n");

		// reads from serial the number of ids
		result = read_nbyte(ridSerial_descriptor,sizeof(uint8_t),&nID);
		if (result == EXIT_FAILURE) {
			fprintf(stderr,"nID number read_nbyte failure\n");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		if (nID==WRONG_NID) {
			fprintf(stderr,"nId == 255 error\n");
			close(ridSerial_descriptor);
			return EXIT_FAILURE;
		}
		printf("Info: Number of id received: %d\n",nID);

		// reads from serial the ids
		id_array_size = (2*nID+1)*sizeof(uint8_t);
		id_array = (uint8_t*) malloc(id_array_size);
		result = read_until_terminator(ridSerial_descriptor,id_array_size,id_array,SCHWARZENEGGER);
		if (result == ERROR) {
			fprintf(stderr,"id list command read_until_terminator failure\n");
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
		printf("Info: Id list received\n");

		// retrieves sum and diff vectors
		sum_diff_array = (uint8_t*) malloc(id_array_size);
		sumVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);
		diffVectors = gsl_matrix_int_alloc(nID,ANGLE_ITERATIONS);

		if (continuousRead) {
			printf("Info: use Ctrl-c to stop reading\n");
			signal(SIGINT,interruptHandler);
			if (!slowMode) {
				continuousSlowFlag = TRUE;
				rowOfSums = gsl_vector_int_alloc(ANGLE_ITERATIONS);
				rowOfDiffs = gsl_vector_int_alloc(ANGLE_ITERATIONS);
			}
		}
		printf("\n");
		do {
			result = readAllAngles(ANGLE_ITERATIONS);
			if (result == EXIT_FAILURE) break;

			// log files
			if (textLogFlag) log_file_txt(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,0);
			log_file_bin(idVector,sumVectors,diffVectors,nID,ANGLE_ITERATIONS,logFileName);

			if ((!slowMode) && continuousRead) {
				for (j=0; j<nID; j++) {
					gsl_matrix_int_get_row(rowOfSums,sumVectors,j);
					gsl_matrix_int_get_row(rowOfDiffs,diffVectors,j);
					printLocation(locateFromData(rowOfSums,rowOfDiffs,ANGLE_ITERATIONS));
				}
			}
			if (continuousRead) usleep(continuousTiming);
			else {
				if (!slowMode) printLocation(locateFromFile(logFileName));
			}
		} while (continuousRead);
		if (continuousSlowFlag) {
			gsl_vector_int_free(rowOfSums);
			gsl_vector_int_free(rowOfDiffs);
		}
		safeExit();
	}
	else {
		// cmd -r passes here
		printf("Info: Please insert the name of the binary logfile: \n-> ");
		scanf("%s",logFileName);
		printLocation(locateFromFile(logFileName));
	}

	printf("Info: Main has successfully terminated! Good game!\n");
	return EXIT_SUCCESS;
}

void printUsage(FILE * outchannel) {
	fprintf(outchannel,"USAGE:\n");
	fprintf(outchannel,"-l\tstores results into txt file\n");
	fprintf(outchannel,"-c\trepeats execution until Ctrl-C\n");
	fprintf(outchannel,"-r\treads from binary file data\n");
	fprintf(outchannel,"-s\tonly stores results into binary file\n");
	fprintf(outchannel,"\nYou can also combine l,s,c options\n");
}

int readInterval() {
	uint32_t microseconds;
	printf("Please insert ms between two reads: ");
	scanf("%u",&microseconds);
	return microseconds*1000;
}

void safeExit() {
	free(sum_diff_array);
	gsl_matrix_int_free(sumVectors);
	gsl_matrix_int_free(diffVectors);
	gsl_vector_int_free(idVector);
	close(ridSerial_descriptor);
}

int readAllAngles(int nAngles) {
	char error_message[50];
	int i,j,result;
	printf("Info: Angle iterations");
	for (i=0; i<nAngles; i++) {
		printf(".");
		// writes to serial "<\n"
		sprintf(error_message,"\nSending request packet for the %d-th angle failure",i+1);
		if (send_packet(ridSerial_descriptor,request_packet,std_packet_size,error_message) == EXIT_FAILURE) {
			return EXIT_FAILURE;
		}

		// reads sum and diff values
		result = read_until_terminator(ridSerial_descriptor,id_array_size,sum_diff_array,SCHWARZENEGGER);
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
	send_packet(ridSerial_descriptor,detect_packet,std_packet_size,"Detect packet send failure");
	return EXIT_SUCCESS;
}
