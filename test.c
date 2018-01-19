/*
 * test.c
 * 
 * Copyright 2018 Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
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
#include <ctype.h>
#include <string.h>
#include <glib.h>
#include "serial.h"

SerialOptions ridSerial;
uint8_t id_read_result[22];

int main(int argc, char **argv)
{
	uint8_t request_confirm[3] = {'x','x','\0'};
	int i,result;
	
	ridSerial.baudRate = B115200;
	ridSerial.dataBits = CS8;
	ridSerial.parityBit = NO_PARITY;
	ridSerial.stopbits = ONE_STOP;
	if (open_serial("/dev/ttyUSB0",&ridSerial) == ERROR) return EXIT_FAILURE;
	
	if (send_packet(ridSerial.serial_fd,"+\n",2,"Reset packet send failure") == EXIT_FAILURE) {
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	sleep(1); //ESSENZIALE
	g_message("Reset packet sent");
	
	if (send_packet(ridSerial.serial_fd,"<\n",2,"Request id packet send failure") == EXIT_FAILURE) {
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	g_message("Request packet sent");
	
	result = read_until_terminator(ridSerial.serial_fd,4,request_confirm,'\n');
	if (result == ERROR) g_critical("request confirm command read_until_terminator failure");
	g_message("Confirmation received");
	
	result = read_until_terminator(ridSerial.serial_fd,22,id_read_result,'\n');
	if (result == ERROR) {
		g_critical("id list command read_until_terminator failure");
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	for (i=0; i<result; i++) printf("%u ",request_confirm[i]);
	printf("\n");
	
		result = read_until_terminator(ridSerial.serial_fd,22,id_read_result,'\n');
	if (result == ERROR) {
		g_critical("id list command read_until_terminator failure");
		close(ridSerial.serial_fd);
		return EXIT_FAILURE;
	}
	for (i=0; i<result; i++) printf("%u ",request_confirm[i]);
	printf("\n");
	
	close(ridSerial.serial_fd);
	return 0;
}

