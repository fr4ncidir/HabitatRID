/*
 * serial.h
 * 
 * Copyright 2017 Francesco Antoniazzi <francesco.antoniazzi1991@gmail.com>
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
 */

/**
 * @brief Header file defining C functions for USB communication
 * @file serial.h
 * @author Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
 * @date 12 Apr 2017
 * @version 1
 *
 * This library contains some higher level function to write and read to an USB connected device.
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>

#define ERROR				-1
#define NO_PACKET			-2
#define NO_PARITY			0
#define EVEN_PARITY			1
#define ODD_PARITY			2
#define TERMINATOR_REACHED 	2
#define ONE_STOP			10
#define TWO_STOP			20

/**
 * @brief A simple structure for basic setups of serial port
 * You fill up the structure, and then give it to the open_serial function.
 * @see open_serial()
 */
typedef struct serial_options {
	int baudRate; /**< The baud rate can be <BR>
		B50,		50 baud <BR>
		B75,		75 baud <BR>
		B110,	110 baud <BR>
		B134,	134.5 baud<BR>
		B150,	150 baud<BR>
		B200,	200 baud<BR>
		B300,	300 baud<BR>
		B600,	600 baud<BR>
		B1200,	1200 baud<BR>
		B1800,	1800 baud<BR>
		B2400,	2400 baud<BR>
		B4800,	4800 baud<BR>
		B9600,	9600 baud<BR>
		B19200,	19200 baud<BR>
		B38400,	38400 baud<BR>
		B57600,	57,600 baud<BR>
		B76800,	76,800 baud<BR>
		B115200,	115,200 baud<BR>
	*/
	int dataBits; /**< Data bits can be<BR>
		CS5,	5 data bits<BR>
		CS6,	6 data bits<BR>
		CS7,	7 data bits<BR>
		CS8,	8 data bits<BR>
	*/
	int parityBit; /**< parity bit is one of the following<BR>
		NO_PARITY<BR>
		EVEN_PARITY<BR>
		ODD_PARITY<BR>
	*/
	int stopbits; /**< stop bit is one of the following<BR>
		ONE_STOP <BR>
		TWO_STOP <BR>
	*/
	int serial_fd; /**< will be set by open_serial to the current file descriptor*/
} SerialOptions;

/**
 * @brief Opens the serial port
 * @param name[]: might be something like "/dev/ttyACM0"
 * @param options: pointer to a SerialOptions setup for the serial port; the file descriptor will be set in the same struct;
 * @return ERROR or EXIT_SUCCESS
 */
int open_serial(const char name[],SerialOptions *options);

/**
 * @brief Reads exactly a certain number of bytes from the serial port
 * @param file_descriptor: is the identifier for the serial port
 * @param fixed_lenght: is the exact number of bytes to be read
 * @param buffer: is the place to store the bytes read
 * @return EXIT_FAILURE or EXIT_SUCCESS
 */
int read_nbyte(int file_descriptor,size_t fixed_lenght,void *buffer);

/**
 * @brief Reads a packet from serial port. The packet is identified with a single starting byte.
 * 
 * While reading the first time, we ignore all is coming before this packet and then we start read packet-by-packet. The packet has a fixed lenght.
 * @param file_descriptor: is the identifier for the serial port
 * @param fixed_length: is the exact number of bytes to be read
 * @param buffer: is the place to store the bytes read
 * @param start: is the starting byte of the packet
 * @return EXIT_FAILURE, EXIT_SUCCESS or NO_PACKET
 */
int read_nbyte_packet(int file_descriptor,size_t fixed_length,void *buffer,uint8_t start);

/**
 * @brief Reads the serial byte by byte, until it reaches max dimension of the buffer, or it reads the specified
 * terminator.
 * @param file_descriptor: is the identifier for the serial port
 * @param max_dim: is the maximum size of the storage buffer
 * @param buffer: is the storage buffer
 * @param terminator: usually called Schwarzenegger, it's the stop-read byte
 * @return ERROR or the number of bytes read, terminator included
 */
int read_until_terminator(int file_descriptor,size_t max_dim,void *buffer,uint8_t terminator);

/**
 * @brief Writes to a serial port previously opened a precise number of bytes.
 * @param file_descriptor: is the identifier for the serial port
 * @param length: is the length of the item to be sent out
 * @param buffer: is a pointer to the item to be sent out
 * @return EXIT_FAILURE or EXIT_SUCCESS
 */
int write_serial(int file_descriptor,size_t length,void *buffer);

/**
 * @brief Sends an array of characters to a serial port. If it fails, it prints an error message
 * @param serial_descriptor: the descriptor of the serial port
 * @param packet: the array containing the packet
 * @param dim: usually is packet_dimension_in_bytes*sizeof(uint8_t)
 * @param error_msg: a string containing an error message
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int send_packet(const int serial_descriptor,const uint8_t packet[],const size_t dim,const char error_msg[]);

/**
 * @brief A debugging function useful as printf for uint8_t arrays
 * @param vector: the uint8_t array
 * @param dim: the dimension of the array
 */
void printUnsignedArray(FILE *outstream,uint8_t *vector,int dim);

#endif /* SERIAL_H_ */
