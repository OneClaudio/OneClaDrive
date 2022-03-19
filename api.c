#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "./comm.h"
#include "./errcheck.h"

int sid=-1;		//Server ID

bool print=false;

/*----------------------------------------------Frequently Used Code-------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/
	
#define WRITE( id, addr, size){										\
		ErrNEG1( writefull(id, addr, size);							\
	/*	ErrDIFF( write(),  size);	//optional strict version */	\
		}
		
#define READ( id, addr, size){										\
		ErrNEG1( readfull(id, addr, size);							\
	/*	ErrDIFF( read(),  size);	//optional strict version */	\
		}

#define FWRITE( addr, size, n, id){	/*implicitly strict version*/	\
		ErrNEG1( fwritefull( addr, size, n, id) );					\
		}

#define FREAD( addr, size, n, id){	/*implicitly strict version*/	\
		ErrNEG1( fwritefull( addr, size, n, id) );					\
		}

#define CHKPATHNAME( pathname){				\
	if( pathname==NULL){					\
		errno=EINVAL;						\
		return -1;							\
		}									\
	if( strlen(pathname) > PATH_MAX ){		\
		errno=ENAMETOOLONG;					\
		return -1;							\
		}									\
	}

#define CHKCONN( sid ){						\
	if( sid==-1){							\
		errno=ENOTCONN;						\
		return -1;							\
		}									\
	}

static void setCmd( Cmd* cmd, CmdCode code, const char* pathname, int info){
	cmd->code=code;
	strncpy(cmd->filename, pathname, strlen(pathname));
	cmd->info=info;
	}

static void errReply( Reply r, const char* pathname){
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
		
		case(ALROPEN);
			fprintf(stderr, "Error: This client had ALREADY OPENED file %s\n", pathname);
			errno=EALREADY;
			break;
		
		case(NOTLOCKED):
			fprintf(stderr, "Error: This client must acquire the LOCK on file %s before performing this action\n", pathname);
			errno=ENOLCK;
			break;
			
		case(ALRLOCKED):
			fprintf(stderr, "Error: This client ALREADY HAS the LOCK on file %s\n", pathname);
			errno=EALREADY;
			break;
		
		case(NOTEMPTY):
			fprintf(stderr, "Error: Trying to WRITE on file %s which was already initialized, use APPEND functionality\n", pathname);
			errno=ENOTEMPTY;
			break;
		
		case(TOOBIG):
			fprintf(stderr, "Error: The file %s is LARGER than the current FSS CAPACITY\n", pathname);
			errno=EFBIG;
			break;
		}
	}

static int firstCMDREPLY( CmdCode code, const char* pathname, int info ){
	//Scambio iniziale con protocollo RICHIESTA-RISPOSTA usato in tutte le funzioni API

	if(code!=READN) CHKPATHNAME(pathname);	//ERR: pathname MISSING   or   pathname TOO LONG	EXCEPTION: READN doesnt use pathname
	CHKCONN(sid);							//ERR: not connected to server	
	
	Cmd cmd;	
	setCmd( &cmd, code, pathname, info);
	WRITE( sid, &cmd, sizeof(Cmd));			//ERR: SETS ERRNO
	
	Reply reply;
	READ( sid, &reply, sizeof(Reply));		//ERR: SETS ERRNO
	
	if( reply == OK ) return 0;				//OK!
	else{
		errReply( reply, pathname);			//ERR: PRINTS ERR and SETS ERRNO
		return -1;		
		}
		
ErrCLEANUP
	return -1;
ErrCLEAN

	}

static int SENDfile( char* pathname){
	
	FILE* file=NULL;
	ErrNULL(  file=fopen(pathname, "r")	 );			//ERR: file not found or other (fopen SETS ERRNO)

    struct stat filestat;
    ErrNEG1(  stat(pathname, &filestat)  );			//ERR: filestat corrupted	(stat SETS ERRNO)
	
	size_t size=filestat.st_size;
	
	void* cont=NULL;
	ErrNULL(  cont=malloc(size);  );				//ERR: malloc failure (malloc SETS ERRNO=ENOMEM)
	
	FREAD(cont, size, 1, file);						//ERR: file read failure (freadfull SETS ERRNO=EIO)

	WRITE(sid, &size, sizeof(size_t));
	WRITE(sid, cont, size);							//ERR: write error while sending data to server
	
SUCCESS
	fclose(file);
	free(cont);	
	return 0;

ErrCLEANUP
	if(file) fclose(file);
	if(cont) free(cont);
	return -1;
ErrCLEAN

	}

