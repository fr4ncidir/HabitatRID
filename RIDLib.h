/*
 * RIDLib.h
 *
 *  Created on: 16 nov 2016
 *      Author: Francesco Antoniazzi
 * 	francesco.antoniazzi@unibo.it
 */

#ifndef RIDLIB_H_
#define RIDLIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector_int.h>
#include <gsl/gsl_matrix_int.h>
#include <math.h>
#include <time.h>

#define RADIUS_TH		6
#define N1_low			2
#define N2_low			2
#define N3_low			2
#define Pr01_low		-63.0
#define	Pr02_low		-61.0
#define Pr03_low		-63.0
#define N1_high			3
#define N2_high			3
#define N3_high			3
#define Pr01_high		-74.0
#define	Pr02_high		-72.0
#define Pr03_high		-74.0
#define RANGE1			11
#define RANGE2			31
#define dDegrees		90
#define dTheta0			0

#define ANGLE_ITERATIONS				40
#define BOTTOM_LEFT_CORNER_DISTANCE		2.74
#define SUM_CORRECTION					-100
#define DIFF_CORRECTION					100

#define MATLAB_COMPATIBLE_TXT			1
#define VERBOSE_CALCULATION


#define RESET_COMMAND				'+'
#define REQUEST_COMMAND				'<'
#define DETECT_COMMAND				'>'
#define SCHWARZENEGGER				'\n' // line feed packet terminator
#define STD_PACKET_DIM				2
#define STD_PACKET_STRING_DIM		3
#define WRONG_NID					255
#define CENTRE_RESCALE				256

typedef struct tm 					TimeStruct;
typedef gsl_vector_int 				intVector;
typedef gsl_matrix_int 				intMatrix;
typedef struct coordinates {
	int id;
	double x;
	double y;
} 									coord;

void printUsage(const char * error_message);
int log_file_txt(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,int matlab_mode);
int log_file_bin(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,char * logFileName);
coord locateFromData(intVector * sum,intVector * diff,int nAngles);
coord*  locateFromFile(const char logFileName[],int * output_dim);
double radiusFind(int i_ref2,intVector * sum);
double radiusFormula(double A,double B,double C);
double thetaFind(int i_ref);
int vector_subst(intVector * vector,int oldVal,int newVal);
void printLocation(FILE * output_stream,coord xy);


#endif /* RIDLIB_H_ */
