#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <linux/limits.h>	//PATH_MAX
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>			//timespec for openConnection()
#include <sys/stat.h>		//mkdir() for mkpath()
#include <libgen.h>			//dirname() for mkpath()

#include "./comm.h"
#include "./errcheck.h"
#include "./api.h"
#include "./utils.h"		//READ(), WRITE(), FREAD(), FWRITE()

int sid=-1;		//Server ID
char* ssocket=NULL;

bool print=false;

/*-------------------------------------------------INTERNAL USE------------------------------------------------------*/
/*---------------------------------------------Frequently Used Code--------------------------------------------------*/	

#define CHKPATHNAME( pathname){												\
	if( pathname==NULL){													\
		fprintf(stderr,"Logic: no file specified\n");						\
		errno=EINVAL;														\
		return -1;															\
		}																	\
	if( strlen(pathname) > PATH_MAX-1){										\
		fprintf(stderr,"Warning: file pathname longer than %d\n", PATH_MAX-1);\
		errno=ENAMETOOLONG;													\
		return -1;															\
		}																	\
	}

#define CHKCONN( sid ){																	\
	if( sid==-1){																		\
		fprintf(stderr, "Warning: client not connected, use openConnection() first\n");	\
		errno=ENOTCONN;																	\
		return -1;																		\
		}																				\
	}

#define ErrREPL( rv ) if( (rv)==-1) goto ErrCleanup;
//SKIPS the default errno printing inside ErrNEG1 for all firstCMDREPLY() calls (they already give detailed logic errors)
		

static void setCmd( Cmd* cmd, CmdCode code, const char* pathname, int info){
	memset( cmd, 0, sizeof(Cmd) );
	cmd->code=code;
	strncpy(cmd->filename, pathname, strlen(pathname));
	cmd->info=info;
	}

