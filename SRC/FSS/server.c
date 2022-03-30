#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>	//PATH_MAX
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h> 

#include <errcheck.h>
#include <utils.h>		//READ(), WRITE(), FREAD(), FWRITE()
#include <comm.h>
#include <idlist.h>
#include <filestorage.h>

	/*------------------------Locks----------------------------*/

#define LOCK(l)     ErrERRNO(  pthread_mutex_lock(l)   );		/* In a MACRO only so they can have a more CONCISE name */
#define UNLOCK(l)   ErrERRNO(  pthread_mutex_unlock(l) );

#define RDLOCK(l)   ErrNZERO(  pthread_rwlock_rdlock(l)  );
#define WRLOCK(l)   ErrNZERO(  pthread_rwlock_wrlock(l)  );
#define RWUNLOCK(l) ErrNZERO(  pthread_rwlock_unlock(l)  );

	/*-------------------Config-(default)----------------------*/

#define CONFIGMAXLINE (PATH_MAX+32)

int maxNumFiles=100;
size_t maxCapacity=64000000;	//64MB

	/*-------------------------Log-----------------------------*/

//#define DEBUG			//Uncomment to print log and dbg info to screen

bool DBG=false;

FILE* Log=NULL;
pthread_mutex_t LogMutex=PTHREAD_MUTEX_INITIALIZER;
		
#define LOG(...)					\
	{								\
		if(DBG)						\
			printf(__VA_ARGS__);	\
									\
		LOCK(&LogMutex);			\
		fprintf( Log,__VA_ARGS__);	\
		UNLOCK(&LogMutex);			\
		fflush(Log);				\
		}

#define LOGOP( esit, file, sz)																								\
	{	char size[20]="0";																									\
		if(sz!=NULL) sprintf(size,"%zu", *(size_t*)sz );																	\
		LOG( "WORK: CID=%-4d""%-7s""%-10s""%s %s""\t%s\n", cid, strCmdCode(cmd.code), strReply(esit), ( sz ? size : ""),	\
		 													( sz ? "B" : "\t"), file ? file : ""  );							\
		}

	/*-------------------------Signals-------------------------*/

#define OFF 0x00
#define SOFT 0x01
#define ON 0x02

volatile sig_atomic_t Status=ON;

void handlesoft(int unused){
	Status=SOFT;
	LOG("MAIN: SOFT QUIT SIGNAL RECEIVED -> NO NEW CONNECTIONS ALLOWED\n");
	return; ErrCLEANUP ErrCLEAN
	}

void  handleoff(int unused){
	Status=OFF;
	LOG("MAIN: HARD QUIT SIGNAL RECEIVED -> NO FURTHER REQUESTS WILL BE EXECUTED\n");
	return; ErrCLEANUP ErrCLEAN
	}

	/*-------------------------Threads-------------------------*/

#define CREATE( t, att, f, arg) ErrNEG1(  pthread_create( t, att, f, arg)  );

#define ACCEPT( cid, sid, addr, l){ errno=0;				\
	cid=accept( sid, addr, l);								\
	if(cid<0){												\
		if( errno==EAGAIN) continue;						\
		else ErrFAIL;										\
		}													\
	}

#define  PSELECT(max, r, w, e, t, mask){ errno=0;			\
	nrdy=pselect(max, r, w, e, t, mask);					\
	if(nrdy<0 && errno!=EINTR) ErrFAIL;						\
	}
	
#define REPLY( MSG ){	/* Each thread sends a REPLY to the CLIENT using this simple macro */	\
	Reply reply;							\
	memset( &reply, 0, sizeof(Reply) );		\
	reply=MSG;								\
	WRITE( cid, &reply, sizeof(Reply) );	\
	}



#define TERMINATE -2		//CODE to TERMINATE each THREAD when the times come
IdList* pending;	//Thread safe FIFO QUEUE holds CIDs ready for the WORKER threads					M -> WT

#define DISCONN   -3		//with this CODE a WORKER tellsto the MAIN that a CLIENT just DISCONNECTED
int done=NOTSET;	//Named PIPE(also FIFO) holds CIDs that completed an OP but arent DISCONNECTING		M <- WT
pthread_mutex_t DoneMutex=PTHREAD_MUTEX_INITIALIZER;

Storage* storage;	//Data Structure that holds all FILES + RWLOCK to access them
	
