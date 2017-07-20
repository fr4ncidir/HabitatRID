/*
 * RIDLib.c
 *
 *  Created on: 16 nov 2016
 *      Author: Francesco Antoniazzi
 * 	francesco.antoniazzi@unibo.it
 */

#include "RIDLib.h"

const uint8_t request_packet[STD_PACKET_STRING_DIM] = {REQUEST_COMMAND,SCHWARZENEGGER,'\0'};
const uint8_t reset_packet[STD_PACKET_STRING_DIM] = {RESET_COMMAND,SCHWARZENEGGER,'\0'};
const uint8_t detect_packet[STD_PACKET_STRING_DIM] = {DETECT_COMMAND,SCHWARZENEGGER,'\0'};
const size_t std_packet_size = STD_PACKET_DIM*sizeof(uint8_t);
RidParams parameters;

void printUsage(const char * error_message) {
	if (error_message!=NULL) fprintf(stderr,"%s\n",error_message);
	execlp("more","more","./manpage.txt",NULL);
	perror("Couldn't print manpage - ");
	exit(EXIT_FAILURE);
}

int log_file_txt(intVector * ids,intMatrix * sums,intMatrix * diffs,int index,int cols,coord location,char * logFileName) {
	time_t sysclock = time(NULL);
	TimeStruct * date = localtime(&sysclock);
	FILE * logFile;
	int j;

	if (!strcmp(logFileName,"")) {
		sprintf(logFileName,"M_%d-%d-%d-%d-%d-%d.txt",
			date->tm_mday,1+date->tm_mon,1900+date->tm_year,date->tm_hour,date->tm_min,date->tm_sec);
		printf("Info: File name: %s\n",logFileName);
		logFile = fopen(logFileName,"w");
		if (logFile == NULL) {
			fprintf(stderr,"Error while creating log file %s\n",logFileName);
			return EXIT_FAILURE;
		}
	}
	else {
		logFile = fopen(logFileName,"a");
		if (logFile == NULL) {
			fprintf(stderr,"Error while accessing log file %s\n",logFileName);
			return EXIT_FAILURE;
		}
	}
	fprintf(logFile,"%d %d %d %d %d ",date->tm_hour,date->tm_min,date->tm_sec,parameters.rid_identifier,gsl_vector_int_get(ids,index));
	for (j=0; j<cols; j++) {
		fprintf(logFile,"%d ",gsl_matrix_int_get(sums,index,j));
	}
	for (j=0; j<cols; j++) {
		fprintf(logFile,"%d ",gsl_matrix_int_get(diffs,index,j));
	}
	fprintf(logFile,"(%lf,%lf)\n",location.x,location.y);
	fclose(logFile);
	return EXIT_SUCCESS;
}

coord locateFromData(intVector * sum,intVector * diff,int nAngles) {
	intVector * mpr;
	coord location;
	double theta,radius;
	int maxIndexMPR;

	gsl_vector_int_reverse(sum);
	gsl_vector_int_reverse(diff);
	vector_subst(sum,-1,SUM_CORRECTION);
	vector_subst(diff,-1,DIFF_CORRECTION);

	mpr = gsl_vector_int_alloc(nAngles);
	gsl_vector_int_memcpy(mpr,sum);
	gsl_vector_int_sub(mpr,diff);

	maxIndexMPR = gsl_vector_int_max_index(mpr);
	theta = thetaFind(maxIndexMPR)*M_PI/180;
	radius = radiusFind(maxIndexMPR,sum);
	
#ifdef VERBOSE_CALCULATION
	printf("Debug: sum vector:\n");
	gsl_vector_int_fprintf(stdout,sum,"%d");
	printf("Debug: diff vector:\n");
	gsl_vector_int_fprintf(stdout,diff,"%d");
	printf("Debug: mpr vector:\n");
	gsl_vector_int_fprintf(stdout,mpr,"%d");
#endif
	logI("Info: radius=%lf\ntheta=%lf\n",radius,theta);
	location.x = radius*cos(theta)+parameters.BOTTOM_LEFT_CORNER_DISTANCE;
	location.y = radius*sin(theta);

	gsl_vector_int_free(mpr);
	return location;
}

/*coord* locateFromFile(const char logFileName[],int * output_dim) {
	FILE *logFile;
	coord *locations;
	intMatrix *data;
	intVector *row;
	intVector *sum;
	intVector *diff;
	int i,j,nAngles,rows,cols;

	logFile = fopen(logFileName,"rb");
	if (logFile == NULL) {
		fprintf(stderr,"Error while opening %s.\n",logFileName);
		return NULL;
	}

	fread(&rows,sizeof(int),1,logFile);
	fread(&cols,sizeof(int),1,logFile);
	data = gsl_matrix_int_alloc(rows,cols);
	row = gsl_vector_int_alloc(cols);
	gsl_matrix_int_fread(logFile,data);
	nAngles = (cols-4)/2;
	sum = gsl_vector_int_alloc(nAngles);
	diff = gsl_vector_int_alloc(nAngles);
	
	fclose(logFile);
	*output_dim = rows;
	
	locations = (coord *) malloc(rows*sizeof(coord));
	if (locations==NULL) {
		fprintf(stderr,"Fatal malloc error in locateFromFile: returning NULL pointer...\n");
	}
	else {
		for (j=0; j<rows; j++) {
			gsl_matrix_int_get_row(row,data,j);
			for (i=0; i<nAngles; i++) {
				gsl_vector_int_set(sum,i,gsl_vector_int_get(row,4+i));
				gsl_vector_int_set(diff,i,gsl_vector_int_get(row,4+nAngles+i));
			}

			locations[j] = locateFromData(sum,diff,nAngles);
			locations[j].id = gsl_vector_int_get(row,3);
		}
	}
	gsl_vector_int_free(sum);
	gsl_vector_int_free(diff);
	gsl_vector_int_free(row);
	gsl_matrix_int_free(data);
	return locations;
}*/

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