/* @brief	used by READ to receive a KNOWN file														(buf,sz!=NULL  savedir==NULL)
 *			used by READN to receive an UNKNOWN file													(buf,sz==NULL  savedir!=NULL)
 *			used by OPEN, WRITE, APPEND to receive an UNKNOWN file triggered by CACHE ALG. ejection		(buf,sz==NULL  savedir!=NULL)
 */
static int RECVfile( void** buf, size_t* sz, char* savedir){
	char * pathname=NULL;
	size_t size=0;
	void* cont=NULL;
	FILE* file=NULL;			//if SAVING enabled this is used to write the RECEIVED file
		
	READ( sid, &size, sizeof(size_t) );
	ErrNULL(  cont=malloc( *size)  );				//ERR: malloc failure (sets ERRNO=ENOMEM)	
	READ( sid, cont, size);							//ERR: read error while receiving data from server

	if( savedir!=NULL ){				//if a TRASH/SAVE DIR has been specified the file gets written in a file in it	
			ErrNULL(  pathname=malloc(PATH_MAX) );		//(if the caller is READN/OPEN/WRITE/APPEND)	
			newfile=true;	
			READ( sid, pathname, PATH_MAX);		//an extra READ is needed from SERVER to get the file PATHNAME (+sets a flag to remember this)
			}
		
		//TODO CREATE DIR STRUCTURE inside SAVEDIR
		
		ErrNULL(  file=fopen( pathname, "w");		//ERR: file not found or other (fopen SETS ERRNO)
    	
    	FWRITE( cont, size, 1, file);				//ERR: file write error (fwritefull SETS ERRNO=EIO)
    	}	
    
    if( buf!=NULL && sz!=NULL ){	//if BUF and SZ are present, the call comes from readFiles() 
    	*buf=cont;							//the received file gets passed to the client trough these PTRs
    	*sz=size;							//the user of the API has the duty to free CONT's memory
    	}
    else free(cont);
 
SUCCESS
	if(pathname) free(pathname);
    return 0;
    
ErrCLEANUP
	if(cont) free(cont);
	if(pathname) free(pathname);
	return -1;
ErrCLEAN
	}
	
static int CACHEretrieve( const char* savedir){
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if(reply==OK) break;			//EXIT COND: this means that all excess files have been EJECTED from the FSS
		else if( reply==CACHE)			//OTHERWISE: keep receiving the files from the FSS
			ErrNEG1(  RECVfile( NULL, NULL, savedir);
		}
SUCCESS
	return 0;
ErrCLEANUP
	return -1;
ErrCLEAN
	}
	
static double diffTimespec(const struct timespec *t1, const struct timespec *t0){
	return (t1->tv_sec - t0->tv_sec) + (t1->tv_nsec - t0->tv_nsec)/1000000000.0;	//10^9
	}

/*------------------------------------------------------API----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int openFile( const char* pathname, int flags, /* */const char* trashdir){	// /!\ TRASHDIR arg not present in specification
	ErrNEG1(  firstCMDREPLY( OPEN, pathname, flags)  );
	ErrNEG1(  CACHEretrieve( trashdir)  );

SUCCESS	
	return 0;
ErrCLEANUP
	return -1;
ErrCLEAN
	}

int closeFile( const char* pathname){
	return firstCMDREPLY( CLOSE, pathname, 0);
	}
	