/*---------------------------------------------------WORKER----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

// called by OPEN/WRITE/APPEND becaus they may trigger a CAP LIMIT, old file/s must be sent back to the CLIENT 
int cacheAlg( int cid, Cmd cmd, size_t size){

	while( storage->numfiles>maxNumFiles || ((storage->capacity+size) > maxCapacity) ){
		
		File* victim=NULL;
		victim=rmvLastFile();
/*		storage->numfiles--;		// /!\ ALREADY CHECKED INSIDE RMVLASTFILE
		storage->capacity-=size;			*/

		if(victim==NULL) break;		//Edge case: the WRITE/APPEND needed to remove ALL FILES to make roome for the NEW one
		
		if( victim->cont!=NULL && victim->size>0){
			REPLY(CACHE);			//The CACHE ALG asks CID to RECEIVE the FILE only if it's not EMPTY
			WRITE(cid, &victim->size, sizeof(size_t));
			WRITE(cid, victim->cont, victim->size );
			WRITE(cid, victim->name, PATH_MAX);
			}
		
		LOGOP(CACHE,victim->name, &victim->size);
		fileDestroy( victim );
		}

	REPLY(OK);
	
	SUCCESS    return  0;
	ErrCLEANUP return -1; ErrCLEAN
	}

void* work(void* unused){				//ROUTINE of WORKER THREADS
	
	while(1){
		ErrLOCAL;
		bool Disconnecting=false;		//needed outside of the switch QUIT case, has to pass information that the CID is closing connection
		
		int cid=NOTSET;					//gets a ready ClientID (CID) from PENDING QUEUE
		while( (deqId(pending, &cid))==EMPTYLIST){	//if empty light BUSY WAIT with timer
			//nanosleep(&emptysleep, NULL);
			usleep(50000);
			}
		if(cid==NOTSET) ErrSHHH;		//ERR: CID UNCHANGED means that a FATAL ERR has happened inside deqId()
		if(cid==TERMINATE) break;		//TERMINATE SIGNAL sent from the MAIN THREAD when it's time to close shop
													
		Cmd cmd={0};					//gets CMD request from CID channel
		READ(cid, &cmd, sizeof(Cmd));
		
		//RDLOCK(&storage->lock);		//----vvvv----
		
		WRLOCK(&storage->lock);
		
		File* f=NULL;					//each OP (except READN) operates on a SPECIFIC FILE, this holds a REFERENCE to it ( getFile() )
		if(cmd.code!=READN){
		//  if(cmd.filename == NULL)	//Already checked in API call, could double check but not needed if all clients are forced to use the API		
			f=getFile(cmd.filename);
			}
			
		//RWUNLOCK(&storage->lock);	//----^^^^----
		
		if(DBG) printf("NUM F=%zu\n", storage->numfiles);
		
		switch(cmd.code){
			
			case OPEN: {
				//WRLOCK(&storage->lock);		//----vvvv----
				
				if( cmd.info & O_CREATE ){	//Client requested an O_CREATE openFile()
					if( f!=NULL){   REPLY(EXISTS); LOGOP(EXISTS,cmd.filename,NULL); break;	} //ERR: file already existing
					ErrNULL(  f=fileCreate(cmd.filename)  );	//ERR:	fatal malloc error while allocating space for new file (ENOMEM)
					ErrNEG1(  addNewFile(f)  );					//Successful CREATION
					// addNEwFile() INCREMENTS NUMFILES
					cmd.code=CREATE; LOGOP(OK,cmd.filename,NULL); cmd.code=OPEN;
					}	
				else{						//Client requested a normal openFile() on a file ALREADY PRESENT
					if( f==NULL){   REPLY(NOTFOUND); LOGOP(NOTFOUND,cmd.filename,NULL); break;	} //ERR: file not already existing/not found
					if( findId(f->openIds, cid) ){   REPLY(ALROPEN); LOGOP(ALROPEN,cmd.filename,NULL); break; } //ERR: cid already opened this file
					}
				
				ErrNEG1(  enqId(f->openIds, cid)  );			//Successful OPEN
				LOGOP(OK,cmd.filename,NULL);
				
				if( cmd.info & O_LOCK ){ 	//Client requested a O_LOCK openFile()
											// mind that the the OPEN with LOCK doesnt HANG like its counterpart and returns IMMEDIATELY if unsuccessful
					if( f->lockId!=NOTSET && f->lockId!=cid ){   REPLY(LOCKED); LOGOP(LOCKED,cmd.filename,NULL); break; }
					else f->lockId=cid;							//Successful LOCK
					cmd.code=LOCK; LOGOP(OK,cmd.filename,NULL); cmd.code=OPEN;
					}
					
				REPLY(OK);
				
				ErrNEG1(  cacheAlg(cid, cmd, (size_t)0)  );		//CACHE ALG enclosed in function bc is reused with the same code elsewhere
				
				//RWUNLOCK(&storage->lock);	//----^^^^----
				
				} break;
			
			case WRITE:				// All these OPERATIONS need a similar set of REQUIREMENTS
			case APPEND:			//		mainly because they all need to be LOCKED by the CLIENT to be successful
			case REMOVE:			//      (and this implies that the FILE EXISTS in FSS and is OPEN by CID)
			case UNLOCK: {
				//RDLOCK(&storage->lock);	//----vvvv----

				if( f==NULL){ REPLY(NOTFOUND); LOGOP(NOTFOUND,cmd.filename,NULL); break; }		//ERR: getFile() didnt FOUND it
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); LOGOP(NOTOPEN,cmd.filename,NULL); break; } //ERR: CID hasnt OPENED it
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); LOGOP(NOTLOCKED,cmd.filename,NULL); break; }	//ERR: CID doesnt currently HOLD the FILE LOCK
				
				//RWUNLOCK(&storage->lock);	//----^^^^----
				
				switch(cmd.code){
				
					case WRITE:{
						//RDLOCK(&storage->lock);	//----vvvv----
															//ERR: WRITE can only be performed on a NEWLY CREATED (EMPTY) FILE
						if( f->cont != NULL ){ REPLY(NOTEMPTY); LOGOP(EMPTY,cmd.filename,NULL); break; }
						
						//RWUNLOCK(&storage->lock);	//----^^^^----					
						
						REPLY(OK);
						
						size_t size=0;
						READ(cid, &size, sizeof(size_t));

																	//ERR: if the incoming FILE cant FIT the WHOLE SPACE, it cannot be accepted in
						if( size > maxCapacity ){ 
							REPLY(TOOBIG);
							LOGOP(TOOBIG,cmd.filename,NULL);
							ErrNEG1(  rmvThisFile(f)  );			//The BIG FILE is already OPENED and LOCKED even if EMPTY, must be REMOVED
							ErrNEG1(  fileDestroy(f)  );			//DEALLOCATES the REMOVED FILE
							break;
							}
						else REPLY(OK);
						
						//WRLOCK(&storage->lock); 	//----vvvv----
						
						ErrNEG1( cacheAlg(cid,cmd,size)  );
						
						void* cont=NULL;
						ErrNULL( cont=calloc(1, size) );
						READ(cid, cont, size);
						
						f->size=size;				//Actual WRITE, storing in the FILE NODE the received CONTs
						f->cont=cont;
						storage->capacity+=size;	//Updating STORAGE DIMENSIONS
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
						
						REPLY(OK);
						size_t* s=&size;			
						LOGOP(OK, cmd.filename, s);	//WARN: this threw 'always true' warning without this extra step
													//		(probably bc ADDR of LOCAL VAR cant be NULL)
						} break;
						
					case APPEND: {

						REPLY(OK);					//APPEND can be performed on ALL files, empty or not
				
						size_t size=0; 
						READ(cid, &size, sizeof(size_t));
						//WRLOCK(&storage->lock); 	//----vvvv----	//ERR: if the RESULTING FILE cant FIT the WHOLE SPACE the append must be canceled
						if( (f->size + size) > maxCapacity ){
							REPLY(TOOBIG);
							LOGOP(TOOBIG,cmd.filename,NULL);
							ErrNEG1(  rmvThisFile(f)  );			//The BIG FILE is already OPENED and LOCKED even if EMPTY, must be REMOVED
							ErrNEG1(  fileDestroy(f)  );			//DEALLOCATES the REMOVED FILE
							break;
							}
						else REPLY(OK);
		
				
						ErrNEG1(  cacheAlg(cid,cmd,size)  );					
						
						void* buf=NULL;
						ErrNULL( buf=calloc(1, size) );
						READ(cid, buf, size);
						
						if( f==NULL ){			//The CACHE ALGORITHM could have expelled the DEST FILE of the APPEND
							REPLY(NOTFOUND);	//	if that's the case no sense in going on
							LOGOP(NOTFOUND,cmd.filename,NULL);
							free(buf);
							break;
							}
					
						void* extendedcont=NULL;
						ErrNULL(  extendedcont=realloc( f->cont, f->size+size)  );	//REALLOCATING OG FILE to fit the new CONTs
						f->cont=extendedcont;
						memcpy( f->cont + f->size, buf, size );		//APPENDING new CONTENT starting from the end of the OG FILE
						f->size+=size;				//Updating FILE SIZE to reflect the operation
				
						storage->capacity+=size;	//Incrementing STORAGE SIZE
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
				
						free(buf);					//the BUFFER has been COPIED, dont need it anymore
				
						REPLY(OK);
						size_t* s=&size;
						LOGOP(OK, cmd.filename, s);	//WARN: this threw 'always true' warning without this extra step (bc ADDR of LOCAL VAR cant be NULL)
						} break;
						
					case REMOVE:				
						//WRLOCK(&storage->lock); 	//----vvvv----
						
						ErrNEG1(  rmvThisFile(f)  );		//Already have all requirements, proceding with REMOVE
						//storage->capacity-=size;			// /!\ ALREADY CHECKED INSIDE RMVLASTFILE
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
						
						LOGOP(OK, cmd.filename, &f->size);	
						ErrNEG1(  fileDestroy(f)  );		//DEALLOCATES the REMOVED FILE
						REPLY(OK);
						break;
						
					case UNLOCK:
						//WRLOCK(&storage->lock); 	//----vvvv----
						
						f->lockId=NOTSET;					//Already have all requirements, proceding with UNLOCK
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
						LOGOP(OK,cmd.filename,NULL);
						REPLY(OK);
						break;
					
					default: ;
					}
				} break;
			
			case READ:			//These 2 OPERATIONS instead require to NOT BEING LOCKED by OTHER CLIENTs
			case LOCK: {		//      (and this also implies that the FILE already EXISTS and that its OPEN by CID)
			
				//RDLOCK(&storage->lock);				//----vvvv----
			
				if( f==NULL){ REPLY(NOTFOUND); LOGOP(NOTFOUND,cmd.filename,NULL); break; }
		
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); LOGOP(NOTOPEN,cmd.filename,NULL); break; }
		
				if( f->lockId!=NOTSET && f->lockId!=cid ){ REPLY(LOCKED); LOGOP(LOCKED,cmd.filename,NULL); break; } //ERR: FILE LOCKED by OTHER CID
				
				switch(cmd.code){
				
					case LOCK:										//ERR: No sense in GETTING the LOCK AGAIN if the CLIENT already owns it
						if( f->lockId==cid ){ REPLY(ALRLOCKED); LOGOP(ALRLOCKED,cmd.filename,NULL); break; }
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
						
						//WRLOCK(&storage->lock); 	//----vvvv----
						f->lockId=cid;							//Proceeding with the LOCKING
						//RWUNLOCK(&storage->lock);	//----^^^^----
		
						LOGOP(OK,cmd.filename,NULL);
						REPLY(OK);
						break;
					
					case READ:
						if( f->cont==NULL ){ REPLY(EMPTY); LOGOP(EMPTY,cmd.filename,NULL); break; }		//ERR: no sense in READING an EMPTY file
						
						REPLY(OK);
				
						WRITE(cid, &f->size, sizeof(size_t));	//Sending the requested DATA back
						WRITE(cid, f->cont, f->size);
						
						//RWUNLOCK(&storage->lock);	//----^^^^----
				
						LOGOP(OK,cmd.filename,&f->size);
						break;
					
					default: ;
					}
				} break;
						
			case CLOSE:{
				//RDLOCK(&storage->lock);		//----vvvv----
				if( f==NULL){   REPLY(NOTFOUND); LOGOP(NOTFOUND,cmd.filename,NULL); break; }		//ERR: file not found
				//RWUNLOCK(&storage->lock);	//----^^^^----

				//WRLOCK(&storage->lock);		//----vvvv----				
				if( f->lockId==cid) f->lockId=NOTSET;		//Eventual UNLOCK before closing
				
				if( findRmvId(f->openIds, cid)  ){  REPLY(OK);  LOGOP(OK,cmd.filename,NULL); }
				else {  REPLY(NOTOPEN); LOGOP(NOTOPEN,cmd.filename,NULL); }
															//ERR: if the FILE wasnt already OPEN by CID, there was no sense in CLOSING it
				//RWUNLOCK(&storage->lock);	//----^^^^----
				} break;		

			case READN:{
				REPLY(OK);		//READN basically CANT FAIL by design (worst case scenario other than FATAL SC ERROR is that it reads 0 files)
				
				//RDLOCK(&storage->lock);		//----vvvv----

				File* curr=storage->last;
				
				int n=( cmd.info<=0  ?  storage->numfiles  :  cmd.info );
				int n_read=0;
				
				while( n_read < n ){	//for each of N RANDOM FILES
					n_read++;									//increments READ COUNT
					if( curr==NULL) break;										//If FSS is completely EMPTY get out
					
					if( curr->lockId!=NOTSET && curr->lockId!=cid ) continue;	//READN cannot read LOCKED files
					
					if( curr->cont==NULL || curr->size<=0) continue;			//Also no sense in reading EMPTY files
					
					REPLY(ANOTHER);								//Tells the CLIENT that there's ANOTHER FILE to be READ
					WRITE(cid, &curr->size, sizeof(size_t));
					WRITE(cid, curr->cont, curr->size);
					WRITE(cid, curr->name, PATH_MAX);			//Must also send his NAME other than SIZE and CONT
					
					LOGOP(ANOTHER, curr->name , &curr->size);		
					curr=curr->prev;							//CONTINUES TRAVERSING the LIST
					}

				//RWUNLOCK(&storage->lock);	//----^^^^----
				
				REPLY(OK);
				LOGOP(OK, NULL, NULL);
				break;
				} break;			
				
	
			case IDLE:
			case QUIT:{

				//WRLOCK(&storage->lock);		//----vvvv----				
				File* curr=storage->last;
			
				for( int i=0; i < storage->numfiles; i++){	//FOR EACH FILE in the FSS:
					if( curr==NULL) break;							//safety check for END of the LIST		
					
					findRmvId(curr->openIds, cid);					//UNLOCKS all files locked by CID	
					if( curr->lockId==cid ) curr->lockId=NOTSET;	//CLOSES all files open by cid 
										
					curr=curr->prev;								//Traverse the FSS				
					}
				Disconnecting=true;		//Sets VAR so that the CID isnt passed back to the MAIN THREAD

				//RWUNLOCK(&storage->lock);	//----^^^^----

				} break;
			
			default:	//Should be unreachable
				fprintf(stderr, "FSS? More like FFS! Unknown cmd code: %d\n", cmd.code);
				break;
			}
		
		RWUNLOCK(&storage->lock);
		
		if(Disconnecting){
			LOG("WORK: CLIENT %d DISCONNECTED\n", cid);
			ErrNEG1(  close(cid)  );
			cid=DISCONN;						//A CODE is SENT back to the MAIN THREAD instead of the DISCONNECTED CLIENT ID
			}									//		(decrements the number of active clients in main)
		
		LOCK(&DoneMutex);
		WRITE( done, &cid, sizeof(int) );		//if it isnt DISCONNECTING the CLIENT ID is SENT BACK to the MAIN THREAD
		UNLOCK(&DoneMutex);
		
		if(DBG){
			printf("STORAGE:  ----------------------------\n");		//DBG: Prints the list of FILES currently in the FSS
			storagePrint();
			printf("--------------------------------------\n\n");
			}
		fflush(stdout);
		continue;
	ErrCLEANUP 
		REPLY(FATAL); 
		kill(0, SIGINT);
	ErrCLEAN
		}
	return NULL;
	}
	
