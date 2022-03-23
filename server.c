#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>			//toupper()
#include <sys/select.h>
#include <pthread.h>
#include <linux/limits.h>	//PATH_MAX
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <signal.h>

#include "./comm.h"
#include "./utils.h"
#include "./idlist.h"
#include "./filestorage.h"
#include "./errcheck.h"

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

#define ACCEPT( cid, sid, addr, l) errno=0;					\
	cid=accept( sid, addr, l);								\
	if(cid<0){												\
		if( errno==EAGAIN) continue;						\
		else ErrFAIL;										\
		}

#define REPLY( MSG ){						\
	Reply reply;							\
	memset( &reply, 0, sizeof(Reply) );		\
	reply=MSG;								\
	WRITE( cid, &reply, sizeof(Reply) );	\
	}
	
#define SPATHNAME "./server_sol"	//Server socket pathname

#define MAXNUMFILES 100
#define MAXCAPACITY 1000000		//1MB

#define MAXTHREADS 4
pthread_t tid[MAXTHREADS-1];		//WORKER THREADS



IdList* pending;	//thread safe QUEUE for fds rdy for the worker threads		M -> WTs
int done;				//named PIPE for fds finished by the worker threads		  WTs -> M

Storage* storage;

volatile sig_atomic_t quit=0;		//QUIT SIGNAL handler
void sighandler(int unused){
    quit=1;
	}
	
