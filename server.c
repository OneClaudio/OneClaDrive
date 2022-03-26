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

#include "./comm.h"
#include "./utils.h"
#include "./idlist.h"
#include "./filestorage.h"
#include "./errcheck.h"

#define OFF 0x00
#define SOFT 0x01
#define ON 0x02

volatile sig_atomic_t Status=ON;

void handlesoft(int unused){
	Status=SOFT;
	printf("\nSOFT quit signal received: NO NEW CONNECTIONS ALLOWED\n");
	}

void  handleoff(int unused){
	Status=OFF;
	printf("\nHARD quit signal received: NO FURTHER REQUESTS WILL BE EXECUTED\n");
	}

/*
#define BIND( id, addr, l) errno=0;				\
	if( bind( id, addr, l) != 0 ){				\
		perror("Error: couldnt name socket\n"); \
		exit(EXIT_FAILURE);						\
		}
		
#define LISTEN( id, bl) errno=0;									\
	if( listen( id, bl) != 0 ){										\
		perror("Error: couldnt listen on the specified socket\n"); 	\
		exit(EXIT_FAILURE);											\
		}

#define WRITE( id, addr, l) errno=0;								\
	if( writen( id, addr, l) <0 ){									\
		perror("Error during write\n");								\
		break;														\
		}

#define READ( id, addr, l) errno=0;									\
	if( readn( id, addr, l) <0 ){									\
		perror("Error during read\n");								\
		break;														\
		}

#define CREATE( t, att, f, arg)		if(pthread_create( t, att, f, arg) != 0){				\
	fprintf(stderr, "Error while creating thread %s at %s:%d\n", #t, __FILE__, __LINE__);	\
	perror(NULL);																			\
	pthread_exit(NULL);																		\
	}

#define JOIN( t, att)	if(pthread_join( t, att) != 0){										\
	fprintf(stderr, "Error while waiting thread %s at %s:%d\n", #t, __FILE__, __LINE__);	\
	perror(NULL);																			\
	exit(errno);																			\
	}
	
#define RDLOCK(l)      if (pthread_rwlock_rdlock(l) != 0){								\
	fprintf(stderr, "Error during rdlock: '%s' at %s:%d\n", #l, __FILE__, __LINE__);	\
	perror(NULL);																		\
    pthread_exit((void*)EXIT_FAILURE);			   										\
  	}

#define WRLOCK(l)      if (pthread_rwlock_wrlock(l) != 0){								\
	fprintf(stderr, "Error during wrlock: '%s' at %s:%d\n", #l, __FILE__, __LINE__);	\
	perror(NULL);																		\
    pthread_exit((void*)EXIT_FAILURE);			   										\
  	}
 
#define UNLOCK(l)      if (pthread_rwlock_unlock(l) != 0){								\
	fprintf(stderr, "Error during unlock: '%s' at %s:%d\n", #l, __FILE__, __LINE__);	\
	perror(NULL);																		\
    pthread_exit((void*)EXIT_FAILURE);			   										\
  	}
*/
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

#define REPLY( MSG ){						\
	Reply reply;							\
	memset( &reply, 0, sizeof(Reply) );		\
	reply=MSG;								\
	WRITE( cid, &reply, sizeof(Reply) );	\
	}

#define FIFOPATHN	"./done"
#define SOCKETPATHN "./server_sol"	//Server socket pathname		//TODO move to config file
#define MAXNUMFILES 100
#define MAXCAPACITY 1000000			//1MB
#define MAXTHREADS 4
#define BACKLOG 10

pthread_t tid[MAXTHREADS-1];		//WORKER THREADS

#define TERMINATE -2



IdList* pending;	//thread safe QUEUE for fds rdy for the worker threads		M -> WTs
int done=NOTSET;				//named PIPE for fds finished by the worker threads	  WTs -> M

Storage* storage;
	