/*-----------------------------------------------------MAIN----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char* argv[]){
	
	/*------------------------Signals--------------------------*/
	sigset_t mask, oldmask;							//SIGNAL MASKS
	ErrNEG1(  sigemptyset(&mask)  );   
	ErrNEG1(  sigaddset(&mask, SIGINT)  ); 
	ErrNEG1(  sigaddset(&mask, SIGQUIT) );
	ErrNEG1(  sigaddset(&mask, SIGHUP)  );
	
	ErrNEG1(  sigaddset(&mask, SIGPIPE) );
	ErrNZERO(  pthread_sigmask(SIG_BLOCK, &mask, &oldmask)  );	//blocks signals during SERVER INITIALIZATION
	ErrNEG1(  sigaddset(&oldmask, SIGPIPE)  );					//blocks SIGPIPE during the wole EXECUTION (both on mask & oldmask)
	
	struct sigaction off={0}, soft={0};
	off.sa_handler=&handleoff;
	soft.sa_handler=&handlesoft;
	off.sa_mask=soft.sa_mask=mask;					//blocks signals inside SIG HANDLER function
	
	ErrNEG1(  sigaction(SIGINT,  &off, NULL)  );				//assigns each SIG to the its SIG HANDLER
	ErrNEG1(  sigaction(SIGQUIT, &off, NULL)  );
	ErrNEG1(  sigaction(SIGHUP, &soft, NULL)  );
	
	/*------------------------Config---------------------------*/
	
	char socketpathn[PATH_MAX]="./serversocket";
	char fifopathn[PATH_MAX]=  "./serverdone";
	char logpathn[PATH_MAX]=   "./serverlog.txt";
	int maxWThreads=4;
	int backlog=32;
	
	if(argc==2){	//CONFIG FILE SPECIFIED
		char* configpathn=argv[1];
		FILE* config=NULL;
		ErrNULL(  config=fopen(configpathn, "r")  );
		
		char line[CONFIGMAXLINE]={'\0'};
		
		while( fgets(line, CONFIGMAXLINE, config) != NULL ){
			char dummy[CONFIGMAXLINE]={'\0'};
			if( sscanf(line, " %s",   dummy )==EOF) continue;	// blank line
			if( sscanf(line, " %[#]", dummy )==1  ) continue;	// comment
			if( sscanf(line, " SOCKETPATHNAME = %s",socketpathn) !=0 ) continue;
			if( sscanf(line, " FIFOPATHNAME = %s",    fifopathn) !=0 ) continue;
			if( sscanf(line, " LOGPATHNAME = %s",     logpathn ) !=0 ) continue;
			if( sscanf(line, " MAXNUMFILES = %d", &maxNumFiles ) !=0 ) continue;
			if( sscanf(line, " MAXCAPACITY = %zu",&maxCapacity ) !=0 ) continue;
			if( sscanf(line, " MAXWTHREADS = %d", &maxWThreads ) !=0 ) continue;
			if( sscanf(line, " BACKLOG = %d",         &backlog ) !=0 ) continue;
			fprintf(stderr, "Warning: Unknown config file parameter, skipped\n");
			}
		ErrNZERO(  fclose(config)  )
		}
	else if(argc>2){	//more than 1 ARG
		fprintf(stderr, "Error: Only 1 argument allowed, the PATHNAME of the CONFIG FILE\n");
		ErrSHHH;
		}
	
		/*-------------------------Log-----------------------------*/
	
	#ifdef DEBUG
	DBG=true;
	#endif
	
	//FILE* Log=NULL;  			//GLOBAL
	ErrNULL(  Log=fopen( logpathn, "w")  );
	
	LOG("LOGPATHNAME=%s\nFIFOPATHNAME=%s\nSOCKETPATHNAME=%s\nMAXNUMFILES=%d\nMAXCAPACITY=%zu\nMAXWTHREADS=%d\nBACKLOG=%d\n\n", 	\
					logpathn,		fifopathn,       socketpathn,    	maxNumFiles,    maxCapacity,    maxWThreads, backlog);
	
	/*------------------------Socket---------------------------*/
	int sid=NOTSET;
	ErrNEG1(  sid=socket(AF_UNIX, SOCK_STREAM, 0)  );	//SID: server id (fd), SOCKET assigns it a free channel
	
	struct sockaddr_un saddr;							//saddr: server address, needed for BIND
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;							//adds domain and pathname to the server address structure
	strcpy(saddr.sun_path, socketpathn);
	
	fprintf(stderr, "SOCKETNAME: %s\n", socketpathn);				
	
	ErrNEG1(  bind(sid, (struct sockaddr*) &saddr, SUN_LEN(&saddr))  );	//BIND: NAMING of the opened socket
	
	ErrNEG1( listen(sid, backlog)  );							//sets socket in LISTEN mode
	
	LOG("MAIN: SOCKET OPENED\n");
	
	/*-------------------Pending-queue/Done-pipe---------------*/
	ErrNULL(  pending=idListCreate()  );				//PENDING queue
	
	if( mkfifo(fifopathn, 0777) != 0){				//DONE named pipe
		if( errno != EEXIST){
			perror("Error: couldnt create named pipe\n");
			ErrSHHH;
			}
		}
	ErrNEG1(  done=open(fifopathn, O_RDWR)  );
	
	/*-------------------------FD-sets-------------------------*/
	fd_set all, rdy;								//ALL set of FDs controlled by select()
	FD_ZERO(&all);

	FD_SET(sid, &all);									//adding SID to ALL
	int maxid=sid;
	
	FD_SET(done, &all);									//adding DONE to ALL
	if(done > maxid) maxid=done;
	
	/*---------------------FSS-/-Threads-----------------------*/	
	
	ErrNEG1(  storageCreate()  );		//allocates an empty FILE STORAGE
		
	pthread_t* tid=NULL;				//allocated ARRAY that holds THREAD IDs
	ErrNULL(  tid=malloc( (maxWThreads+1)*sizeof(pthread_t))  );
		
	for(int i=0; i<maxWThreads; i++){
		CREATE(&tid[i], NULL, work, NULL );		//WORKER THREAD CREATION
		LOG("MAIN: CREATED WORKER THREAD #%d\n", i);
		}

	LOG("MAIN: SERVER READY\n");
	
	
	/*---------------------Manager-loop------------------------*/
	int activeCid=0;
	
	while( Status!=OFF ){
		rdy=all;
		int nrdy=0;	//select returns the number of rdy fds				//select "destroys" the fd that arent rdy
		/*nrdy=*/PSELECT(FD_SETSIZE, &rdy, NULL, NULL, NULL, &oldmask);		//PSELECT macro HANDLES ERR
		
		if( nrdy>0){							//if <=0 the select has been triggered by one of the installed signals and the FD cycle is skipped
			for(int i=0; i<=maxid; i++){
				if( FD_ISSET( i, &rdy) ){
					if( i==sid && Status==ON){				//the server socket (sid) has a pending request and is ready to accept it
						int cid;							//	but only if the server is not in SOFT QUIT mode (Status==NOCONN)
						ACCEPT(cid, sid, NULL, NULL );		//ACCEPT macro HANDLES ERR
						FD_SET(cid, &all);
						if(cid>maxid) maxid=cid;
						activeCid++;
						LOG("MAIN: ACCEPTED NEW CLIENT %d\n", cid);
						}
					else if( i==done){			//the named pipe containing the finished requests has a cid that must return in the controlled set
						int cid;
						
						LOCK(&DoneMutex);
						READ(done, &cid, sizeof(int));
						UNLOCK(&DoneMutex);
						
						if( cid==DISCONN ){
							activeCid--;
							}
						else FD_SET(cid, &all);
							//printf("received back client %d\n", cid);
						}
					else{									//one of the clients has something that the server has to read
						FD_CLR( i, &all);
						enqId(pending, i);
						}				
					}
				}
			}
		//LOG("MAIN:\t\t\tNCID:%d\n", activeCid);
		if( Status==SOFT && activeCid==0) break; 
		}

	/*---------------------Cleanup-----------------------------*/

	for( int i=0; i<maxWThreads; i++)
		enqId(pending, TERMINATE);
		
	for( int i=0; i<maxWThreads; i++){
		ErrERRNO(  pthread_join( tid[i], NULL)  );
		LOG("MAIN: THREAD #%d JOINED\n", i);
		}
	free(tid);
	
	printf("SOCKET NAME: %s\n", socketpathn);
	printf("DONE   NAME: %s\n", fifopathn);
	
	ErrNEG1(  storageDestroy()       );
	ErrNEG1(  unlink(fifopathn)      );
	ErrNEG1(  close(done)            );
	ErrNEG1(  idListDestroy(pending) );
	ErrNEG1(  unlink(socketpathn)    );
	ErrNEG1(  close(sid)             );
	LOG("MAIN: SERVER SUCCESSFULLY CLOSED\n");
	ErrNZERO(  fclose(Log)  );
	return 0;
	
ErrCLEANUP
	fflush(stderr);
	if(storage) storageDestroy();
	unlink(fifopathn);
	if(done!=NOTSET) close(done);
	if(pending) idListDestroy(pending);
	unlink(socketpathn);
	if(sid!=NOTSET) close(sid);
	LOG("MAIN: SERVER ERROR (CLEANUP DONE)\n");
	fflush(Log);
	if(Log!=NULL) fclose(Log);
	exit(EXIT_FAILURE);
ErrCLEAN
	}
