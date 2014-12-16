/* 
 *  Name: Mengyu YANG
 *   Andrew ID: mengyuy@andrew.cmu.edu
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>


typedef struct {
    int valid;
    int tag;
    /* a larger time value means
       the line is used more recently*/
    int time;
} line;


int hitN = 0;
int missN = 0;
int evicN = 0;

int readCommand(int argc, char *argv[], int *s, int *E, int *b, char *fileName)
{
    int opt;
    int count = 0;

    while((opt = getopt(argc, argv, "s:E:b:t:")) != -1){
     
        if( opt == 's'){
           count++;
           *s = atoi(optarg);
        } 
        else if( opt == 'E'){
          count++;
          *E = atoi(optarg);
        }
        else if(opt == 'b'){
          count++;
          *b = atoi(optarg);
        }
        else if (opt =='t'){
          count++;
          strcpy(fileName, optarg); 
        }
    }
  
    if (count != 4)
      exit(-1);
 
    return 0;
  
}


int returnSet (int address, int s, int b){
    int mask = 0x7fffffffffffffff >> (63-s);
    return (address>>b) & mask;
}


long returnTag (int address, int s, int b){
    int mask = 0x7fffffffffffffff >> (s+b-1);
    return (address>>(s+b)) & mask;
}


int updateTime (line **cache, int setBits, int i, int E){
   
    for (int j=0; j < E; j++){
        if ((cache[setBits][j].valid == 1) &&
	   (cache[setBits][j].time > cache[setBits][i].time)){
           cache[setBits][j].time--;
        }
    } 
    cache[setBits][i].time = E;
    return 0;
}

int simulate (line **cache, char *commandLine, int s, int E, int b){

    char opt;
    long address, tagBits;
    int setBits;
    int i;

    sscanf(commandLine, " %c %lx", &opt, &address);
    setBits = returnSet (address, s, b);
    tagBits = returnTag (address, s, b);

    for (i = 0; i < E; i++) {
        if ((cache[setBits][i].valid == 1) &&
       	    (cache[setBits][i].tag == tagBits)) {
            // This is a hit
            hitN ++;
            if ('M' == opt)
 	        hitN ++;
       
            updateTime(cache, setBits, i, E);
            return 0;         
        }
    } 

    missN ++;
    for (i = 0; i < E; i++) {
        if (cache[setBits][i].valid == 0) {
            // This is a miss without eviction    
            cache[setBits][i].valid = 1;
            cache[setBits][i].tag = tagBits;
            updateTime (cache, setBits, i, E);
            if ('M' == opt)
	        hitN ++;	     
            return 0;   
        }
    }

    evicN ++;
    // This is a miss followed by eviction
    for (i = 0; i < E; i++) {
      if (cache[setBits][i].time == 1) {                                    
            cache[setBits][i].tag = tagBits;
            updateTime (cache, setBits, i, E);
            if ('M' == opt)
                hitN ++;             
            return 0;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    FILE *file;
    char fileName[100];
    char commandLine[100];
    int s, E, b;
    line **cache;
    int setNum;
    
    readCommand(argc, argv, &s, &E, &b, fileName);
    setNum = pow(2.0,s);
  
    // Initialize the cache
    cache = malloc (setNum * sizeof(line*));

    for (int i = 0; i < setNum; i++){
        cache[i] = malloc(E * sizeof(line));
        for (int j = 0; j < E; j++)
	    cache[i][j].valid = 0;
    }

    // read tracefile commands and simulate the cache
    file = fopen(fileName,"r");
    if (!file){
        printf("File not found!\n");
        return -1;
    }
    while (fgets(commandLine, 100, file) != NULL){
        if (commandLine[0] == ' '){
            commandLine[strlen(commandLine)-1] = '\0';
            simulate(cache, commandLine, s, E, b);
        }
    }
    fclose(file);

    printSummary(hitN, missN, evicN);

    return 0;
}
