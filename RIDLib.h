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
#include <ctype.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector_int.h>
#include <gsl/gsl_matrix_int.h>
#include <time.h>
#include "sepa_producer.h"

#define PREFIX_RDF		"PREFIX rdf:<http://www.w3.org/1999/02/22-rdf-syntax-ns#> "
#define PREFIX_HBT		"PREFIX hbt:<http://www.unibo.it/Habitat#> "
#define N1_low			N_low[0]
#define N2_low			N_low[1]
#define N3_low			N_low[2]
#define Pr01_low		Pr0_low[0]
#define	Pr02_low		Pr0_low[1]
#define Pr03_low		Pr0_low[2]
#define N1_high			N_high[0]
#define N2_high			N_high[1]
#define N3_high			N_high[2]
#define Pr01_high		Pr0_high[0]
#define	Pr02_high		Pr0_high[1]
#define Pr03_high		Pr0_high[2]
#define RANGE1			11
#define RANGE2			31

#define SUM_CORRECTION				-100
#define DIFF_CORRECTION				100
#define SEPA_UPDATE_BOUNDED			500
#define MATLAB_COMPATIBLE_TXT			1
//#define VERBOSE_CALCULATION


#define RESET_COMMAND				'+'
#define REQUEST_COMMAND				'<'
#define DETECT_COMMAND				'>'
#define SCHWARZENEGGER				'\n' // line feed packet terminator
#define STD_PACKET_DIM				2
#define STD_PACKET_STRING_DIM			3
#define WRONG_NID				255
#define CENTRE_RESCALE				256

typedef struct tm 				TimeStruct;
typedef gsl_vector_int 				intVector;
typedef gsl_matrix_int 				intMatrix;
typedef struct rid_parameters {
	double RADIUS_TH;
	double N_low[3],N_high[3];
	double Pr0_low[3],Pr0_high[3];
	int dDegrees,dTheta0,ANGLE_ITERATIONS;
	double BOTTOM_LEFT_CORNER_DISTANCE;
	int rid_identifier,sample_time;
	char *http_sepa_address;
} 						RidParams,*pRidParams;
typedef struct coordinates {
	int id;
	double x,y;
} 						coord;

void printUsage(const char * error_message);
int log_file_txt(intVector * ids,intMatrix * sums,intMatrix * diffs,int index,int cols,coord location,char * logFileName);
coord locateFromData(intVector * sum,intVector * diff,int nAngles);
coord* locateFromFile(const char logFileName[],int * output_dim);
double radiusFind(int i_ref2,intVector * sum);
double radiusFormula(double A,double B,double C);
double thetaFind(int i_ref);
int vector_subst(intVector * vector,int oldVal,int newVal);
void printLocation(FILE * output_stream,coord xy);
long sepaLocationUpdate(const char * SEPA_address,coord location,const char * unbounded_sparql);
int parametrize(const char * fParam);

#endif /* RIDLIB_H_ */