/*---------------------------------------------------WORKER----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

void* work(void* unused){			//ROUTINE of WORKER THREADS
	
	while(1){
		int cid=-1;
		
		while( deqId(pending, &cid) != 0) //TODO	//gets CID from QUEUE
			usleep(50000);
		
		//printf("Got CID!\n");
		
		Cmd cmd;
		memset( &cmd, 0, sizeof(Cmd) );	
		READ(cid, &cmd, sizeof(Cmd));
		//printf("Got CMD\n");
		
		bool quit=false;
		
		//Reply reply;
		
		ErrNZERO(  pthread_rwlock_wrlock(&storage->lock)  );
		
		switch(cmd.code){
			
			case (OPEN):{
ErrLOCAL
			/*	if(cmd.filename == NULL)	//Already checked in API call//  */
				printf("starting OPEN file '%s'\n", cmd.filename);
						
				File* f=getFile(cmd.filename);

				
				if( cmd.info & O_CREATE ){					//Client requested an O_CREATE openFile()
					
					if( f!=NULL){								//ERR: file already existing
						REPLY(EXISTS);
						break;
						}
						
					ErrNULL(  f=fileCreate(cmd.filename)  );	//ERR:	fatal malloc error while allocating space for new file (ENOMEM)
						
					ErrNEG1(  addNewFile(f)  );				//Successful CREATION	
					}	
				else{										//Client requested a normal openFile()
					if( f==NULL){ REPLY(NOTFOUND); break; }		//ERR: file not already existing/not found
					}
				
				if( findId(f->openIds, cid) ) REPLY(ALROPEN);	
				ErrNEG1(  enqId(f->openIds, cid)  );				//Successful OPEN
				
				if( cmd.info & O_LOCK ){
					if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
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
				printf("done OPEN file '%s'\n", cmd.filename);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
					
			
			
			
			case (CLOSE):{
ErrLOCAL		
				printf("CLOSE file '%s'\n", cmd.filename);
				File* f=getFile(cmd.filename);
				if( f==NULL){				//ERR: file not found
						REPLY(NOTFOUND);
						break;
						}
				
				if( findRmvId(f->openIds, cid)==-1  ){  REPLY(NOTOPEN);  }
				else REPLY(OK);
				
				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;

			
			
			
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
				printf("cont: %s", (char *) cont );
				
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
				}
				break;				

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
				}
				break;			




			case (READ):{
ErrLOCAL
				printf("starting READ file '%s'\n", cmd.filename);	
				File* f=getFile(cmd.filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( !findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
				
				REPLY(OK);
				
				WRITE(cid, &f->size, sizeof(size_t));
				WRITE(cid, f->cont, f->size);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
			
			
			
			
			case (READN):{
ErrLOCAL
				printf("starting READN\n");	
				REPLY(OK);
				
				File* curr=storage->last;
				
				int n=cmd.info;
				
				for( int i=0; i < (n<=0 ? storage->numfiles : n); i++){
					if( curr==NULL) break;
					
					if( curr->lockId!=-1 && curr->lockId!=cid ) break;
					
					REPLY(ANOTHER);
					WRITE(cid, &curr->size, sizeof(size_t));
					WRITE(cid, curr->cont, curr->size);
					WRITE(cid, curr->name, PATH_MAX);
					
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
				
				if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
				else if( f->lockId==cid ){	REPLY(ALRLOCKED); break; }
				
			/*	if( f->lockId==-1) */
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
				
				f->lockId=-1;
				
				REPLY(OK);

				break;
ErrCLEANUP
				exit(EXIT_FAILURE);
ErrCLEAN
				}
				break;
			
//			case (IDLE):
			case (QUIT):{
				printf("quitting CONN\n");	
				File* curr=storage->last;
			
				for( int i=0; i < storage->numfiles; i++){		//close all files open by cid, unlock all files locked by cid	
					if( curr==NULL) break;				//safety check for empty list		
					
					findRmvId(curr->openIds, cid);
					if( curr->lockId==cid ) curr->lockId=-1;
										
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
		
ErrCLEANUP
	exit(EXIT_FAILURE);
ErrCLEAN

	}

void spawnworker(){								//worker threads SPAWNER
	for(int i=0; i<MAXTHREADS-1; i++){
		CREATE(&tid[i], NULL, work, NULL);		//WORKER THREAD
		printf("Created worker thread %d\n", i);
		}

	return ;
ErrCLEANUP
	return ;
ErrCLEAN
	}
	
/*-----------------------------------------------------MAIN----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int main(){

	unlink(SPATHNAME);								//remove server socket if already exists
	
	sigset_t mask, oldmask;			//SIGNAL MASKS
	sigemptyset(&mask);   
	sigaddset(&mask, SIGINT); 
	sigaddset(&mask, SIGQUIT);				//dropped SIGTERM
	sigaddset(&mask, SIGHUP);	
	pthread_sigmask(SIG_BLOCK, &mask, &oldmask);	//blocks signals during SERVER INITIALIZATION
	
	struct sigaction sigst;
	sigst.sa_handler=&sighandler;
	sigst.sa_mask=mask;								//blocks signals inside SIG HANDLER function
	
	sigaction(SIGINT, &sigst, NULL);				//assigns each SIG to the SIG HANDLER
	sigaction(SIGQUIT, &sigst, NULL);
	sigaction(SIGHUP, &sigst,NULL);
	
	
	
	
	int sid=-1;
	ErrNEG1(  sid=socket(AF_UNIX, SOCK_STREAM, 0)  );	//SID: server id (fd), SOCKET assigns it a free channel
	
	struct sockaddr_un saddr;							//saddr: server address, needed for BIND
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;							//adds domain and pathname to the server address structure
	strcpy(saddr.sun_path, SPATHNAME);				
	
	ErrNEG1(  bind(sid, (struct sockaddr*) &saddr, SUN_LEN(&saddr))  );	//BIND: NAMING of the opened socket
	
	ErrNEG1( listen(sid, 8)  );							//sets socket in LISTEN mode with a single connection
	
	printf("Server started\n");
	
	
	
	
	fd_set all, rdy;								//ALL set of FDs controlled by select()
	FD_ZERO(&all);

	FD_SET(sid, &all);									//adding SID to ALL
	int maxid=sid;
	
	
	
	
	ErrNULL(  pending=idListCreate()  )				//PENDING queue
	
	
	
	
	
	if( mkfifo("./done", 0777) != 0){				//DONE named pipe
		if( errno != EEXIST){
			perror("Error: couldnt create named pipe\n");
			ErrFAIL;
			}
		}
	ErrNEG1(  done=open("./done", O_RDWR)  );
	
	FD_SET(done, &all);									//adding DONE to ALL
	if(done > maxid) maxid=done;
	
	
	
	
	spawnworker();									//worker threads SPAWNER	
	
	
	
	
	ErrNEG1(  storageCreate()  );
		
	printf("Server ready\n");
	pthread_sigmask(SIG_SETMASK, &oldmask, NULL);	//unlocks SIGNALS for the rest of the execution
	
	while(!quit){
		rdy=all;										//select "destroys" the fd that arent rdy
		select(FD_SETSIZE, &rdy, NULL, NULL, NULL);		// select( n, read, write, err, timeout)
		if(quit) break;
		printf("select done\n");
		
		for(int i=0; i<=maxid; i++){
			if( FD_ISSET( i, &rdy) ){
				printf("----------\nfd: %d\n", i);
				if( i==sid){							//the server socket (sid) has a pending request and is ready to accept it
					int cid;
					ACCEPT(cid, sid, NULL, NULL);
					FD_SET(cid, &all);
					if(cid>maxid) maxid=cid;
					printf("accepted client %d\n", cid);
					}
				else if( i==done){					//the named pipe containing the finished requests has a cid that must return in the controlled set
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
	
	
	close(sid);							// /!\ only way to close the server is with signals
	unlink(SPATHNAME);
	idListDestroy(pending);
	close(done);
										// /!\ terminate threads correctly
										
	printf("\nServer successfully closed\n");
	return 0;
	
ErrCLEANUP
	//...
	exit(EXIT_FAILURE);
ErrCLEAN
	}
