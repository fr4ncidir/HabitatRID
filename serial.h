/*
 * serial.h
 *
 *  Created on: 18 ott 2016
 *      Author: Francesco Antoniazzi
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <inttypes.h>

//#define VERBOSE

#define ERROR			-1
#define NO_PACKET		-2
#define NO_PARITY		0
#define EVEN_PARITY		1
#define ODD_PARITY		2

/*
 * baudRate is one of the following:
	B50		50 baud
	B75		75 baud
	B110	110 baud
	B134	134.5 baud
	B150	150 baud
	B200	200 baud
	B300	300 baud
	B600	600 baud
	B1200	1200 baud
	B1800	1800 baud
	B2400	2400 baud
	B4800	4800 baud
	B9600	9600 baud
	B19200	19200 baud
	B38400	38400 baud
	B57600	57,600 baud
	B76800	76,800 baud
	B115200	115,200 baud
 * data bits is one of the following
	CS5	5 data bits
	CS6	6 data bits
	CS7	7 data bits
	CS8	8 data bits
 * parity bit is one of the following
	NO_PARITY
	EVEN_PARITY
	ODD_PARITY
 */
typedef struct serial_options {
	int baudRate;
	int dataBits;
	int parityBit;
} SerialOptions;

int open_serial(const char name[],SerialOptions options);
int fixed_read(int file_descriptor,size_t fixed_lenght,void * buffer);
int read_serial_packet(int file_descriptor,size_t fixed_lenght,void * buffer,uint8_t start);
int write_serial(int file_descriptor,size_t lenght,void * buffer);

#endif /* SERIAL_H_ */
