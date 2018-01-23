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
gcc -Wall -I/usr/local/include ridMain4.c RIDLib.c serial.c ../SEPA-C/sepa_producer.c ../SEPA-C/sepa_utilities.c ../SEPA-C/jsmn.c -o ridReader -lgsl -lgslcblas -lm -lcurl `pkg-config --cflags --libs glib-2.0`
* G_MESSAGES_DEBUG=all
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "serial.h"
#include "RIDLib.h"


SerialOptions ridSerial;

int ridExecution(const char * usb_address,int iterations);
int readAllAngles(int nAngles,size_t id_array_size);
void printUsage(const char * error_message);

int main(int argc, char ** argv) {
	int execution_result=0;
	
	g_log_set_handler(NULL, G_LOG_LEVEL_MASK, g_log_default_handler,NULL);
	
	execution_result = ridExecution("/dev/ttyUSB0",1);
	if (execution_result==EXIT_FAILURE) g_critical("Execution ended with code EXIT_FAILURE.\n");
	else g_message("Execution ended with code EXIT_SUCCESS.\n");
	return execution_result;
}

int ridExecution(const char * usb_address,int iterations) {
	char response[70];
	int i;
	char dato;
	
	// serial opening
	ridSerial.baudRate = B115200;
	ridSerial.dataBits = CS8;
	ridSerial.parityBit = NO_PARITY;
	ridSerial.stopbits = ONE_STOP;
	if (open_serial(usb_address,&ridSerial) == ERROR) return EXIT_FAILURE;
	// serial opening end
		
	dato = '+';
	write_serial(ridSerial.serial_fd,1,(void*) &dato);
	sleep(1);
	printf("sono qui\n");
	dato = '<';
	write_serial(ridSerial.serial_fd,1,(void*) &dato);
	read_nbyte(ridSerial.serial_fd,2,(void*) response);
	printUnsignedArray((uint8_t*)response,2);
	printf("\n");
	read_nbyte(ridSerial.serial_fd,4,(void*) response);
	printUnsignedArray((uint8_t*)response,4);
	printf("\n");
	
	for (i=0; i<40; i++) {
		write_serial(ridSerial.serial_fd,1,(void*) &dato);
		read_nbyte(ridSerial.serial_fd,3,(void*) response);
		printUnsignedArray((uint8_t*)response,3);
		printf("\n");
	}
	
	dato = '>';
	write_serial(ridSerial.serial_fd,1,(void*) &dato);
	read_nbyte(ridSerial.serial_fd,3,(void*) response);
	printUnsignedArray((uint8_t*)response,3);
	printf("\n");
	close(ridSerial.serial_fd);
	return EXIT_SUCCESS;
}

int readAllAngles(int nAngles,size_t id_array_size) {
	
	return EXIT_SUCCESS;
}

void printUsage(const char * error_message) {
	if (error_message!=NULL) g_critical("%s",error_message);
	execlp("cat","cat","./manpage.txt",NULL);
	perror("Couldn't print manpage - ");
	exit(EXIT_FAILURE);
}
