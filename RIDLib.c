/*
 * RIDLib.c
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

#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <time.h>
#include "RIDLib.h"

#define G_LOG_DOMAIN    "HabitatRID"
#include <glib.h>

const uint8_t request_packet[STD_PACKET_STRING_DIM] = {REQUEST_COMMAND,SCHWARZENEGGER,'\0'};
const uint8_t reset_packet[STD_PACKET_STRING_DIM] = {RESET_COMMAND,SCHWARZENEGGER,'\0'};
const uint8_t detect_packet[STD_PACKET_STRING_DIM] = {DETECT_COMMAND,SCHWARZENEGGER,'\0'};
const size_t std_packet_size = STD_PACKET_DIM*sizeof(uint8_t);
RidParams parameters;

int log_file_txt(intVector * ids,intVector * sums,intVector * diffs,int index,int cols,coord location,char * logFileName) {
	time_t sysclock = time(NULL);
	TimeStruct * date = localtime(&sysclock);
	FILE * logFile;
	int j;

	if (!strcmp(logFileName,"")) {
		sprintf(logFileName,"M_%d-%d-%d-%d-%d-%d.txt",
			date->tm_mday,1+date->tm_mon,1900+date->tm_year,date->tm_hour,date->tm_min,date->tm_sec);
		g_debug("File name: %s",logFileName);
		logFile = fopen(logFileName,"w");
		if (logFile == NULL) {
			g_critical("Error while creating log file %s\n",logFileName);
			return EXIT_FAILURE;
		}
	}
	else {
		logFile = fopen(logFileName,"a");
		if (logFile == NULL) {
			g_critical("Error while accessing log file %s\n",logFileName);
			return EXIT_FAILURE;
		}
	}
	fprintf(logFile,"%d %d %d %d %d ",date->tm_hour,date->tm_min,date->tm_sec,parameters.rid_identifier,gsl_vector_int_get(ids,index));
	for (j=0; j<cols; j++) {
		fprintf(logFile,"%d ",gsl_vector_int_get(sums,j));
	}
	for (j=0; j<cols; j++) {
		fprintf(logFile,"%d ",gsl_vector_int_get(diffs,j));
	}
#ifdef MATLAB_COMPATIBILITY
	fprintf(logFile,"\n");
#else
	fprintf(logFile,"(%lf,%lf)\n",location.x,location.y);
#endif
	fclose(logFile);
	return EXIT_SUCCESS;
}

coord locateFromData(intVector * sum,intVector * diff,int nAngles) {
	intVector * mpr;
	coord location;
	double theta,radius;
	int maxIndexMPR;
#ifdef VERBOSE_CALCULATION
	FILE * verbose;
#endif

	vector_subst(sum,-1,SUM_CORRECTION);
	vector_subst(diff,-1,DIFF_CORRECTION);

	mpr = gsl_vector_int_alloc(nAngles);
	gsl_vector_int_memcpy(mpr,sum);
	gsl_vector_int_sub(mpr,diff);

	// esegue anche il reverse dell'indice
	maxIndexMPR = nAngles-1-gsl_vector_int_max_index(mpr);
	theta = thetaFind(maxIndexMPR)*M_PI/180;
	radius = radiusFind(maxIndexMPR,sum);
	
#ifdef VERBOSE_CALCULATION
	verbose = fopen("./verbose.txt","a");
	fprintf(verbose,"Debug: sum vector:\n");
	gsl_vector_int_fprintf(verbose,sum,"%d");
	fprintf(verbose,"Debug: diff vector:\n");
	gsl_vector_int_fprintf(verbose,diff,"%d");
	fprintf(verbose,"Debug: mpr vector:\n");
	gsl_vector_int_fprintf(verbose,mpr,"%d");
	fclose(verbose);
#endif
	g_debug("Radius=%lf - Theta=%lf",radius,theta);
	location.x = radius*cos(theta)+parameters.BOTTOM_LEFT_CORNER_DISTANCE;
	location.y = radius*sin(theta);

	gsl_vector_int_free(mpr);
	return location;
}

double radiusFind(int i_ref2,intVector * sum) {
	int power;
	double radius;
	power = gsl_vector_int_max(sum);
	if (i_ref2<RANGE1) {
		radius = radiusFormula(power,parameters.Pr01_low,parameters.N1_low);
		if (radius>parameters.RADIUS_TH) {
			return radiusFormula(power,parameters.Pr01_high,parameters.N1_high);
		}
	}
	else {
		if (i_ref2<RANGE2) {
			radius = radiusFormula(power,parameters.Pr02_low,parameters.N2_low);
			if (radius>parameters.RADIUS_TH) {
				return radiusFormula(power,parameters.Pr02_high,parameters.N2_high);
			}
		}
		else {
			radius = radiusFormula(power,parameters.Pr03_low,parameters.N3_low);
			if (radius>parameters.RADIUS_TH) {
				return radiusFormula(power,parameters.Pr03_high,parameters.N3_high);
			}
		}
	}
	return radius;
}

double radiusFormula(double A,double B,double C) {
	return pow(10,(-A+B)/(10*C));
}

double thetaFind(int i_ref) {
	double i_ref_deg = parameters.dDegrees/2-parameters.dDegrees*i_ref/(parameters.ANGLE_ITERATIONS-1);
	return i_ref_deg-parameters.dTheta0+parameters.dDegrees;
}

int vector_subst(intVector * vector,int oldVal,int newVal) {
	int i,substitutions=0;
	for (i=0; i<vector->size; i++) {
		if (gsl_vector_int_get(vector,i)==oldVal) {
			gsl_vector_int_set(vector,i,newVal);
			substitutions++;
		}
	}
	return substitutions;
}

void printLocation(FILE * output_stream,coord xy) {
	fprintf(output_stream,"Location of id %d: (x,y)=(%lf,%lf)\n",xy.id,xy.x,xy.y);
}

long sepaLocationUpdate(const char * SEPA_address,coord location) {
	char posUid[15];
	char *bounded_sparql;
	long result=-1;
	if ((SEPA_address!=NULL) && (strcmp(SEPA_address,""))) {
		// TODO ancora non è una WebThing
		sprintf(posUid,"Pos_%d",location.id);
		bounded_sparql = (char *) malloc((strlen(PREFIX_RDF PREFIX_HBT SPARQL_FORMAT)+5*strlen(posUid)+40)*sizeof(char));
		if (bounded_sparql==NULL) g_error("Malloc error while binding SPARQL");
		else {
			sprintf(bounded_sparql,PREFIX_RDF PREFIX_HBT SPARQL_FORMAT,posUid,posUid,posUid,location.x,posUid,location.y,posUid,posUid);
			result = kpProduce(bounded_sparql,SEPA_address,NULL);
			free(bounded_sparql);
		}
	}
	return result;
}

int parametrize(const char * fParam) {
	FILE *json;
	jsmn_parser parser;
	jsmntok_t *jstokens;
	char c;
	char *jsonString,*js_buffer=NULL,*js_data=NULL,id_field[7];
	int i=0,jDim=300,n_field,jstok_dim,completed=0;
	
	json = fopen(fParam,"r");
	if (json==NULL) {
		g_critical("Error while opening %s.\n",fParam);
		return EXIT_FAILURE;
	}
	jsonString = (char *) malloc(jDim*sizeof(char));
	if (jsonString==NULL) {
		g_critical("Malloc error while opening %s.\n",fParam);
		fclose(json);
		return EXIT_FAILURE;
	}
	
	do {
		c = getc(json);
		if ((c=='{') || (c=='}') || (c=='"') || (c=='.') || (c==',') || (c=='-') || (c==':') || (c=='_') || (c=='/') || (isalnum(c))) {
			jsonString[i]=c;
			i++;
			if (i==jDim) {
				jDim += 100;
				jsonString = (char *) realloc(jsonString,jDim*sizeof(char));
			}
		}
	} while (!feof(json));
	fclose(json);
	
	jsmn_init(&parser);
	jstok_dim = jsmn_parse(&parser, jsonString, strlen(jsonString), NULL, 0);
	if (jstok_dim<0) {
		g_critical("Result dimension parsing gave %d\n",jstok_dim);
		free(jsonString);
		return EXIT_FAILURE;
	}
	
	jstokens = (jsmntok_t *) malloc(jstok_dim*sizeof(jsmntok_t));
	if (jstokens==NULL) {
		g_critical("Malloc error in json parsing!\n");
		free(jsonString);
		return EXIT_FAILURE;
	}
	jsmn_init(&parser);
	jsmn_parse(&parser, jsonString, strlen(jsonString), jstokens, jstok_dim);
	for (i=0; (i<jstok_dim-1) && (completed<20); i++) {
		if (jstokens[i].type==JSMN_STRING) {
			getJsonItem(jsonString,jstokens[i],&js_buffer);
			getJsonItem(jsonString,jstokens[i+1],&js_data);
			if (js_buffer[0]=='l') { // loc_time
				sscanf(js_data,"%d",&(parameters.sample_time));
				completed++;
				continue;
			}
			if (js_buffer[0]=='I') { // ID_rid
				sscanf(js_data,"%d",&(parameters.rid_identifier));
				completed++;
				continue;
			}
			if (js_buffer[0]=='s') { // sepa_ip
				parameters.http_sepa_address = NULL;
				parameters.http_sepa_address = strdup(js_data);
				if (parameters.http_sepa_address==NULL) {
					g_error("Error while allocating space for sepa address!\n");
					return EXIT_FAILURE;
				}
				completed++;
				continue;
			}
			if (js_buffer[0]=='r') { // radius_threshold
				sscanf(js_data,"%lf",&(parameters.RADIUS_TH));
				completed++;
				continue;
			}	
			if (js_buffer[0]=='A') {
				sscanf(js_data,"%d",&(parameters.ANGLE_ITERATIONS));
				completed++;
				continue;
			}	
			if (js_buffer[0]=='B') { // Bottom_Left_Corner_Distance
				sscanf(js_data,"%lf",&(parameters.BOTTOM_LEFT_CORNER_DISTANCE));
				completed++;
				continue;
			}
			if (!strcmp(js_buffer,"delta_Degrees")) {
				sscanf(js_data,"%d",&(parameters.dDegrees));
				completed++;
				continue;
			}
			if (!strcmp(js_buffer,"delta_Theta0")) {
				sscanf(js_data,"%d",&(parameters.dTheta0));
				completed++;
				continue;
			}
			if (js_buffer[0]=='N') { // N parameters
				sscanf(js_buffer,"N%d_%s",&n_field,id_field);
				if (!strcmp(id_field,"low")) sscanf(js_data,"%lf",&(parameters.N_low[n_field-1]));
				else sscanf(js_data,"%lf",&(parameters.N_high[n_field-1]));
				completed++;
				continue;
			}
			if (js_buffer[0]=='P') { // P parameters
				sscanf(js_buffer,"Pr0%d_%s",&n_field,id_field);
				if (!strcmp(id_field,"low")) sscanf(js_data,"%lf",&(parameters.Pr0_low[n_field-1]));
				else sscanf(js_data,"%lf",&(parameters.Pr0_high[n_field-1]));
				completed++;
			}	
		}
	}
	free(jstokens);
	free(jsonString);
	free(js_data);
	if (completed!=20) {
		g_error("ERROR! Parameter json not complete! (%d/20)\n",completed);
		return EXIT_FAILURE;
	}
	g_debug("\nN_low: %lf %lf %lf\n",parameters.N1_low,parameters.N2_low,parameters.N3_low);
	g_debug("\nN_high: %lf %lf %lf\n",parameters.N1_high,parameters.N2_high,parameters.N3_high);
	g_debug("\nPr_low: %lf %lf %lf\n",parameters.Pr01_low,parameters.Pr02_low,parameters.Pr03_low);
	g_debug("\nPr_high: %lf %lf %lf\n",parameters.Pr01_high,parameters.Pr02_high,parameters.Pr03_high);
	g_debug("\ndDegrees: %d\ndTheta: %d\nAngle iterations: %d\nBtm_left_corner: %lf\n",parameters.dDegrees,parameters.dTheta0,parameters.ANGLE_ITERATIONS,parameters.BOTTOM_LEFT_CORNER_DISTANCE);
	g_debug("\nSample time: %d\nRID_id=%d\nRadius threshold: %lf\n",parameters.sample_time,parameters.rid_identifier,parameters.RADIUS_TH);
	
	if (!strcmp(parameters.http_sepa_address,"")) g_warning("\nDetected empty SEPA address: will skip SEPA interaction!\n");
	else g_debug("\nSEPA address: %s\n",parameters.http_sepa_address);
	return EXIT_SUCCESS;
}
