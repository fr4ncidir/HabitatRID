/*
 * main.c
 *
 *  Created on: 18 ott 2016
 *      Author: francesco
 */


#include <stdio.h>
#include <stdlib.h>
#include "serial.h"

#define MESSAGE_SIZE	15

int main(int argc, char ** argv) {
	int serial_descriptor;
	int success,end=0;
	SerialOptions mySerial;
	char message[50];

	mySerial.baudRate = B9600;
	mySerial.dataBits = CS8;
	mySerial.parityBit = NO_PARITY;
	serial_descriptor = open_serial("/dev/ttyACM0",mySerial);
	if (serial_descriptor == ERROR) return EXIT_FAILURE;

	do {
		//success = read_serial_packet(serial_descriptor,MESSAGE_SIZE,message,'H');
		if (success == EXIT_FAILURE) break;
		else {
			if (success == NO_PACKET) {
				printf("No starting bit found!\n");
				return EXIT_FAILURE;
			}
			end++;
			message[MESSAGE_SIZE]='\0';
			printf("%d) Message received: %s\n",end-1,message);
		}
	} while (end<100);

	close(serial_descriptor);
	return EXIT_SUCCESS;
}
