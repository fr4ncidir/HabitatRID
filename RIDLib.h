/*
 * RIDLib.h
 * 
 * Copyright 2017 Francesco Antoniazzi <francesco.antoniazzi@unibo.it>
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

#ifndef RIDLIB_H_
#define RIDLIB_H_

#include <stdlib.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector_int.h>
#include <gsl/gsl_matrix_int.h>
#include "serial.h"
#include "../SEPA-C/sepa_producer.h"

#define G_LOG_DOMAIN    "HabitatRID"
#include <glib.h>

#define PREFIX_RDF		"PREFIX rdf:<http://www.w3.org/1999/02/22-rdf-syntax-ns#> "
#define PREFIX_HBT		"PREFIX hbt:<http://www.unibo.it/Habitat#> "
#define SPARQL_FORMAT	"INSERT { ?idEvent rdf:type hbt:PositionEvent ; " \
						"hbt:timestamp ?timestamp ; " \
						"hbt:idReader '%d' ; " \
						"hbt:idActiveTag '%d' ; " \
						"hbt:posX '%lf' ; " \
						"hbt:posY '%lf'} WHERE { " \
						"BIND(IRI(CONCAT(\"http://it.unibo/habitat#PositionEvent-\",STRUUID())) AS ?idEvent) " \
						"BIND(now() AS ?timestamp)}"
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
#define MATLAB_COMPATIBILITY
//#define VERBOSE_CALCULATION

#define ALLOC_ID_MAX 				22 // numero_di_id+2*valori_id+terminatore    
#define ONE_BYTE					1
#define TWO_BYTES					2
#define SIX_BYTES					6
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

int log_file_txt(intVector * ids,intVector * diffs,intVector * sums,int index,int cols,coord location,char * logFileName);
coord locateFromData(intVector * diff,intVector * sum,int nAngles);
double radiusFind(int i_ref2,intVector * sum);
double radiusFormula(double A,double B,double C);
double thetaFind(int i_ref);
int vector_subst(intVector * vector,int oldVal,int newVal);
void printLocation(FILE * output_stream,coord xy);
long sepaLocationUpdate(const char * SEPA_address,int tag_id,coord location);
int parametrize(const char * fParam);
int send_reset();
int send_request();
int receive_request_confirm();
int receive_id_info();
int angle_iterations();
int send_detect();
int receive_end_scan();

#endif /* RIDLIB_H_ */
