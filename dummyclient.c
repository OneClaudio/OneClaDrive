#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "./api.h"
#include "./comm.h"


#define SPATHNAME "./server_sol"	//Server socket pathname

int main(){
	ezOpen(SPATHNAME);
	
	printf("CLOSE (fail)\n");
	closeFile( "COJONE" );
	
	int flag=O_CREATE | O_LOCK;
	
	//openFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", 0, NULL);
	
	printf("OPEN (ok)\n");
	openFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", flag, NULL);
	
	printf("LOCK (ok)\n");
	lockFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("WRITE (ok)\n");
	writeFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", NULL);
	
	printf("READ (ok)\n");
	void* r=NULL; size_t sz;
	readFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", &r, &sz);
	
	printf("read: %s\n", (char*)r);
	
	printf("APPEND (ok)\n");
	char* s=strdup("lazy dog e' n cojone pero'\n");	
	appendToFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", (void*)s, strlen(s), NULL);
	
	printf("UNLOCK (ok)\n");
	unlockFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("CLOSE (ok)\n");
	closeFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("CLOSE (fail)\n");
	writeFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", NULL);
	
	printf("OPEN (ok)\n");
	openFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", 0 ,NULL);
	
	printf("READ (ok)\n");
	readFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog", (void*)r, &sz);

	printf("LOCK (ok)\n");
	lockFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("RMV (ok)\n");
	removeFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("CLOSE (fail)\n");
	closeFile("/home/oneclaudio/Desktop/SOL/PROGETTO/OneClaDrive/TEST/lazydog");
	
	printf("END CONN (ok)\n");
	closeConnection(SPATHNAME);
	
	return 0;
	}
	
	
	
	
