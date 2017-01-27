/*
 * RIDLib.c
 *
 *  Created on: 16 nov 2016
 *      Author: Francesco Antoniazzi
 */

#include "RIDLib.h"

int log_file_txt(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,int matlab_mode) {
	time_t sysclock = time(NULL);
	TimeStruct * date = localtime(&sysclock);
	FILE * logFile;
	char logFileName[50];
	int i,j;

	sprintf(logFileName,"M_%d-%d-%d-%d-%d-%d.txt",
			date->tm_mday,1+date->tm_mon,1900+date->tm_year,date->tm_hour,date->tm_min,date->tm_sec);
	printf("File name: %s\n",logFileName);
	logFile = fopen(logFileName,"w");
	if (logFile == NULL) {
		printf("Error while opening log file in text append mode\n");
		return EXIT_FAILURE;
	}
	if (matlab_mode != MATLAB_COMPATIBLE_TXT) fprintf(logFile,"rows=%d\ncols=%d\n",rows,4+2*cols);
	for (i=0; i<rows; i++) {
		fprintf(logFile,"%d %d %d %d ",date->tm_hour,date->tm_min,date->tm_sec,gsl_vector_int_get(ids,i));
		for (j=0; j<cols; j++) {
			fprintf(logFile,"%d ",gsl_matrix_int_get(sums,i,j));
		}
		for (j=0; j<cols; j++) {
			fprintf(logFile,"%d ",gsl_matrix_int_get(diffs,i,j));
		}
		fprintf(logFile,"\n");
	}
	fclose(logFile);
	return EXIT_SUCCESS;
}

int log_file_bin(intVector * ids,intMatrix * sums,intMatrix * diffs,int rows,int cols,char * logFileName) {
	time_t sysclock = time(NULL);
	TimeStruct * date = localtime(&sysclock);
	FILE * logFile;
	intVector * temp;
	intMatrix * complete_matrix_log;
	int i,realCols;

	sprintf(logFileName,"M_%d-%d-%d-%d-%d-%d.rid",
			date->tm_mday,1+date->tm_mon,1900+date->tm_year,date->tm_hour,date->tm_min,date->tm_sec);
	logFile = fopen(logFileName,"wb");
	if (logFile == NULL) {
		printf("Error while opening log file in byte append mode\n");
		return EXIT_FAILURE;
	}

	realCols = 4+cols*2;
	complete_matrix_log = gsl_matrix_int_alloc(rows,realCols);
	temp = gsl_vector_int_alloc(rows);

	gsl_vector_int_set_all(temp,date->tm_hour);
	gsl_matrix_int_set_col(complete_matrix_log,0,temp);
	gsl_vector_int_set_all(temp,date->tm_min);
	gsl_matrix_int_set_col(complete_matrix_log,1,temp);
	gsl_vector_int_set_all(temp,date->tm_sec);
	gsl_matrix_int_set_col(complete_matrix_log,2,temp);
	gsl_matrix_int_set_col(complete_matrix_log,3,ids);
	for (i=4; i<cols+4; i++) {
		gsl_matrix_int_get_col(temp,sums,i-4);
		gsl_matrix_int_set_col(complete_matrix_log,i,temp);
		gsl_matrix_int_get_col(temp,diffs,i-4);
		gsl_matrix_int_set_col(complete_matrix_log,cols+i,temp);
	}

	fwrite(&rows,sizeof(int),1,logFile);
	fwrite(&realCols,sizeof(int),1,logFile);
	gsl_matrix_int_fwrite(logFile,complete_matrix_log);

	gsl_vector_int_free(temp);
	gsl_matrix_int_free(complete_matrix_log);
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

#ifdef VERBOSE_CALCULATION
	printf("sum vector:\n");
	gsl_vector_int_fprintf(stdout,sum,"%d");
	printf("diff vector:\n");
	gsl_vector_int_fprintf(stdout,diff,"%d");
	printf("mpr vector:\n");
	gsl_vector_int_fprintf(stdout,mpr,"%d");
#endif

	maxIndexMPR = gsl_vector_int_max_index(mpr);
	theta = thetaFind(maxIndexMPR)*M_PI/180;
	radius = radiusFind(maxIndexMPR,sum);

	printf("radius=%lf\ntheta=%lf\n",radius,theta);

	location.x = radius*cos(theta)+BOTTOM_LEFT_CORNER_DISTANCE;
	location.y = radius*sin(theta);

	gsl_vector_int_free(mpr);
	return location;
}

coord locateFromFile(const char logFileName[]) {
	FILE * logFile;
	coord location;
	intMatrix * data;
	intVector * row;
	intVector * sum;
	intVector * diff;
	int i,nAngles,rows,cols;

	logFile = fopen(logFileName,"rb");
	if (logFile == NULL) {
		printf("Error while opening %s.\n",logFileName);
		return location;
	}

	fread(&rows,sizeof(int),1,logFile);
	fread(&cols,sizeof(int),1,logFile);
	data = gsl_matrix_int_alloc(rows,cols);
	row = gsl_vector_int_alloc(cols);
	gsl_matrix_int_fread(logFile,data);
	gsl_matrix_int_get_row(row,data,rows-1);

	fclose(logFile);

	nAngles = (cols-4)/2;
	sum = gsl_vector_int_alloc(nAngles);
	diff = gsl_vector_int_alloc(nAngles);
	for (i=0; i<nAngles; i++) {
		gsl_vector_int_set(sum,i,gsl_vector_int_get(row,4+i));
		gsl_vector_int_set(diff,i,gsl_vector_int_get(row,4+nAngles+i));
	}

	location = locateFromData(sum,diff,nAngles);
	gsl_vector_int_free(sum);
	gsl_vector_int_free(diff);
	gsl_matrix_int_free(data);
	return location;
}

double radiusFind(int i_ref2,intVector * sum) {
	int power;
	double radius;
	power = gsl_vector_int_max(sum);
	if (i_ref2<RANGE1) {
		radius = radiusFormula(power,Pr01_low,N1_low);
		if (radius>RADIUS_TH) {
			return radiusFormula(power,Pr01_high,N1_high);
		}
	}
	else {
		if (i_ref2<RANGE2) {
			radius = radiusFormula(power,Pr02_low,N2_low);
			if (radius>RADIUS_TH) {
				return radiusFormula(power,Pr02_high,N2_high);
			}
		}
		else {
			radius = radiusFormula(power,Pr03_low,N3_low);
			if (radius>RADIUS_TH) {
				return radiusFormula(power,Pr03_high,N3_high);
			}
		}
	}
	return radius;
}

double radiusFormula(double A,double B,double C) {
	return pow(10,(-A+B)/(10*C));
}

double thetaFind(int i_ref) {
	double i_ref_deg = dDegrees/2-dDegrees*i_ref/(ANGLE_ITERATIONS-1);
	return i_ref_deg-dTheta0+dDegrees;
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

void printLocation(coord xy) {
	printf("\nLocation calculated: \n(x,y)=(%lf,%lf)\n",xy.x,xy.y);
}
