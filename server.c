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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <signal.h>

#include "./comm.h"
#include "./utils.h"
#include "./idlist.h"
#include "./filestorage.h"

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

#define ACCEPT( cid, sid, addr, l) errno=0;					\
	cid=accept( sid, addr, l);								\
	if(cid<0){												\
		if( errno==EAGAIN) continue;						\
		else{												\
			perror("Error: accept of client failed\n");		\
			break;											\
			}												\
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
#define REPLY( MSG ){						\
	Reply reply=MSG;								\
	WRITE( cid, &reply, sizeof(Reply) );	\
	}

#define SPATHNAME "./server_sol"	//Server socket pathname

#define MAXNUMFILES 100
#define MAXCAPACITY 1000000		//1MB

#define MAXTHREADS 4
pthread_t tid[MAXTHREADS-1];		//WORKER THREADS



queue_tsafe* pending;	//thread safe QUEUE for fds rdy for the worker threads		M -> WTs
int done;				//named PIPE for fds finished by the worker threads		  WTs -> M

volatile sig_atomic_t quit=0;		//QUIT SIGNAL handler
void sighandler(int unused){
    quit=1;
	}

void* work(void* unused){			//ROUTINE of WORKER THREADS
	while(1){
		int cid;
		
		while( deq(pending, &cid) != 0)				//gets CID from QUEUE
			usleep(50000);
		
		Cmd cmd;		
		READ(cid, &cmd, sizeof(Cmd));
		
		bool quit=false;
		
		//Reply reply;
		
		ErrNZERO(  pthread_rwlock_wrlock(storage->lock)  );
		
		switch(cmd->code){
			
			case(OPEN):
			/*	if(cmd->filename == NULL)	//Already checked in API call//  */
								
				File* f=getFile(cmd->filename);
				
				if( cmd->info & O_CREATE ){					//Client requested an O_CREATE openFile()
					
					if( f!=NULL){								//ERR: file already existing
						REPLY(EXISTS);
						break;
						}
						
					ErrNULL(  f=fileCreate(cmd->filename)  );	//ERR:	fatal malloc error while allocating space for new file (ENOMEM)
						
					ErrNEG1(  addNewFile(st, f)  );				//Successful CREATION	
					}	
				else{										//Client requested a normal openFile()
					if( f==NULL){ REPLY(NOTFOUND); break; }		//ERR: file not already existing/not found
					}
				
				if( findId(f->openIds, cid) ) REPLY(ALROPEN);	
				ErrNEG1(  enqId(file->openIds, cid)  );				//Successful OPEN
				
				if( cmd->info & O_LOCK ){
					if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
					else f->lockId=cid;			//Successful LOCK
					}
					
				REPLY(OK);
				break;
					
			
			
			
			case(CLOSE):
				File* f=getFile(cmd->filename);
				if( f==NULL){				//ERR: file not found
						REPLY(NOTFOUND);
						break;
						}
				
				if( findRmvId(f->openIds, cid)==-1  ) REPLY(NOTOPEN);
				else REPLY(OK);
				break;

			
			
			
			case(WRITE):				
				File* f=getFile(cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				if( f->cont != NULL ){ REPLY(NOTEMPTY); break; }
				
				REPLY(OK);
				
				size_t size=0;
				READ(cid, &size, csizeof(size_t));
				
				void* cont=NULL;
				ErrNULL( cont=malloc(size) );
				READ(cid, &cont, size);
				
				if( size > MAXCAPACITY ){ REPLY(TOOBIG); break; }
				
				while( storage->numfiles == MAXNUFILES  ||  (storage->capacity+size) > MAXCAPACITY ){
					REPLY(CACHE);
					
					File* victim=NULL;
					ErrNULL( victim=rmvLastFile() );
					
					WRITE(cid, &victim->size, sizeof(size_t));
					WRTIE(cid, &victim->cont, victim->size );
					
					fileDestroy( victim );
					}
				
				f->size=size;
				f->cont=cont;
				storage->numfiles++;
				storage->capacity+=size;
				
				REPLY(OK);
				break;				

			case(APPEND):
				File* f=getFile(cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				REPLY(OK);
				
				size_t size=0;
				READ(cid, &size, sizeof(size_t));
				
				void* buf=NULL;
				ErrNULL( buf=malloc(size) );
				READ(cid, &buf, size);
				
				if( size > MAXCAPACITY ){ REPLY(TOOBIG); break; }
				
				while( storage->numfiles == MAXNUFILES  ||  (storage->capacity+size) > MAXCAPACITY ){
					REPLY(CACHE);
					
					File* victim=NULL;
					ErrNULL( victim=rmvLastFile() );
					
					WRITE(cid, &victim->size, sizeof(size_t));
					WRTIE(cid, &victim->cont, victim->size );
					
					fileDestroy( victim );
					}
					
				void* extendedcont=NULL;
				ErrNULL(  extendedcont=realloc( f->cont, file->size+size)  );
				f->cont=extendedcont;
				memcpy( f->cont+f->size, cont, size );
				f->size+=size
				
				storage->capacity+=size;
				
				free(buf);
				
				REPLY(OK);
				break;			




			case(READ):
				File* f=getFile(cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( !findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
				
				REPLY(OK);
				
				WRITE(cid, &f->size, sizeof(size_t));
				WRITE(cid, &f->cont, f->size);
				
				break;
			
			
			
			
			case(READN):
				REPLY(OK);
				
				File* curr=storage->last;
				
				int n=cmd->info;
				
				for( int i=0; i < (n<=0 ? storage->numfiles : n); i++){
					if( curr==NULL) break;
					
					if( f->lockId!=-1 && f->lockId!=cid ) break;
					
					REPLY(ANOTHER);
					WRITE(cid, &curr->size, sizeof(size_t));
					WRITE(cid, &curr->cont, curr->size);
					
					curr=curr->prev;					
					}
				
				REPLY(OK);
				break;			
		
		
		
		
			case(REMOVE):
				File* f=getFile(cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
					
				if( ! findId(f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){	REPLY(NOTLOCKED); break; }
				
				ErrNEG1(  rmvThisFile(f)  );
				
				fileDestroy(f);
				
				REPLY(OK);
				break;
				
				
				
				
			case(LOCK):
				File* f=getFile(cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId( f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId!=-1 && f->lockId!=cid ){ REPLY(LOCKED); break; }
				else if( f->lockId==cid ){	REPLY(ALRLOCKED); break; }
				
			/*	if( f->lockId==-1) */
				f->lockId=cid;
				
				REPLY(OK);
				break;
				
				
				
			
			case(UNLOCK):
				File* f=getfile(st, cmd->filename);
				if( f==NULL){ REPLY(NOTFOUND); break; }
				
				if( ! findId( f->openIds, cid) ){ REPLY(NOTOPEN); break; }
				
				if( f->lockId != cid ){ REPLY(NOTLOCKED); break; }
				
				f->lockId=-1;
				
				REPLY(OK);
				break;
			
			case(QUIT):
				File* curr=storage->last;
			
				for( int i=0; i < storage->numfiles; i++){		//close all files open by cid, unlock all files locked by cid	
					if( curr==NULL) break;				//safety check for empty list		
					
					findRmvId(f->openIds, cid);
					if( f->lockId==cid ) f->lockId=-1;
										
					curr=curr->prev;					
					}				
				
				REPLY(OK);
				break;
				
			default:
				fprintf(stderr, "Come sei finito qui? cmd code: %d\n", cmd->code);
				break;		
			}
		ErrNZERO(  pthread_rwlock_wrlock(storage->lock)  );
		
		if( !quit ) enqId( /* idqueue */, cid);
		else /*	Client CID successfully unconnected */
		
		}
	}

void spawnworker(){								//worker threads SPAWNER
	for(int i=0; i<MAXTHREADS-1; i++){
		CREATE( &tid[i], NULL, work, NULL);			//WORKER THREAD
		printf("Created worker thread %d\n", i);
		}	
	}
	
/*-----------------------------------------------------MAIN----------------------------------------------------------*/	
/*-------------------------------------------------------------------------------------------------------------------*/

int main(){

	unlink(SPATHNAME);								//remove server socket if already exists
	
	sigset_t mask, oldmask;			//SIGNAL MASKS
	sigemptyset(&mask);   
	sigaddset(&mask, SIGINT); 
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);	
	pthread_sigmask(SIG_BLOCK, &mask, &oldmask);	//blocks signals during SERVER INITIALIZATION
	
	struct sigaction sigst;
	sigst.sa_handler=&sighandler;
	sigst.sa_mask=mask;								//blocks signals inside SIG HANDLER function
	
	sigaction(SIGINT, &sigst, NULL);				//assigns each SIG to the SIG HANDLER
	sigaction(SIGQUIT, &sigst, NULL);
	sigaction(SIGTERM, &sigst, NULL);
	sigaction(SIGHUP, &sigst, NULL);
	
	
	
	
	int sid=socket(AF_UNIX, SOCK_STREAM, 0);		//SID: server id (fd), SOCKET assigns it a free channel
	if( sid<0){
		perror("Error: couldnt open socket\n");
		exit(EXIT_FAILURE);
		}
	
	struct sockaddr_un saddr;						//saddr: server address, needed for BIND
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;							//adds domain and pathname to the server address structure
	strcpy(saddr.sun_path, SPATHNAME);				
	
	BIND(sid, (struct sockaddr*) &saddr, SUN_LEN(&saddr));		//BIND: NAMING of the opened socket
	
	LISTEN( sid, 8);								//sets socket in LISTEN mode with a single connection
	
	printf("Server started\n");
	
	
	
	
	fd_set all, rdy;								//ALL set of FDs controlled by select()
	FD_ZERO(&all);

	FD_SET(sid, &all);									//adding SID to ALL
	int maxid=sid;
	
	
	
	
	pending=qcreate();								//PENDING queue
	if( pending==NULL){
		perror("Error: couldnt create pending queue\n");
		exit(EXIT_FAILURE);
		}
	
	
	
	
	if( mkfifo("./done", 0777) != 0){				//DONE named pipe
		if( errno != EEXIST){
			perror("Error: couldnt create named pipe\n");
			exit(EXIT_FAILURE);
			}
		}
	done=open("./done", O_RDWR);
	if( done<0){
		perror("Error: couldnt open named pipe\n");
		exit(EXIT_FAILURE);
		}
	
	FD_SET(done, &all);									//adding DONE to ALL
	if(done > maxid) maxid=done;
	
	
	
	
	spawnworker();									//worker threads SPAWNER						
		
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
					enq(pending, i);
					printf("serving client %d\n", i);
					}				
				}
			}
		}
	
	
	close(sid);							// /!\ only way to close the server is with signals
	unlink(SPATHNAME);
	qdestroy(pending);
	close(done);
										// /!\ terminate threads correctly
										
	printf("\nServer successfully closed\n");
	return 0;
	}