int writeFile( const char* pathname, const char* trashdir ){
	ErrNEG1(  firstCMDREPLY( WRITE, pathname, 0)  );
	ErrNEG1(  SENDfile( pathname)  );
	ErrNEG1(  CACHEretrieve( trashdir)  );

SUCCESS	
	return 0;

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int appendToFile( const char* pathname, void* buf, size_t size, const char* trashdir ){
	ErrNEG1(  firstCMDREPLY( APPEND, pathname, 0)  );
	
	WRITE(sid, &size, sizeof(size_t));
	WRITE(sid, buf, size);
	
	ErrNEG1(  CACHEretrieve( trashdir)  );

SUCCESS	
	return 0;

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int readFile( const char* pathname, void** buf, size_t* size /*,const char* readdir*/){
															//it makes sense that like readNFiles() even readFile gets a READDIR arg to store read files
	ErrNEG1(  firstCMDREPLY( READ, pathname, 0)  );				//READDIR isnt present in the specification though
	return RECVfile( buf, size, NULL /*readdir*/);

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int readNFiles( int n; const char* readdir){
	int read=0;							//stores the count of READ files
	
	if( readdir==NULL)	return read;	//not much sense in requesting n reads if there is no READDIR specified to save all the files
		
	ErrNEG1(  firstCMDREPLY( READN, 0, n)  );
	
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all excess files have been EJECTED from the FSS
		else if( reply==ANOTHER){
			ErrNEG1(  RECVfile( NULL, NULL, readdir)  );
			read++;
			}
		}
SUCCESS
    return read;

ErrCLEANUP
	int errnocpy=errno;
	fprint(stderr,"Error: readNFiles() failed after %d successful reads\n", read);
	errno=errnocpy;
	return -1;
ErrCLEAN
	}


int removeFile(const char* pathname){
	return firstCMDREPLY( REMOVE, pathname, 0);
	}


int lockFile(const char* pathname){
	CHKPATHNAME(pathname);					//ERR: pathname MISSING   or   pathname TOO LONG
	CHKCONN(sid);							//ERR: not connected to server
	
	do{
		Cmd cmd;	
		setCmd( &cmd, code, pathname, info);
		WRITE( sid, &cmd, sizeof(Cmd));			//ERR: SETS ERRNO
	
		Reply reply;
		READ( sid, &reply, sizeof(Reply));		//ERR: SETS ERRNO
	
	} while( /* opt sleep */ reply==LOCKED );
	
	if( reply == OK ) return 0;				//OK!
	else{
		errReply( reply, pathname);			//ERR: PRINTS ERR and SETS ERRNO
		return -1;		
		}

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int unlockFile(const char* pathname){
	return firstCMDREPLY( UNLOCK, pathname, 0);
	}
	
int openConnection(const char* sockname, int msec, const struct timespec abstime){
	
	if( sockname==NULL || msec<0 || abstime.tv_sec<0 || abstime.tv_nsec<0 ){
		errno=EINVAL;
		return -1;
		}
	
	if( sid != -1 ){
		errno=EISCONN;
		return -1;
		}
	
	ErrNEG1(  sid=socket(AF_UNIX, SOCK_STREAM, 0)  );	//socket() assigns a free channel to sid (Server ID) 
	
	struct sockaddr_un saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;							//specify the domain (AF_UNIX) and the path (sockname) of the SOCKET
	strncpy(saddr.sun_path, sockname,);
	
	
	struct timespec sleeptime;		//INTERVAL time between the connection attempts
	sleeptime.tv_sec=  msec/1000
	sleeptime.tv_nsec=(msec%1000)*1000000;
	
	struct timespec currtime;		//at each conn attempt CURRTIME is checked against ABSTIME
	
	bool connected=false;
	
	do{
		if( connect( sid, (struct sockaddr *)&saddr, SUN_LEN(&saddr) ) == 0){		//immediate FIRST TRY
			connected=true;
			break;
			}
		
		else nanosleep( sleeptime, NULL);				//HOLD for msec ms
		clock_gettime(CLOCK_REALTIME, &currtime);		//gets the CURRENT TIME
	
	} while( diffTimespec( &abstime, &currtime)>0 );	//if ABSTIME hasnt been reached LOOP again
	
	if(connected) return 0;
	else return -1;
	
ErrCLEANUP
	return -1;
ErrCLEAN
	}

int closeConnection(const char* sockname){
	
	CHKCONN(sid);

	if( sockname==NULL){
		errno=EINVAL;
		return -1;
		}
	
	Cmd cmd;	
	setCmd( &cmd, QUIT, NULL, 0);
	WRITE( sid, &cmd, sizeof(Cmd));
	
	ErrNEG1( close(sid)  );
	sid=-1;											

ErrCLEANUP
	return -1;
ErrCLEAN
	}
