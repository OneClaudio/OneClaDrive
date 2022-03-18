#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "./comm.h"

int sid=0;	//not connected

bool print=false;

/*----------------------------------------------Frequently Used Code-------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

#define WRITE( id, addr, size) do{ errno=0;							\
	int r=writefull( id, addr, size);								\
	if( r<0){	 /*strict version if( r!=size )*/					\
		perror("Error: during client write to server\n");			\
		errno=EIO;													\
		return -1;													\
		}															\
	}while(0);

#define READ( id, addr, size) do{ errno=0;							\
	int r=readfull( id, addr, size);								\
	if( r<0){	 /*strict version if( r!=size )*/					\
		perror("Error: during client read from server\n");			\
		errno=EIO;													\
		return -1;													\
		}															\
	}while(0);

/*	#define CHKNULL( ptr, str){					\	//TODO
	if( ptr==NULL ){							\
		fprintf("Error: during %s\n", str);		\
		errno=ENOMEM;							\
		return -1;								\
		}	*/

void setCmd( Cmd* cmd, int code, const char* pathname, int info){
	cmd->code=code;
	strncpy(cmd->filename, pathname, strlen(pathname));
	cmd->info=info;
	}

void errReply( Reply r, const char* pathname){
	switch( r ){
		case(NOTFOUND):
			fprintf(stderr, "Error: file %s NOT FOUND in the FSS\n", pathname);
			errno=ENOENT;
			break;
			
		case(EXISTS):
			fprintf(stderr, "Error: CANT CREATE file %s because the file is already present in the FSS\n", pathname);
			errno=EEXIST;
			break;
			
		case(LOCKED):
			fprintf(stderr, "Error: The file %s is currently LOCKED by another client\n", pathname);
			errno=EBUSY;
			break;
			
		case(NOTOPEN):
			fprintf(stderr, "Error: This client has NOT OPENED file %s and thus cannot access to it\n", pathname);
			errno=EACCESS;
			break;
		
		case(NOTLOCKED):
			fprintf(stderr, "Error: This client must acquire the LOCK on file %s before performing this action\n", pathname);
			errno=ENOLCK;
			break;
		
		case(TOOBIG):
			fprintf(stderr, "Error: The file %s is LARGER than the current FSS CAPACITY\n", pathname);
			errno=EFBIG;
			break;
		}
	}

#define CHKPATHNAME( pathname)				\
	if( pathname==NULL){					\
		errno=EINVAL;						\
		return -1;							\
		}									\
	if( strlen(pathname) > PATH_MAX ){		\
		errno=ENAMETOOLONG;					\
		return -1;							\
		}

#define CHKCONN( sid ) ....		//TODO

int INITCMDREPLY( int code, const char* pathname, int info ){	//Scambio iniziale con protocollo RICHIESTA-RISPOSTA usato in tutte le funzioni API

	if(code!=READN) CHKPATHNAME(pathname);	//ERR: pathname MISSING   or   pathname TOO LONG	EXCEPTION: READN doesnt use pathname
	CHKCONN(sid);							//ERR: not connected to server	
	
	Cmd cmd;	
	setCmd( &cmd, code, pathname, info);
	WRITE( sid, &cmd, sizeof(Cmd));
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));
	
	if( reply == OK ) return 0;		//OK!
	else{
		errReply( reply, pathname);	//ERR: PRINTS ERR and SETS ERRNO
		return -1;		
		}	
	}

