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

#define LOCK(l)      if (pthread_mutex_lock(l) != 0){									\
	fprintf(stderr, "Error during lock: '%s' at %s:%d\n", #l, __FILE__, __LINE__);		\
	perror(NULL);																		\
    pthread_exit((void*)EXIT_FAILURE);			   										\
  	}
  	
#define TRYLOCK(l)	pthread_mutex_trylock(l)
  	
#define UNLOCK(l)    if (pthread_mutex_unlock(l) != 0){									\
	fprintf(stderr, "Error during unlock: '%s' at %s:%d\n", #l, __FILE__, __LINE__);	\
	perror(NULL);																		\
	pthread_exit((void*)EXIT_FAILURE);				    								\
	}

typedef struct node{			//QUEUE NODE	contains:
    int value;									//FILE DESCRIPTOR
    struct node* prev;							// ptr to PREVIOUS node
	} node;

typedef struct queue_tsafe{		//THREAD SAFE QUEUE type
    node* first;
    node* last;
    pthread_mutex_t mutex;
	} queue_tsafe;

queue_tsafe* qcreate(){			//allocates a new THREAD SAFE QUEUE
    queue_tsafe* queue=malloc(sizeof(queue_tsafe));
    if(queue==NULL) return NULL;					//on ERROR:		returns NULL

    queue->first = NULL;
    queue->last  = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    
    return queue;									//on SUCCESS:	returns a ptr to the T SAFE QUEUE
	}

void qdestroy(queue_tsafe* queue){		//dealocates a THREAD SAFE QUEUE
	pthread_mutex_destroy(&queue->mutex);
	free(queue);
	}
	
void enq(queue_tsafe* queue, int value){			//ENQ( queue, val) adds node with value val in first position in the queue
    node *new=malloc(sizeof(struct node));				//ENQUEUE used by MANAGER THREAD to give a CIDto the worker threads
    new->value=value;
    new->prev=NULL;
	
/*	while(1){
		usleep(10000);
    	if( TRYLOCK(&queue->mutex)==0) break;
    	} 	*/
    LOCK(&queue->mutex);							//shield queue from other threads
    //printf("Enqueueing %d\n", value);		//DeBuG

    if (queue->first==NULL){						//if QUEUE EMPTY first and last point to the new element
        queue->first=new;
        queue->last=new;
    	}
    else{        										//QUEUE MULTIPLE NODES
        queue->first->prev=new;						//the first becomes the second in the queue
        queue->first=new;							//the new node becomes the first
        }
        
    UNLOCK(&queue->mutex);
}

int deq(queue_tsafe* queue, int *value){			//DEQ( queue, valptr) stores the last element value in value and deletes the last node
														//DEQUEUE used bu WORKER THREADS to fetch a CID (client file desc)
/*	while(1){
		usleep(10000);
    	if( TRYLOCK(&queue->mutex)==0) break;
    	} */
    LOCK(&queue->mutex);
	
    node* curr=queue->last;
    
	if(curr == NULL){								//if the QUEUE is empty exits with a code <--
        UNLOCK(&queue->mutex);							//QUEUE EMPTY
        return -1;
    	}
	if(queue->first==queue->last){					//if curr is the last node in the queue, the first and last are resetted to null (initial state)
		queue->first=NULL;								//QUEUE 1 NODE
		queue->last=NULL;
		}
	else queue->last = queue->last->prev;				//QUEUE MULTIPLE NODES
	
    UNLOCK(&queue->mutex);
    
    *value = curr->value;							//the value of the EXTRACTED NODE is read and then freed
	//printf("Dequeueing %d\n", *value);	//DeBuG
    free(curr);
    
    return 0;										//if the QUEUE wasnt empty exits with 0 <--
	}


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
	if( write( id, addr, l) <0 ){									\
		perror("Error during write\n");								\
		break;														\
		}

#define READ( id, addr, l) errno=0;									\
	if( read( id, addr, l) <0 ){									\
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
		
		int l;		
		READ(cid, &l, sizeof(int));
			
		char msg[l];		
		READ(cid, &msg, l);							//READ from the client into local array
		
		if( strncmp(msg, "quit", 4) != 0 ){		//if it isnt a QUIT signal
			for(int i=0; i<l; i++)
				msg[i]=toupper(msg[i]);				//toupper code just werks
			msg[l]='\0';
		
			WRITE(cid, &l, sizeof(int));			//WRITE the result to the client process
			WRITE(cid, &msg, l);
			
			WRITE(done, &cid, sizeof(int));			//puts CID on the PIPE for the MANAGER
			}
		else{									//if it is a QUIT signal doesnt return the CID and CLOSES it instead
			printf("Closing connection with client\n");
			close(cid);
			}
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