static void errReply( Reply r, const char* pathname){
	switch( r ){
		case(NOTFOUND):
			fprintf(stderr, "Logic: file '%s' NOT FOUND in the FSS\n", pathname);
			errno=ENOENT;
			break;
			
		case(EXISTS):
			fprintf(stderr, "Logic: CANT CREATE file '%s' because the file is already present in the FSS\n", pathname);
			errno=EEXIST;
			break;
			
		case(LOCKED):
			fprintf(stderr, "Logic: The file '%s' is currently LOCKED by another client\n", pathname);
			errno=EBUSY;
			break;
			
		case(NOTOPEN):
			fprintf(stderr, "Logic: This client has NOT OPENED file '%s' and thus cannot access to it\n", pathname);
			errno=EACCES;
			break;
		
		case(ALROPEN):
			fprintf(stderr, "Logic: This client had ALREADY OPENED file '%s'\n", pathname);
			errno=EALREADY;
			break;
		
		case(NOTLOCKED):
			fprintf(stderr, "Logic: This client must acquire the LOCK on file '%s' before performing this action\n", pathname);
			errno=ENOLCK;
			break;
			
		case(ALRLOCKED):
			fprintf(stderr, "Logic: This client ALREADY HAS the LOCK on file '%s'\n", pathname);
			errno=EALREADY;
			break;
		
		case(NOTEMPTY):
			fprintf(stderr, "Logic: Trying to WRITE on file '%s' which was already initialized, use APPEND functionality\n", pathname);
			errno=ENOTEMPTY;
			break;
		
		case(TOOBIG):
			fprintf(stderr, "Logic: The file '%s' is LARGER than the current FSS CAPACITY\n", pathname);
			errno=EFBIG;
			break;
			
		default:
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
	
	if( reply!=OK ){
		errReply( reply, pathname);			//ERR: PRINTS ERR and SETS ERRNO
		if( reply==ALROPEN || reply==ALRLOCKED) return 0;	//these are NON DISRUPTIVE
		else return -1;										//all others are DISRUPTIVE		
		}
	else return 0;		//reply==OK						
		
ErrCLEANUP
	return -1;
ErrCLEAN

	}

static int SENDfile(const char* pathname){
	
	FILE* file=NULL;
	ErrNULL(  file=fopen(pathname, "r")	 );			//ERR: file not found or other (fopen SETS ERRNO)

    struct stat filestat;
    ErrNEG1(  stat(pathname, &filestat)  );			//ERR: filestat corrupted	(stat SETS ERRNO)
	
	size_t size=filestat.st_size;
	//printf("size: %d\n", size);
	
	void* cont=NULL;
	ErrNULL(  cont=calloc(1, size)  );				//ERR: malloc failure (malloc SETS ERRNO=ENOMEM)
	
	FREAD(cont, size, 1, file);						//ERR: file read failure (freadfull SETS ERRNO=EIO)
	
	//printf("cont: %s\n", (char*)cont);

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

/** @brief	used by READN to receive an UNKNOWN file
 *			used by OPEN, WRITE, APPEND to receive an UNKNOWN file triggered by CACHE ALG. ejection
 */
static int RECVfile(const char* savedir){
	char * pathname=NULL;
	size_t size=0;
	void* cont=NULL;
		
	READ( sid, &size, sizeof(size_t) );
	ErrNULL(  cont=malloc( size)  );				//ERR: malloc failure (sets ERRNO=ENOMEM)	
	READ( sid, cont, size);
	ErrNULL(  pathname=malloc(PATH_MAX) );			//ERR: ...
	READ( sid, pathname, PATH_MAX);					//an extra READ is needed from SERVER to get the file PATHNAME (+sets a flag to remember this)
	
	if( savedir!=NULL )				//if a TRASH/SAVE DIR has been specified the file gets written in a file in it	
		SAVEfile( cont, size, pathname, savedir);

	free(cont);
	free(pathname);
    return 0;
    
ErrCLEANUP
	if(cont) free(cont);
	if(pathname) free(pathname);
	return -1;
ErrCLEAN
	}
	
static int CACHEretrieve( const char* trashdir){
	while(1){						//LOOP for each file EJECTED from the FSS by the cache alg.
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if(reply==OK) break;			//EXIT COND: this means that all excess files have been EJECTED from the FSS
		else if( reply==CACHE)			//OTHERWISE: keep receiving the files from the FSS
			ErrNEG1(  RECVfile(trashdir)  );
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

/*---------------------------------------------------PUBLIC USE------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int mkpath(char *dir){											//no NULL checks, it's the caller's duty
    if (strlen(dir) == 1 && dir[0] == '/') return 0;			//exit cond: only '/' remains
    
    if( strcmp(dir, "..")==0 || strcmp(dir,".")==0 ) return 0;	//ignores '.' and '..' otherwise endless recursive loop
    
    mkpath(dirname(strdupa(dir)));		//ES:   str: /home/path/file.txt    dirname( str ): /home/path    basename(str): file.txt
												//strdupa( str):   strdup with automatic free, compile with -D_GNU_SOURCE
    if( mkdir(dir, 0777)==-1)			//Dont care about permissions for this, using default system mask should be good
    	if(errno != EEXIST)
    		ErrFAIL;								//ERR: if it fails at any level for some reason (other than dir alr existing) (mkdir SETS ERRNO)
SUCCESS
	return 0;
ErrCLEANUP
	return -1;
ErrCLEAN
   	}
	
int SAVEfile(void* cont, size_t size,const char* pathname, const char* savedir){
	if( savedir==NULL ){
		fprintf(stderr,"Warning: directory not specified, file '%s' wont be saved\n", pathname);
		return 0;
		}
	if( strlen(savedir)+1+strlen(pathname) > PATH_MAX-1 ){
		fprintf(stderr,"Error: resulting pathname longer than %d chars, file '%s' wont be saved\n", PATH_MAX-1, pathname);
		errno=ENAMETOOLONG;
		return -1;
		}

	FILE* file=NULL;

	char* altname=strdupa(pathname);
	for( int i=0; i<strlen(altname); i++)			// pathname: ./home/spock/sol/kirk.txt  ->  altfname: f-home-spock-sol-kirk.txt
		if( altname[i]=='/') altname[i]='-';
	
	if( altname[0]=='.') altname[0]='f';			// /!\ without this all relative path files (starting with a dot '.') are HIDDEN in the folders	
	
	//mode_t oldmask=umask(0000);
	
	char* dircpy=strdupa(savedir);
	ErrNEG1(  mkpath(dircpy)  );
	
	//umask(oldmask);
	
	char finalpathname[PATH_MAX];
	memset( finalpathname, '\0', PATH_MAX );
	strcpy( finalpathname, savedir );
	strcat( finalpathname, "/");
	strcat( finalpathname, altname);	
	
	ErrNULL(  file=fopen( finalpathname, "w")  );
	FWRITE( cont, size, 1, file);
	fflush(file); /* fsync(fileno(file)); */

SUCCESS
	fclose(file);
	return 0;
ErrCLEANUP
	if(file) fclose(file);
	return -1;
ErrCLEAN
	}

/*------------------------------------------------------API----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int openFile( const char* pathname, int flags, /* */const char* trashdir){	// /!\ TRASHDIR arg not present in specification
	ErrREPL(  firstCMDREPLY( OPEN, pathname, flags)  );
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
	ErrREPL(  firstCMDREPLY( WRITE, pathname, 0)  );
	ErrNEG1(  SENDfile( pathname)  );
	ErrNEG1(  CACHEretrieve( trashdir)  );

SUCCESS	
	return 0;

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int appendToFile( const char* pathname, void* buf, size_t size, const char* trashdir ){
	ErrREPL(  firstCMDREPLY( APPEND, pathname, 0)  );
	
	WRITE(sid, &size, sizeof(size_t));
	WRITE(sid, buf, size);
	
	ErrNEG1(  CACHEretrieve( trashdir)  );

	return 0;	//SUCCESS

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int readFile( const char* pathname, void** buf, size_t* sz /*,const char* readdir*/){
	if( buf==NULL || sz==NULL ){						//it makes sense that like readNFiles() even readFile gets a READDIR arg to store read files
		errno=EINVAL;											//READDIR isnt present in the specification though
		return -1;
		}
	
	ErrREPL(  firstCMDREPLY( READ, pathname, 0)  );
	
	size_t size=0;
	void* cont=NULL;
		
	READ( sid, &size, sizeof(size_t) );
	ErrNULL(  cont=malloc( size)  );				//ERR: malloc failure (sets ERRNO=ENOMEM)	
	READ( sid, cont, size);	

    *buf=cont;							//the received file gets passed to the client trough these PTRs
    *sz=size;							//the user of the API has the duty to free CONT's memory
	
	free(cont);
	return 0;

ErrCLEANUP
	return -1;
ErrCLEAN
	}


int readNFiles( int n, const char* readdir){
	int read=0;							//stores the count of READ files
	
	if( readdir==NULL)	return read;	//not much sense in requesting n reads if there is no READDIR specified to save all the files
		
	ErrREPL(  firstCMDREPLY( READN, "", n)  );
	
	while(1){						//LOOP for each file to be READ from the FSS
		Reply reply;
		READ( sid, &reply, sizeof(Reply));
		
		if ( reply==OK) break;			//exit condition: this means that all requested files have been sent back to the client
		else if( reply==ANOTHER){
			ErrNEG1(  RECVfile(readdir)  );
			read++;
			}
		}
SUCCESS
    return read;

ErrCLEANUP
	int errnocpy=errno;
	fprintf(stderr,"Error: readNFiles() failed after %d successful reads\n", read);
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
	
	Reply reply;	
	do{
		Cmd cmd;
		setCmd( &cmd, LOCK, pathname, 0);
		WRITE( sid, &cmd, sizeof(Cmd));			//ERR: SETS ERRNO
	
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
	strncpy(saddr.sun_path, sockname, strlen(sockname));
	
	
	struct timespec sleeptime;		//INTERVAL time between the connection attempts
	sleeptime.tv_sec=  msec/1000;
	sleeptime.tv_nsec=(msec%1000)*1000000;
	
	struct timespec currtime;		//at each conn attempt CURRTIME is checked against ABSTIME
	
	bool connected=false;
	
	do{
		if( connect( sid, (struct sockaddr *)&saddr, SUN_LEN(&saddr) ) == 0){		//immediate FIRST TRY
			connected=true;
			break;
			}
		
		else nanosleep( &sleeptime, NULL);				//HOLD for msec ms
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
	
	if( strcmp( sockname, ssocket ) !=0 ){
		fprintf(stderr, "Error: trying to close a different socket\n");
		errno=EBADF;
		return -1;
		}
	
	Cmd cmd;	
	setCmd( &cmd, QUIT, "", 0);
	WRITE( sid, &cmd, sizeof(Cmd));
	
	ErrNEG1( close(sid)  );
	sid=-1;
	return 0;
											
ErrCLEANUP
	return -1;
ErrCLEAN
	}
	
int ezOpen(const char* sockname){
	
	if( sockname==NULL){
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
	strncpy(saddr.sun_path, sockname, strlen(sockname));
	
	ssocket=strdup(sockname);
	
	ErrNEG1( connect( sid, (struct sockaddr *)&saddr, SUN_LEN(&saddr))  );
	return 0;
	
ErrCLEANUP
	return -1;
ErrCLEAN
	}
