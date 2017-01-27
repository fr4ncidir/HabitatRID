#! /bin/bash

if [ "$1" == "setup" ]; then 
	sudo apt-get install libgsl0ldbl gsl-doc-info gsl-bin libgsl0-dev
	printf "\\nAnd now you can rerun without parameters\\n"
else
	gcc -Wall -I/usr/local/include main.c RIDLib.c serial.c -o RIDexecutable.exe -lgsl -lgslcblas -lm
fi
