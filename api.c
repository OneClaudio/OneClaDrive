#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>								//for MAX_INPUT

#include "./comm.h"

void setcmd( Cmd* cmd, int code, const char* pathname, int info){
	cmd->code=code;
	strncpy(cmd->filename, pathname, strlen(pathname));
	cmd->info=info;
	}	

int openFile( const char* pathname, int flags){
	if( pathname==NULL);		//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)
	
	Cmd cmd;
	setcmd( &cmd, OPEN, pathname, flags)
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	
	//ERR or SUCCESS (could print inside the functionor in the client)
	//set ERRNO
	//return CODE (0/-1)	
	}

int closeFile( const char* pathname){
	if( pathname==NULL);		//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;	
	setcmd( &cmd, CLOSE, pathname, 0)
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(int));
	
	//ERR or SUCCESS (could print inside the functionor in the client)
	//set ERRNO
	//return CODE (0/-1)	
	}
	
int writeFile( const char* pathname, const char* trashdir ){
	if( pathname==NULL);		//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;	
	setcmd( &cmd, WRITE, pathname, 0);
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	//se ok continua




	FILE* file=fopen(pathname, "r");
    if (file==0) return -1;		//ERR (file didnt open/not found)

    struct stat filestat;  // * filestat.st_size è la dimensione del file
    if (stat(pathname, &filestat) == -1) {
        fclose(file);
        return -1;		//ERR (file stat error)
    }
    
    void* filecont=malloc(filestat.st_size);
    fread( filecont, filestat.st_size, 1, file);
    
    WRITE( sid, &filestat.st_size, sizeof(int) );
    WRITE( sid, &filecont, filestat.st_size );
    
    free filecont;
    
    
    while( /*CI SONO FILE DA RESTITUIRE PER FARE SPAZIO A QUELLO CHE HAI MANDATO*/ ){
    	
    	//READ( SIZE/CONT)
    	//if(trashdir!=NULL) salvali in trashdir, senno' free
    	}
    
    return 0;	//SUCCESS
	}

int appendToFile( const char* pathname, void* buf, size_t size, const char* trashdir ){
	//SIMILAR TO writeFile()
	}

int readFile( const char* pathname, void** buf, size_t* size, const char* readdir){
	if( pathname==NULL);	//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;	
	setcmd( &cmd, READ, pathname, 0);

	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	//se ok continua
	
	
	READ( sid, size, sizeof(size_t) );
	
	*buf=malloc( *size);
	//ERR CHECK MALLOC
	
	READ( sid, *buf, *size);
	
	if( readdir!=NULL){
		FILE* file= fopen( /*readdir/filename (not the original pathname)*/)
		fwrite(*buf, *size, 1, file);
		}

	//no free perche' buf e size sono i parametri che vengono ritornati al cliente caller
	
	return 0;
	}

int readNFiles( int n; const char* readdir){
	
	Cmd cmd;	
	setcmd( &cmd, READN, NULL, n);
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	//se ok continua
	
	for( int i=0; i<n; i++){
		
		int size=0;
		READ( sid /*SIZE o FILE FINITI*/);
		
		if( /* FILE FINITI */) break;
		
		void* cont=malloc( size);
		//ERR CHECK MALLOC
	
		READ( sid, *buf, *size);
	
		if( readdir!=NULL){
			FILE* file= fopen( /*readdir/filename (not the original pathname)*/)
			fwrite(*buf, *size, 1, file);
			}
		}
	
	return 0;		
	}

int removeFile(const char* pathname){
	if( pathname==NULL);	//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;	
	setcmd( &cmd, REMOVE, pathname, 0);
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	//REMOVED
	
	return 0;
	}

int lockFile(const char* pathname){
	if( pathname==NULL);	//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;
	setcmd( &cmd, LOCK, pathname, 0);
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	
	//se non e' riuscito ad acquisire la lock perche' gia' lockata rimanda cicla da WRITE( sid, cmd,...)
	
	//altrimenti SUCCESS
	return 0;	
	}

int unlockFile(const char* pathname){
	if( pathname==NULL);	//ERR (file pathname missing)
	if( strlen(pathname)<MAX_INPUT)	return //ERR (pathname too long)	
	
	Cmd cmd;
	setcmd( &cmd, LOCK, pathname, 0);
	
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	//se non riesce la UNLOCK si vede che il client nonha mai fatto la LOCK
	//SUCCESS
	return 0;
	}
	

	