/*---------------------------------------------------WORKER----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

void* work(void* unused){			//ROUTINE of WORKER THREADS
	
	while(1){
		bool quit=false;
		
		int cid=NOTSET;					//tries to get CID from QUEUE
		while( (deqId(pending, &cid))==EMPTYLIST){	//if empty BUSY WAIT with timer
			usleep(50000);
			}
		if(cid==NOTSET) ErrSHHH;		//that means that a FATAL LOCK ERR has happened inside deqId()
		if(cid==TERMINATE) break;
													
		Cmd cmd={0};						//gets CMD request from CIDs channel
		//memset( &cmd, 0, sizeof(Cmd) );	
		READ(cid, &cmd, sizeof(Cmd));
		
		
		ErrNZERO(  pthread_rwlock_wrlock(&storage->lock)  );	//TODO move inside with rd/wr differentiation
		
		switch(cmd.code){
			
			case (OPEN):{
				ErrLOCAL
			/*	if(cmd.filename == NULL)	//Already checked in API call//  */
				printf("starting OPEN file '%s'\n", cmd.filename);
						
				File* f=getFile(cmd.filename);
				
				if( cmd.info & O_CREATE ){	//Client requested an O_CREATE openFile()
					if( f!=NULL){								//ERR: file already existing
						REPLY(EXISTS);
						break;
						}
						
					ErrNULL(  f=fileCreate(cmd.filename)  );	//ERR:	fatal malloc error while allocating space for new file (ENOMEM)
						
					ErrNEG1(  addNewFile(f)  );					//Successful CREATION	
					}	
				else{		//Client requested a normal openFile()
					if( f==NULL){
						REPLY(NOTFOUND);						//ERR: file not already existing/not found
						break;
						}
					}
				
				if( findId(f->openIds, cid) ) REPLY(ALROPEN);	
				ErrNEG1(  enqId(f->openIds, cid)  );				//Successful OPEN
				
				if( cmd.info & O_LOCK ){
					if( f->lockId!=NOTSET && f->lockId!=cid ){ REPLY(LOCKED); break; }
					else f->lockId=cid;			//Successful LOCK
					}
					
				REPLY(OK);
				
				if( storage->numfiles == MAXNUMFILES ){
					REPLY(CACHE);
					
					File* victim=NULL;
					ErrNULL( victim=rmvLastFile() );
					
					WRITE(cid, &victim->size, sizeof(size_t));
					WRITE(cid, victim->cont, victim->size );
					WRITE(cid, victim->name, PATH_MAX);
					
					fileDestroy( victim );					
					}
					
				REPLY(OK);
				break;

			ErrCLEANUP
				exit(EXIT_FAILURE);
			ErrCLEAN
				} break;
					
			
			
			
			case (CLOSE):{
				printf("CLOSE file '%s'\n", cmd.filename);
				File* f=getFile(cmd.filename);
				if( f==NULL){				//ERR: file not found
						REPLY(NOTFOUND);
						break;
						}
				
				if( f->lockId==cid) f->lockId=NOTSET;
				
				if( ! findRmvId(f->openIds, cid)  ){  REPLY(NOTOPEN);  }
				else REPLY(OK);
				
				} break;

			
			
			
			case (WRITE):{
ErrLOCAL		
				printf("starting WRITE file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				if( f->cont != NULL ){ REPLY(NOTEMPTY); break; }
				
				REPLY(OK);
				
				printf("receiving files cont\n");
				size_t size=0;
				READ(cid, &size, sizeof(size_t));
				printf("size: %ld\n", size);
				
				void* cont=NULL;
				ErrNULL( cont=calloc(1, size) );
				READ(cid, cont, size);
				//printf("cont: %s", (char *)cont );
				
				if( size > MAXCAPACITY ){ REPLY(TOOBIG); break; }
				
				while( storage->numfiles == MAXNUMFILES  ||  (storage->capacity+size) > MAXCAPACITY ){
					REPLY(CACHE);
					
					File* victim=NULL;
					ErrNULL( victim=rmvLastFile() );
					
					WRITE(cid, &victim->size, sizeof(size_t));
					WRITE(cid, victim->cont, victim->size );
					WRITE(cid, victim->name, PATH_MAX);
					
					fileDestroy( victim );
					}
				
				f->size=size;
				f->cont=cont;
				storage->numfiles++;
				storage->capacity+=size;
				
				REPLY(OK);
				
				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				} break;				

			case (APPEND):{
ErrLOCAL
				printf("starting APPEND to file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				REPLY(OK);
				
				size_t size=0;
				READ(cid, &size, sizeof(size_t));
				
				void* buf=NULL;
				ErrNULL( buf=calloc(1, size) );
				READ(cid, buf, size);
				
				if( size > MAXCAPACITY ){ REPLY(TOOBIG); break; }
				
				while( storage->numfiles == MAXNUMFILES  ||  (storage->capacity+size) > MAXCAPACITY ){
					REPLY(CACHE);
					
					File* victim=NULL;
					ErrNULL( victim=rmvLastFile() );
					
					WRITE(cid, &victim->size, sizeof(size_t));
					WRITE(cid, victim->cont, victim->size );
					WRITE(cid, victim->name, PATH_MAX);
					
					fileDestroy( victim );
					}
					
				void* extendedcont=NULL;
				ErrNULL(  extendedcont=realloc( f->cont, f->size+size)  );
				f->cont=extendedcont;
				memcpy( f->cont+f->size, buf, size );
				f->size+=size;
				printf("cont: %s", (char *) f->cont );
				
				storage->capacity+=size;
				
				free(buf);
				
				REPLY(OK);
				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				} break;			




			case (READ):{
ErrLOCAL
				printf("starting READ file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId!=NOTSET && f->lockId!=cid ){ REPLY(LOCKED); break; }
				
				if( f->cont==NULL ){ REPLY(EMPTY); break; }		//cant READ EMPTY files
				
				REPLY(OK);
				
				WRITE(cid, &f->size, sizeof(size_t));
				WRITE(cid, f->cont, f->size);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				} break;
			
			
			
			
			case (READN):{
ErrLOCAL
				printf("starting READN\n");	
				REPLY(OK);
				
				File* curr=storage->last;
				
				int n=( cmd.info<=0  ?  storage->numfiles  :  cmd.info );
				int n_read=0;
				
				while( n_read < n ){
					if( curr==NULL) break;
					
					if( curr->lockId!=NOTSET && curr->lockId!=cid ) continue;	//cant READ LOCKED files
					
					if( curr->cont==NULL ) continue;							//cant READ EMPTY files
					
					REPLY(ANOTHER);
					WRITE(cid, &curr->size, sizeof(size_t));
					WRITE(cid, curr->cont, curr->size);
					WRITE(cid, curr->name, PATH_MAX);
					
					n_read++;
					curr=curr->prev;					
					}
				
				REPLY(OK);
				
				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;			
		
		
		
		
			case (REMOVE):{
ErrLOCAL
				printf("starting REMOVE file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
					
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				ErrNEG1(  rmvThisFile(f)  );
				
				fileDestroy(f);
				
				REPLY(OK);
				
				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
				
				
				
				
			case (LOCK):{
ErrLOCAL
				printf("LOCK file '%s'\n", cmd.filename);
				
				File* f=getFile(cmd.filename);
				
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId( f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId!=NOTSET && f->lockId!=cid ){ REPLY(LOCKED); break; }
				else if( f->lockId==cid ){	REPLY(ALRLOCKED); break; }
				
				
				f->lockId=cid;
				
				REPLY(OK);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
				
				
				
			
			case (UNLOCK):{
ErrLOCAL
				printf("starting UNLOCK file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId( f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){ REPLY(NOTLOCKED); break; }
				
				f->lockId=NOTSET;
				
				REPLY(OK);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
			
			case (IDLE):
			case (QUIT):{
				printf("quitting CONN\n");	
				File* curr=storage->last;
			
				for( int i=0; i < storage->numfiles; i++){	//close all files open by cid, unlock all files locked by cid	
					if( curr==NULL) break;	//safety check for empty list		
					
					findRmvId(curr->openIds, cid);
					if( curr->lockId==cid ) curr->lockId=NOTSET;
										
					curr=curr->prev;					
					}
				quit=true;			
				}
				break;
			
			default:
				fprintf(stderr, "Come sei finito qui? cmd code: %d\n", cmd.code);
				break;		
			}
		ErrNZERO(  pthread_rwlock_unlock(&storage->lock)  );
		
		if( !quit ){  WRITE( done, &cid, sizeof(int) );  }
		else printf("Closing connection with client %d\n", cid);   /*	Client CID successfully unconnected */
		
		printf("STORAGE:  ----------------------------\n");
		storagePrint();
		printf("--------------------------------------\n\n");
		fflush(stdin);
		}

	SUCCESS    return NULL;
	ErrCLEANUP return NULL; ErrCLEAN
	}

int spawnworker(){								//worker threads SPAWNER
	for(int i=0; i<MAXTHREADS-1; i++){
		CREATE(&tid[i], NULL, work, NULL);		//WORKER THREAD
		printf("Created worker thread %d\n", i);
		}

	return 0;
ErrCLEANUP
	return -1;
ErrCLEAN
	}
	
/*-----------------------------------------------------MAIN----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int main(){

	unlink(SOCKETPATHN);								//remove server socket if already exists FIXME
	
	
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
	ErrNEG1(  sigaction(SIGQUIT,&soft, NULL)  );	// /!\ temp quit in softquit bc it has a keyboard shortcut
	ErrNEG1(  sigaction(SIGHUP, &soft, NULL)  );
	
	/*------------------------Socket---------------------------*/
	int sid=NOTSET;
	ErrNEG1(  sid=socket(AF_UNIX, SOCK_STREAM, 0)  );	//SID: server id (fd), SOCKET assigns it a free channel
	
	struct sockaddr_un saddr;							//saddr: server address, needed for BIND
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;							//adds domain and pathname to the server address structure
	strcpy(saddr.sun_path, SOCKETPATHN);				
	
	ErrNEG1(  bind(sid, (struct sockaddr*) &saddr, SUN_LEN(&saddr))  );	//BIND: NAMING of the opened socket
	
	ErrNEG1( listen(sid, BACKLOG)  );							//sets socket in LISTEN mode
	
	printf("Server started\n");
	
	/*-------------------Pending-queue/Done-pipe---------------*/
	ErrNULL(  pending=idListCreate()  );				//PENDING queue
	
	if( mkfifo("./done", 0777) != 0){				//DONE named pipe
		if( errno != EEXIST){
			perror("Error: couldnt create named pipe\n");
			ErrFAIL;
			}
		}
	ErrNEG1(  done=open("./done", O_RDWR)  );
	
	/*-------------------------FD-sets-------------------------*/
	fd_set all, rdy;								//ALL set of FDs controlled by select()
	FD_ZERO(&all);

	FD_SET(sid, &all);									//adding SID to ALL
	int maxid=sid;
	
	FD_SET(done, &all);									//adding DONE to ALL
	if(done > maxid) maxid=done;
	
	/*---------------------FSS-/-Threads-----------------------*/	
	ErrNEG1(  storageCreate()  );		//allocates an empty FILE STORAGE
	
	ErrNEG1(  spawnworker()  );			//spawns WORKER THREADS

	printf("Server ready\n");
	
	
	/*---------------------Manager-loop------------------------*/
	
	while( Status!=OFF ){
		rdy=all;
		int nrdy=0;	//select returns the number of rdy fds				//select "destroys" the fd that arent rdy
		/*nrdy=*/PSELECT(FD_SETSIZE, &rdy, NULL, NULL, NULL, &oldmask);		// pselect( n, read, write, err, timeout, signals)
		printf("select done\n");
		
		if( nrdy>0){							//if <=0 the select has been triggered by one of the installed signals and the FD cycle is skipped
			for(int i=0; i<=maxid; i++){
				if( FD_ISSET( i, &rdy) ){
					printf("----------\nfd: %d\n", i);
					if( i==sid && Status==ON){				//the server socket (sid) has a pending request and is ready to accept it
						int cid;							//	but only if the server is not in SOFT QUIT mode (Status==NOCONN)
						ACCEPT(cid, sid, NULL, NULL);
						FD_SET(cid, &all);
						if(cid>maxid) maxid=cid;
						printf("accepted client %d\n", cid);
						}
					else if( i==done){			//the named pipe containing the finished requests has a cid that must return in the controlled set
						int cid;
						READ(done, &cid, sizeof(int));
						FD_SET(cid, &all);
						printf("received back client %d\n", cid);
						}
					else{									//one of the clients has something that the server has to read
						FD_CLR( i, &all);
						enqId(pending, i);
						printf("serving client %d\n", i);
						}				
					}
				}
			}
		}		//TODO SOFT QUIT way to break out of loop

	for( int i=0; i<MAXTHREADS-1; i++)
		enqId(pending, TERMINATE);
		
	for( int i=0; i<MAXTHREADS-1; i++)
		ErrERRNO(  pthread_join( tid[i], NULL)  );
	
	ErrNEG1(  storageDestroy()       );
	ErrNEG1(  unlink(FIFOPATHN)      );
	ErrNEG1(  close(done)            );
	ErrNEG1(  idListDestroy(pending) );
	ErrNEG1(  unlink(SOCKETPATHN)    );
	ErrNEG1(  close(sid)             );
										
	printf("\nServer successfully closed\n");
	return 0;
	
ErrCLEANUP
	if(storage) storageDestroy();
	unlink(FIFOPATHN);
	if(done!=NOTSET) close(done);
	if(pending) idListDestroy(pending);
	unlink(SOCKETPATHN);
	if(sid!=NOTSET) close(sid);
	exit(EXIT_FAILURE);
ErrCLEAN
	}
