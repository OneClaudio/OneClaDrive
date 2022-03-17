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
	
	


#define SPATHNAME "./server_sol"	//Server socket pathname

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
		
		int r=0;
		
		//LOCK
		switch(cmd->code){
			
			case(OPEN):
				if(cmd->filename == NULL)
					//ERROR		(needs filename passed)
				
				File* f=getfile(st, cmd->filename);
				if( FLAG_CREATE(cmd->info)){	
					if( f!=NULL)
						//ERROR		(file already existing)
					
					f=file_create(cmd->filename);
					if( f==NULL)
						//ERROR		(while allocating new file)
						
					addnew(st, f);					
					}
				else{
					if( f==NULL)
						//ERROR		(file not found)
					}
				
				if( openfile(st, f, cid) !=0)
					//ERROR		(idlist error (unlikely))
				
				if( FLAG_LOCK(cmd->info))
					if( lockfile(st, f, cid) !=0 )
						//ERROR (file already locked)
					
				//SUCCESS
				break;
			
			
			
			case(CLOSE):
				if(cmd->filename == NULL)
					//ERROR		(needs filename passed)

				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
				
				if( closefile(st, f, cid) !=0)
					//ERROR		(cid didnt have file f opened)
				
				//SUCCESS
				break;
			
			
			
			case(WRITE):
				if(cmd->filename == NULL)
					//ERROR		(needs filename passed)
				
				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
				
				//CHECK OPEN
				//CHECK UNLOCKED 		--> should be CHECK LOCKED by CID
				//CHECK NEVER WRITTEN (cont==NULL)		--> OVERWRITING not PERMITTED from specific
				
				//REPLY OK
				
				//READ from CID (SIZE/CONT)
				
				writefile(st, f, cont, size);	//this overwrites
				
				//SUCCESS
				break;
			
			
			
			case(APPEND):
				//same as write
				
				appendtofile(st, f, cont, size);
				//SUCCESS
				break;				
			
			
			
			case(READ):
				if(cmd->filename == NULL)			//TODO /!\ if no FILENAME could be READN! (maybe better to handle separately)
					//ERROR
				
				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
				
				//CHECK OPEN
				//CHECK UNLOCKED
				
				readfile(st, f, &cont, &size);
				
				//WRITE to CID (SIZE/CONT)
				
				//SUCCESS
				break;
			
			
			
			case(READN):
			
				break;			
		
		
		
			case(REMOVE):
				if(cmd->filename == NULL)
					//ERROR
				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
					
				//CHECK OPEN
				//CHECK UNLOCKED	--> should be CHECK LOCKED by CID
				
				rmv(st, f);<
				
				//WRITE to CID (SIZE/CONT)
				
				file_destroy(f);
				//SUCCESS
				break;
				
				
				
			case(LOCK):
				if(cmd->filename == NULL)
					//ERROR

				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
				
				if( lockfile(st, f, cid) !=0)
					//ERROR		(cid didnt have file f opened)
				
				//SUCCESS
				break;
			
			case(UNLOCK):
				if(cmd->filename == NULL)
					//ERROR

				File* f=getfile(st, cmd->filename);
				if( f==NULL)
					//ERROR		(file not found)
				
				if( unlockfile(st, f, cid) !=0)
					//ERROR		(cid didnt have file f opened)
				
				//SUCCESS
				break;
				
			default:
				fprintf(stderr, "Come sei finito qui? cmd code: %d\n", cmd->code);
				break;		
			}
		//UNLOCK
		}
	}

void spawnworker(){								//worker threads SPAWNER
	for(int i=0; i<MAXTHREADS-1; i++){
		CREATE( &tid[i], NULL, work, NULL);			//WORKER THREAD
		printf("Created worker thread %d\n", i);
		}	
	}

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

/*
[Realizzare l'Esercizio 2 dell'Esercitazione 10 con un pool di N thread (N è un parametro del programma) secondo il modello Manager-Workers dove però il generico thread Worker gestisce interamente tutta le richieste di un client connesso. Gestire i segnali SIGINT e SIGQUIT per la terminazione consistente del server.]
Realizzare una seconda versione dell'Esercizio 1 (sempre secondo lo schema Manager-Workers con thread pool) in cui il generico thread Worker gestisce solamente una richiesta di uno dei client connessi.
*/
