/**
 * serial.h
 * @author Francesco Antoniazzi
 * @version 0.9
 * @date 20 oct 2016
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <inttypes.h>

#define VERBOSE

#define ERROR				-1
#define NO_PACKET			-2
#define NO_PARITY			0
#define EVEN_PARITY			1
#define ODD_PARITY			2
#define TERMINATOR_REACHED 	2
#define ONE_STOP			10
#define TWO_STOP			20

/**
 * SerialOptions: a simple struct for basic setups of serial port
 */
typedef struct serial_options {
	int baudRate; /**<
	The baud rate can be <BR>
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
	int dataBits; /**<
	Data bits can be<BR>
		CS5,	5 data bits<BR>
		CS6,	6 data bits<BR>
		CS7,	7 data bits<BR>
		CS8,	8 data bits<BR>
	*/
	int parityBit; /**<
	parity bit is one of the following<BR>
		NO_PARITY<BR>
		EVEN_PARITY<BR>
		ODD_PARITY<BR>
	*/
	int stopbits; /**<
	stop bit is one of the following<BR>
		ONE_STOP <BR>
		TWO_STOP <BR>
	*/
} SerialOptions;

/**
 * open_serial opens the serial port
 * @param name[] might be something like "/dev/ttyACM0"
 * @param options is a setup for the serial port
 * @return ERROR or the serial file descriptor
 */
int open_serial(const char name[],const SerialOptions options);

/**
 * fixed_read reads a certain number of bytes from the serial port
 * @param file_descriptor is the identifier for the serial port
 * @param fixed_lenght is the exact number of bytes to be read
 * @param buffer is the place to store the bytes read
 * @return EXIT_FAILURE or EXIT_SUCCESS
 */
int read_nbyte(int file_descriptor,size_t fixed_lenght,void * buffer);

/**
 * read_serial_packet reads a packet from serial port. The packet is identified with a starting byte. So,
 * while reading the first time, we ignore all is coming before this packet and then we start read packet-by-packet.
 * The packet has a fixed lenght.
 * @param file_descriptor is the identifier for the serial port
 * @param fixed_lenght is the exact number of bytes to be read
 * @param buffer is the place to store the bytes read
 * @param start is the starting byte of the packet
 * @return EXIT_FAILURE, EXIT_SUCCESS or NO_PACKET
 */
int read_nbyte_packet(int file_descriptor,size_t fixed_lenght,void * buffer,uint8_t start);

/**
 * reads the serial byte by byte, until it reaches max dimention of the buffer, or it reads the specified
 * terminator.
 * @param file_descriptor is the identifier for the serial port
 * @param max_dim is the maximum size of the storage buffer
 * @param buffer is the storage buffer
 * @param terminator is the stop-read byte
 * @return ERROR or the number of bytes read, terminator included
 */
int read_until_terminator(int file_descriptor,size_t max_dim,void * buffer,uint8_t terminator);

/**
 * write_serial writes to a serial port previously opened.
 * @param file_descriptor is the identifier for the serial port
 * @param length is the length of the item to be sent out
 * @param buffer is a pointer to the item to be sent out
 * @return EXIT_FAILURE or EXIT_SUCCESS
 */
int write_serial(int file_descriptor,size_t lenght,void * buffer);

#endif /* SERIAL_H_ */
