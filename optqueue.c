#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "./optqueue.h"

/*
typedef struct Opt{			//LIST Opt	contains:
    int cmd;							// single char represents command OPTIONS
    char* arglist;						// comma separated list of ARGUMENTS to the option
    struct Opt* prev;				// ptr to PREVIOUS Opt
	} Opt;

typedef struct OptQueue{
    Opt* first;					
    Opt* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
										//    ^					  ^
	} OptQueue;							//   first				last		*/

int optQueueCreate(){
    optQueue=malloc(sizeof(OptQueue));
    if(optQueue==NULL) return -1;

    optQueue->first = NULL;
    optQueue->last  = NULL;
    return 0;
	}


int optQueueDestroy(){		//dealocates a THREAD SAFE LIST
	
	Opt* curr;
	while( (curr=deqOpt( optQueue)) !=NULL);
		free(curr);
		
	free(optQueue);
	return 0;
	}

	
int enqOpt(int cmd, char* arglist){
    Opt* new=malloc(sizeof(struct Opt));
    if(new==NULL){
    	errno=ENOMEM;
    	fprintf(stderr,"Error: malloc inside OptQueue enqOpt() call\n");
    	return -1;
    	}
    	
    new->cmd=cmd;
    new->arglist=arglist;
    new->prev=NULL;

    if (optQueue->first==NULL){					//if QUEUE EMPTY first and last point to the new element
        optQueue->first=new;
        optQueue->last=new;
    	}
    else{        										//QUEUE has MULTIPLE NODES
        optQueue->first->prev=new;						//the first becomes the second in the optQueue
        optQueue->first=new;							//the new node becomes the first
        }

    return 0;
	}

Opt* deqOpt(){
    Opt* curr=optQueue->last;
    
	if(curr == NULL) return NULL;			//if the QUEUE is EMPTY returns NULL
														
	if(optQueue->first==optQueue->last){			//if curr is the last node in the optQueue, the first and last are resetted to null (initial state)
		optQueue->first=NULL;						//QUEUE has 1 NODE
		optQueue->last=NULL;
		}
	else optQueue->last = optQueue->last->prev;		//QUEUE has MULTIPLE NODES
    
    curr->prev=NULL;
    
    return curr;
	}

void optDestroy(Opt* victim){
	if( victim->arglist) free(victim->arglist);
	free(victim);
	}
