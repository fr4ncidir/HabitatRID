/*
 * serial.c
 *
 *  Created on: 18 ott 2016
 *      Author: Francesco Antoniazzi
 */
#include "serial.h"

int open_serial(const char name[],SerialOptions options) {
	int file_descriptor;
	char message[50];
	struct termios opt;

	/*
	 * O_RDWR means that we want to open the serial port in read and write
	 * O_NOCTTY means that it's the user the controller of the terminal, not the serial port itself
	 * O_NDLEAY the open function is not blocking
	 */
	file_descriptor = open(name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (file_descriptor == ERROR) {
		sprintf(message,"open_serial: Unable to open %s -",name);
		perror(message);
		return EXIT_FAILURE;
	}
	else {
		/*
		 * clears possible spurious flags
		 */
		fcntl(file_descriptor, F_SETFL, 0);

		/*
		 * Gets the options for the current file descriptor
		 * Set the baud rate
		 * Set the correct flags
		 */
		tcgetattr(file_descriptor, &opt);
		cfsetispeed(&opt,options.baudRate);
		cfsetospeed(&opt,options.baudRate);
		opt.c_cflag &= ~CSIZE;

		switch (options.parityBit) {
		case NO_PARITY:
			opt.c_cflag &= ~PARENB;
			break;
		case EVEN_PARITY:
			opt.c_cflag |= PARENB;
			opt.c_cflag &= ~PARODD;
			break;
		case ODD_PARITY:
			opt.c_cflag |= PARENB;
			opt.c_cflag |= PARODD;
			break;
		default: // misconfigured
#ifdef VERBOSE
			printf("open_serial: ERROR bad parity bit configuration");
#endif
			break;
		}

		opt.c_cflag &= ~CSTOPB;
		opt.c_cflag |= (CLOCAL | CREAD | options.dataBits);

		/*
		 * Update options
		 * TCSANOW means that changes are immediates
		 */
		tcsetattr(file_descriptor, TCSANOW | TCSAFLUSH, &opt);
	}

	return file_descriptor;
}

int fixed_read(int file_descriptor,size_t fixed_lenght,void * buffer) {
	int bytes_read;
	size_t toBeRead = fixed_lenght;
	uint8_t * rebuffer = buffer;

	while (toBeRead>0) {
		bytes_read = read(file_descriptor,rebuffer+fixed_lenght-toBeRead,toBeRead);
		if (bytes_read == ERROR) {
			perror("read_serial: Unable to read from the serial port - ");
			return EXIT_FAILURE;
		}
		toBeRead = toBeRead - bytes_read;
	}
	return EXIT_SUCCESS;
}

int read_serial_packet(int file_descriptor,size_t fixed_lenght,void * buffer,uint8_t start) {
	int startIndex=0,i;
	uint8_t rebuffer[50];
	uint8_t * buffer_copy = buffer;

	if (fixed_read(file_descriptor,fixed_lenght,rebuffer) == EXIT_FAILURE) return EXIT_FAILURE;
	while ((rebuffer[startIndex])!=start) {
		startIndex++;
		if (startIndex==fixed_lenght) return NO_PACKET;
	}
	for (i=startIndex; i<fixed_lenght; i++) {
		buffer_copy[i-startIndex] = rebuffer[i];
	}
	if (startIndex!=0) {
		// SYNCHRO ZONE
		if (fixed_read(file_descriptor,startIndex,rebuffer) == EXIT_FAILURE) return EXIT_FAILURE;
		for (i=0; i<startIndex; i++) {
			buffer_copy[fixed_lenght+i] = rebuffer[i];
		}
	}
	return EXIT_SUCCESS;
}

int write_serial(int file_descriptor,size_t lenght,void * buffer) {
	int bytes_sent=0,result;

	while (bytes_sent<lenght) {
		result = write(file_descriptor,buffer,lenght);
		if (result == ERROR) {
			perror("write_serial: Unable to write from the serial port - ");
			return EXIT_FAILURE;
		}
		bytes_sent = bytes_sent + result;
	}
	return EXIT_SUCCESS;
}