#define STAT( pathname, filestat)	do{		\
	int r=stat( pathname, filestat);		\
	if( r!=0){								\	
	//TODO

static int SENDfile( int sid, char* pathname){
	
	FILE* file=fopen(pathname, "r");
    if (file==NULL){				//ERR: file not found or other (fopen SETS ERRNO)
    	perror("Error: file opening failed.");
    	return -1;
    	}	

    struct stat filestat;
    /* int r=*/stat(pathname, &filestat);
    /*if( r!=0){			
        fclose(file);				//ERR: filestat corrupted
        return -1;
		}	*/
	
	size_t size=filestat.st_size;
	void* cont=malloc(size);
	/*if(cont==NULL){				//ERR: malloc failure (sets ERRNO=ENOMEM)
		fclose(file);
		return -1;
		}
	
	/* size_t s=*/fread( cont, size, 1, file);
	/* if( s!=size){				//TODO invalid fread (can be handled similarly to readall/writeall)
			if( 	 feof(file)  )	...
			else if( ferror(file) ) ...
			errno=EIO;
			fclose(file);
			free(cont);
			return -1;
			}	*/

	WRITE(sid, &size, sizeof(size_t));
	WRITE(sid, cont, size);		//ERR: write error while sending data to server
	
	fclose(file);
	free(cont);
	
	return 0; //SUCCESS
	}

static int RECVfile( int sid, char* pathname, void** buf, size_t* sz, char* savedir){
	
	size_t size=0;	
	READ( sid, &size, sizeof(size_t) );
	
	void* cont=malloc( *size);
	if(cont==NULL) return -1;		//ERR: malloc failure (sets ERRNO=ENOMEM)
			
	READ( sid, cont, size);
		
	int readN=0;
	if( savedir!=NULL ){			//if a TRASH/SAVE DIR has been specified, the file gets written in a file in it

		if(pathname==NULL){			//if its a call from readNFile() or CACHE from openFile(), writeFile() or appendToFile()
										//extra READ from SERVER to get the file PATHNAME (+sets a flag to remember this)		
			pathname=malloc(PATH_MAX);
			if(pathname==NULL){		//ERR: malloc failure (sets ERRNO=ENOMEM)
				free(cont);
				return -1;
				}
				
			READ( sid, pathname, PATH_MAX);		//TODO free(cont) inside READ failure?
			readN=1;
			}
		
		//TODO CREATE DIR STRUCTURE inside SAVEDIR
		
		FILE* file=fopen( pathname, "w");
		if (file==NULL){				//ERR: file not found or other (fopen SETS ERRNO)
    		perror("Error: file opening failed.");
    		free(cont);
    		if(readN) free(pathname);
    		return -1;
    		}
    	
    	fwrite( cont, size, 1, file);	//TODO ERR CHECKING
    	}
    	
    if( buf!=NULL && sz!=NULL ){	//if BUF and SZ are present, the call comes from readFile() 
    	*buf=cont;							//the received file gets passed to the client trough these PTRs
    	*sz=size;							//the client will free CONT's memory
    	}
    else free(cont);
    
    if(readN) free(pathname);
    
    return 0;						//SUCCESS
	}
	
/*------------------------------------------------------API----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int openFile( const char* pathname, int flags, /* */const char* trashdir){	// /!\ TRASHDIR arg not present in specification
	int r=0;																	// needed for CACHE ALG. to work when the FSS hits the MAX NUM of files
	return r= INITCMDREPLY( OPEN, pathname, flags);
	
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all excess files have been EJECTED from the FSS
			
		else if( reply==CACHE){
			r= RECVfile( sid, NULL, NULL, NULL, trashdir );
			if( r!=0 ) return -1;		//TODO CACHE ALG could be moved in a function in F.U.C. to reduce repetitive code
			}
		}

	}

int closeFile( const char* pathname){
	int r=0;
	return r= INITCMDREPLY( CLOSE, pathname, 0);
	}
	
int writeFile( const char* pathname, const char* trashdir ){
	
	int r=0;
	r= INITCMDREPLY( WRITE, pathname, 0);
	if( r!=0 ) return -1;
	//else OK! Operation permitted.
	
	r= SENDfile( sid, pathname);	//WRITE(size) -> WRITE(cont)
	if( r!=0 ) return -1;				//ERR: see SENDfile (ERRNO SET)
	
	
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all excess files have been EJECTED from the FSS
			
		else if( reply==CACHE){
			r= RECVfile( sid, NULL, NULL, NULL, trashdir );
			if( r!=0 ) return -1;	
			}
		}
    
    return 0;	//SUCCESS
	}

int appendToFile( const char* pathname, void* buf, size_t size, const char* trashdir ){
	
	int r=0;
	r= INITCMDREPLY( APPEND, pathname, 0);
	if( r!=0 ) return -1;
	//else OK! Operation permitted.
	
	WRITE(sid, &size, sizeof(size_t));
	WRITE(sid, buf, size);
	
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all excess files have been EJECTED from the FSS
			
		else if( reply==CACHE){
			r= RECVfile( sid, NULL, NULL, NULL, trashdir );
			if( r!=0 ) return -1;	
			}
		}
    
    return 0;	//SUCCESS
	}

int readFile( const char* pathname, void** buf, size_t* size /*,const char* readdir*/){
	int r=0;											//it makes sense that like readNFiles() even readFile gets a READDIR arg to store read files
	r= INITCMDREPLY( READ, pathname, 0);					//READDIR isnt present in specification though
	if( r!=0 ) return -1;
	//else OK! Operation permitted.
	
	return RECVfile( sid, pathname, buf, size, NULL /*readdir*/);
	}

int readNFiles( int n; const char* readdir){

	int read=0;							//stores the count of READ files
	
	if( readdir==NULL)	return read;	//not much sense in requesting n reads if there is no READDIR specified to save all the files
		
	int r=0;
	r= INITCMDREPLY( READN, NULL, n);
	if( r!=0 ) return -1;
	//else OK! Operation permitted.
	
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all excess files have been EJECTED from the FSS
			
		else if( reply==ANOTHER){
			r= RECVfile( sid, NULL, NULL, NULL, readdir);
			if( r!=0 ) return -1;
			read++;
			}
		}
    
    return read;	//SUCCESS
	}

int removeFile(const char* pathname){
	int r=0;
	return r= INITCMDREPLY( REMOVE, pathname, 0);
	}

int lockFile(const char* pathname){
	int r=0;
	return r= INITCMDREPLY( LOCK, pathname, 0);		//TODO HANG VERSION
	}

int unlockFile(const char* pathname){
	int r=0;
	return r= INITCMDREPLY( UNLOCK, pathname, 0);
	}
