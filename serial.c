/**
 * @mainpage C serial libray main page
 * Contact <francesco.antoniazzi1991@gmail.com>
 */

/**
 * serial.c
 * @author Francesco Antoniazzi
 * @version 0.9
 * @date 20 oct 2016
 */
#include "serial.h"

int open_serial(const char name[],SerialOptions * options) {
	int file_descriptor;
	char message[50];
	struct termios opt;

	/*
	 * O_RDWR means that we want to open the serial port in read and write
	 * O_NOCTTY means that it's the user the controller of the terminal, not the serial port itself
	 * O_NDLEAY the open function is not blocking
	 */
	file_descriptor = open(name, O_RDWR | O_NOCTTY | O_NDELAY);
	options->serial_fd = file_descriptor;
	if (file_descriptor == ERROR) {
		sprintf(message,"open_serial: Unable to open %s -",name);
		perror(message);
		return ERROR;
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
		cfsetispeed(&opt,options->baudRate);
		cfsetospeed(&opt,options->baudRate);
		opt.c_cflag &= ~CSIZE;

		switch (options->parityBit) {
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
			fprintf(stderr,"open_serial: ERROR bad parity bit configuration");
			return ERROR;
		}

		/*
		 * This is the stop bit option. If ~CSTOPB, 1 stop bit. Else, 2 bits
		 */
		if (options->stopbits==ONE_STOP) opt.c_cflag &= ~CSTOPB;
		else opt.c_cflag |= CSTOPB;

		opt.c_cflag |= (CLOCAL | CREAD | options->dataBits);

		/*
		 * Update options
		 * TCSANOW means that changes are immediates
		 */
		tcsetattr(file_descriptor, TCSANOW | TCSAFLUSH, &opt);
		return EXIT_SUCCESS;
	}
}

int read_nbyte(int file_descriptor,size_t fixed_lenght,void * buffer) {
	int bytes_read;
	size_t toBeRead = fixed_lenght;
	uint8_t * rebuffer = buffer;

	while (toBeRead>0) {
		bytes_read = read(file_descriptor,rebuffer+fixed_lenght-toBeRead,toBeRead);
		if ((bytes_read == ERROR) || (!bytes_read)) {
			perror("read_nbyte: Unable to read from the serial port - ");
			return EXIT_FAILURE;
		}
		toBeRead = toBeRead - bytes_read;
	}
	return EXIT_SUCCESS;
}

int read_nbyte_packet(int file_descriptor,size_t fixed_lenght,void * buffer,uint8_t startByte) {
	int startIndex=0,i;
	uint8_t * rebuffer;
	uint8_t * buffer_copy = buffer;

	rebuffer = (uint8_t *) malloc(fixed_lenght*sizeof(uint8_t));

	if (read_nbyte(file_descriptor,fixed_lenght,rebuffer) == EXIT_FAILURE) {
		free(rebuffer);
		return EXIT_FAILURE;
	}
	while ((rebuffer[startIndex])!=startByte) {
		startIndex++;
		if (startIndex==fixed_lenght) {
			free(rebuffer);
			return NO_PACKET;
		}
	}
	for (i=startIndex; i<fixed_lenght; i++) {
		buffer_copy[i-startIndex] = rebuffer[i];
	}
	if (startIndex!=0) {
		// SYNCHRO ZONE
		if (read_nbyte(file_descriptor,startIndex,rebuffer) == EXIT_FAILURE) {
			free(rebuffer);
			return EXIT_FAILURE;
		}
		for (i=0; i<startIndex; i++) {
			buffer_copy[fixed_lenght+i] = rebuffer[i];
		}
	}
	free(rebuffer);
	return EXIT_SUCCESS;
}

int read_until_terminator(int file_descriptor,size_t max_dim,void * buffer,uint8_t terminator) {
	uint8_t * rebuffer = buffer;
	uint8_t bytebuffer;
	int i=0,bytes_read;

	do {
		bytes_read = read(file_descriptor,&bytebuffer,1);
		if ((bytes_read == ERROR) || (!bytes_read)) {
			perror("read_until_terminator: Unable to read from the serial port - ");
			return ERROR;
		}
		if (bytes_read == 1) {
			rebuffer[i]=bytebuffer;
			i++;
		}
	} while ((i<max_dim) && (bytebuffer!=terminator));
	return i;
}

int write_serial(int file_descriptor,size_t lenght,void * buffer) {
	int bytes_sent=0,result;

	while (bytes_sent<lenght) {
		result = write(file_descriptor,buffer,lenght);
		if ((result == ERROR) || (!result)) {
			perror("write_serial: Unable to write from the serial port - ");
			return EXIT_FAILURE;
		}
		bytes_sent = bytes_sent + result;
	}
	return EXIT_SUCCESS;
}

int send_packet(const int serial_descriptor,const uint8_t packet[],const size_t dim,const char error_msg[]) {
	int result;
	result = write_serial(serial_descriptor,dim,(void *) packet);
	if (result == EXIT_FAILURE) {
		fprintf(stderr,"%s\n",error_msg);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