long sepaLocationUpdate(const char * SEPA_address,coord location,const char * unbounded_sparql) {
	char posUid[20];
	char ridUid[20];
	char *bounded_sparql;
	long result=-1;
	if (SEPA_address!=NULL) {
		// TODO query da riscrivere
		// active only if [-uSEPA_ADDRESS] is present
		sprintf(ridUid,"hbt:rid%d",location.id); //TODO must be checked
		sprintf(posUid,"hbt:pos%d",location.id); //TODO must be checked
		
		bounded_sparql = (char *) malloc((400+8*strlen(posUid)+strlen(ridUid)+20)*sizeof(char));
		
		sprintf(bounded_sparql,PREFIX_RDF PREFIX_HBT "DELETE {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateY ?oldY} INSERT {%s rdf:type hbt:ID. %s hbt:hasPosition %s. %s rdf:type hbt:Position. %s hbt:hasCoordinateX '%lf'. %s hbt:hasCoordinateY '%lf'} WHERE {OPTIONAL {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateX ?oldY}}", 
			posUid,posUid,				//DELETE {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateY ?oldY} 
			ridUid,ridUid,posUid, 			//INSERT {%s rdf:type hbt:ID. %s hbt:hasPosition %s.
			posUid,posUid,location.x,		//        %s rdf:type hbt:Position. %s hbt:hasCoordinateX '%lf'.
			posUid,location.y,			//		  %s hbt:hasCoordinateY '%lf'}
			posUid,posUid);				//WHERE {OPTIONAL {%s hbt:hasCoordinateX ?oldX. %s hbt:hasCoordinateX ?oldY}}
		result = kpProduce(bounded_sparql,SEPA_address,NULL);
		free(bounded_sparql);
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
		fprintf(stderr,"Error while opening %s.\n",fParam);
		return EXIT_FAILURE;
	}
	jsonString = (char *) malloc(jDim*sizeof(char));
	if (jsonString==NULL) {
		fprintf(stderr,"Malloc error while opening %s.\n",fParam);
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
		fprintf(stderr,"Result dimension parsing gave %d\n",jstok_dim);
		free(jsonString);
		return EXIT_FAILURE;
	}
	
	jstokens = (jsmntok_t *) malloc(jstok_dim*sizeof(jsmntok_t));
	if (jstokens==NULL) {
		fprintf(stderr,"Malloc error in json parsing!\n");
		free(jsonString);
		return EXIT_FAILURE;
	}
	jsmn_init(&parser);
	jsmn_parse(&parser, jsonString, strlen(jsonString), jstokens, jstok_dim);
	for (i=0; (i<jstok_dim-1) && (completed<20); i++) {
		if (jstokens[i].type==JSMN_STRING) {
			getJsonItem(jsonString,jstokens[i],&js_buffer);
			getJsonItem(jsonString,jstokens[i+1],&js_data);
			if (js_buffer[0]=='l') {
				sscanf(js_data,"%d",&(parameters.sample_time));
				completed++;
				continue;
			}
			if (js_buffer[0]=='I') {
				sscanf(js_data,"%d",&(parameters.rid_identifier));
				completed++;
				continue;
			}
			if (js_buffer[0]=='s') {
				parameters.http_sepa_address = NULL;
				parameters.http_sepa_address = strdup(js_data);
				if (parameters.http_sepa_address==NULL) {
					fprintf(stderr,"Error while allocating space for sepa address!\n");
					return EXIT_FAILURE;
				}
				completed++;
				continue;
			}
			if (js_buffer[0]=='r') {
				sscanf(js_data,"%lf",&(parameters.RADIUS_TH));
				completed++;
				continue;
			}	
			if (js_buffer[0]=='A') {
				sscanf(js_data,"%d",&(parameters.ANGLE_ITERATIONS));
				completed++;
				continue;
			}	
			if (js_buffer[0]=='B') {
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
			if (js_buffer[0]=='N') {
				sscanf(js_buffer,"N%d_%s",&n_field,id_field);
				if (!strcmp(id_field,"low")) sscanf(js_data,"%lf",&(parameters.N_low[n_field-1]));
				else sscanf(js_data,"%lf",&(parameters.N_high[n_field-1]));
				completed++;
				continue;
			}
			if (js_buffer[0]=='P') {
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
		fprintf(stderr,"ERROR! Parameter json not complete! (%d/20)\n",completed);
		return EXIT_FAILURE;
	}
	printf("N_low: %lf %lf %lf\n",parameters.N1_low,parameters.N2_low,parameters.N3_low);
	printf("N_high: %lf %lf %lf\n",parameters.N1_high,parameters.N2_high,parameters.N3_high);
	printf("Pr_low: %lf %lf %lf\n",parameters.Pr01_low,parameters.Pr02_low,parameters.Pr03_low);
	printf("Pr_high: %lf %lf %lf\n",parameters.Pr01_high,parameters.Pr02_high,parameters.Pr03_high);
	printf("dDegrees: %d\ndTheta: %d\nAngle iterations: %d\nBtm_left_corner: %lf\n",parameters.dDegrees,parameters.dTheta0,parameters.ANGLE_ITERATIONS,parameters.BOTTOM_LEFT_CORNER_DISTANCE);
	printf("Sample time: %d\nRID_id=%d\n",parameters.sample_time,parameters.rid_identifier);
	return EXIT_SUCCESS;
}
