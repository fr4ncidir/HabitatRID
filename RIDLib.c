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

RidParams parameters;
SerialOptions ridSerial;
intVector *idVector;
intMatrix *sumVectors;
intMatrix *diffVectors;

int log_file_txt(intVector * ids,intVector * diffs,intVector * sums,int index,int nID,int cols,coord location,char * logFileName) {
	time_t sysclock = time(NULL);
	TimeStruct * date = localtime(&sysclock);
	FILE * logFile;
#ifdef MATLAB_COMPATIBILITY
	FILE * positions;
#endif
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
	if (index==0) positions = fopen("/var/www/html/posRID.txt","w");
	else positions = fopen("/var/www/html/posRID.txt","a");
	if (positions==NULL) {
		g_error("Error while opening in write mode /var/www/html/posRID.txt");
		return EXIT_FAILURE;
	}
	fprintf(positions,"%d %lf %lf",location.id,location.x,location.y);
	fclose(positions);
#else
	fprintf(logFile,"(%d,%lf,%lf)\n",location.id,location.x,location.y);
#endif
	fclose(logFile);
	return EXIT_SUCCESS;
}

coord locateFromData(intVector * diff,intVector * sum,int nAngles) {
	intVector * mpr;
	coord location;
	double theta,radius;
	int maxIndexMPR;
#ifdef VERBOSE_CALCULATION
	FILE * verbose;
#endif

	vector_subst(sum,-1,SUM_CORRECTION);
	vector_subst(diff,-1,DIFF_CORRECTION);
	
	/*
	g_warning("Difference vector element 0 set to 100");
	gsl_vector_int_set(diff,0,100);
	*/

	mpr = gsl_vector_int_alloc(nAngles);
	gsl_vector_int_memcpy(mpr,sum);
	gsl_vector_int_sub(mpr,diff);

	// esegue anche il reverse dell'indice
	maxIndexMPR = nAngles-1-gsl_vector_int_max_index(mpr);
	theta = (thetaFind(maxIndexMPR)-parameters.dTheta0+parameters.dDegrees)*M_PI/180;
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
	return parameters.dDegrees/2-parameters.dDegrees*i_ref/(parameters.ANGLE_ITERATIONS-1);
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

long sepaLocationUpdate(const char * SEPA_address,int rid_id,coord location) {
	char temp[40];
	char *bounded_sparql;
	long x,y,result=-1;
	if ((SEPA_address!=NULL) && (strcmp(SEPA_address,""))) {
		x = lround(location.x*100);
		y = lround(location.y*100);
		sprintf(temp,"%d%d%ld%ld",rid_id,location.id,x,y);
		bounded_sparql = (char *) malloc((strlen(PREFIX_RDF PREFIX_HBT SPARQL_FORMAT)+strlen(temp)+10)*sizeof(char));
		if (bounded_sparql==NULL) g_error("Malloc error while binding SPARQL");
		else {
			sprintf(bounded_sparql,PREFIX_RDF PREFIX_HBT SPARQL_FORMAT,rid_id,location.id,x,y);
			g_debug("Update produced:\n%s\n",bounded_sparql);
			result = kpProduce(bounded_sparql,SEPA_address,NULL);
			if (result!=200) g_message("Update http result = %ld\n",result);
			free(bounded_sparql);
		}
	}
	else g_warning("Empty sepa address! Skip update...");
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

int send_reset() {
	char dato = '+';
	return write_serial(ridSerial.serial_fd,ONE_BYTE,(void*) &dato);
}

int send_request() {
	char dato = '<';
	return write_serial(ridSerial.serial_fd,ONE_BYTE,(void*) &dato);
}

int receive_request_confirm() {
	uint8_t response[TWO_BYTES];
	int result;
	result = read_nbyte(ridSerial.serial_fd,TWO_BYTES,(void*) response);
	if (result!=EXIT_FAILURE) {
#ifdef VERBOSE_CALCULATION
		printUnsignedArray(response,TWO_BYTES);
		printf("\n");
#endif
		if ((response[0]!='<') || (response[1]!='\n')) {
			g_critical("Received unexpected %c%c instead of <\\n\n",(char) response[0],(char) response[1]);
			result = EXIT_FAILURE;
		}
	}
	return result;
}

int receive_id_info(uint8_t *id_info_result,int *read_bytes){
	int result;
	result = read_until_terminator(ridSerial.serial_fd,ALLOC_ID_MAX,(void*) id_info_result,'\n');
	if (result!=ERROR) {
		*read_bytes = result;
		result = EXIT_SUCCESS;
#ifdef VERBOSE_CALCULATION
		printUnsignedArray(id_info_result,*read_bytes);
		printf("\n");
#endif
	}
	else result = EXIT_FAILURE;
	return result;
}

int angle_iterations(int nID,int id_array_size,uint8_t *id_array) {
	int i,j,result;
	uint8_t *response;
	GTimer *timer;
	int response_dim = 2*nID+1;

#ifdef VERBOSE_CALCULATION
	FILE * verbose;
	verbose = fopen("./readAllAnglesLog.txt","a");
	if (verbose==NULL) {
		g_error("Verbose file open failure");
		return EXIT_FAILURE;
	}
#endif
	idVector = gsl_vector_int_alloc(nID);
	j=0;
	for (i=0; i<2*nID; i=i+2) {
		g_message("ID%d: %d",j,id_array[i]);
		gsl_vector_int_set(idVector,j,id_array[i]);
		j++;
	}
	
	sumVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
	diffVectors = gsl_matrix_int_alloc(nID,parameters.ANGLE_ITERATIONS);
	
	response = (uint8_t *) malloc(response_dim*sizeof(uint8_t));
	if (response==NULL) {
		g_critical("Malloc error in angle_iterations");
		return EXIT_FAILURE;
	}
	
	g_debug("Starting Angle iterations");
	timer = g_timer_new();
	
	for (i=0; i<parameters.ANGLE_ITERATIONS; i++) {
		fprintf(stderr,".");
		result = send_request();
		if (result==EXIT_FAILURE) {
			g_critical("send_request failure");
			free(response);
			return EXIT_FAILURE;
		}
		
		result = read_nbyte(ridSerial.serial_fd,response_dim,(void*) response);
		if (result == EXIT_FAILURE) {
			g_critical("Reading sum-diff vector for %d-th angle failure",i+1);
			free(response);
			return EXIT_FAILURE;
		}
#ifdef VERBOSE_CALCULATION
		else {
			printUnsignedArray(response,response_dim);
			printf("\n");
		}
#endif
		for (j=0; j<nID; j++) {
#ifdef VERBOSE_CALCULATION
			fprintf(verbose,"sum_diff_array:\nS\tD\n");
			fprintf(verbose,"%u\t%u\n",response[2*j],response[2*j+1]);
			fprintf(verbose,"%d\t%d\n\n",response[2*j]-CENTRE_RESCALE,response[2*j+1]-CENTRE_RESCALE);
#endif
			gsl_matrix_int_set(diffVectors,j,i,response[2*j]-CENTRE_RESCALE);
			gsl_matrix_int_set(sumVectors,j,i,response[2*j+1]-CENTRE_RESCALE);
		}
	}
	g_timer_stop(timer);
	fprintf(stderr,"completed in %lf ms\n",g_timer_elapsed(timer,NULL)*1000);
	g_timer_destroy(timer);
#ifdef VERBOSE_CALCULATION
	fclose(verbose);
#endif
	
	free(response);
	return EXIT_SUCCESS;
}

int send_detect() {
	char dato = '>';
	return write_serial(ridSerial.serial_fd,ONE_BYTE,(void*) &dato);
}

int receive_end_scan() {
	int result;
	uint8_t response[SIX_BYTES];
	result = read_nbyte(ridSerial.serial_fd,SIX_BYTES,(void*) response);
	if ((response[0]!=40) || (response[1]!=10) || (response[3]!=10) || (response[5]!=10)) {
		fprintf(stderr,"END_SCAN PACKET: ");
		if (result!=EXIT_FAILURE) {
			printUnsignedArray(stderr,response,SIX_BYTES);
			printf("\n");
		}
	}
	else g_message("Time1: %u;\t Time2: %u",response[2],response[4]);
	return result;
}
