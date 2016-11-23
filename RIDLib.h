/*
 * RIDLib.h
 *
 *  Created on: 16 nov 2016
 *      Author: Francesco Antoniazzi
 */

#ifndef RIDLIB_H_
#define RIDLIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsl/gsl_vector_int.h>
#include <gsl/gsl_matrix_int.h>
#include <math.h>
#include <time.h>

#define N1			2
#define N2			2
#define N3			2
#define Pr01		-54.0
#define	Pr02		-52.0
#define Pr03		-52.0
#define RANGE1		11
#define RANGE2		31
#define dDegrees	90
#define dTheta0		-45

#define ANGLE_ITERATIONS				40
#define BOTTOM_LEFT_CORNER_DISTANCE		2.74
#define SUM_CORRECTION					-200
#define DIFF_CORRECTION					200

#define MATLAB_COMPATIBLE_TXT			1
//#define VERBOSE_CALCULATION

typedef struct tm 					TimeStruct;
typedef gsl_vector_int 				intVector;
typedef gsl_matrix_int 				intMatrix;
typedef struct coordinates {
	double x;
	double y;
} 									coord;

int log_file_txt(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,int matlab_mode);
int log_file_bin(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,char * logFileName);
coord localization(const char logFileName[]);
double radiusFind(int i_ref2,intVector * sum);
double radiusFormula(double A,double B,double C);
double thetaFind(int i_ref);
int vector_subst(intVector * vector,int oldVal,int newVal);


#endif /* RIDLIB_H_ */
