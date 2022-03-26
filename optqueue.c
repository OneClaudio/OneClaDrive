#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "./optqueue.h"
#include "./errcheck.h"

/*
typedef struct Opt{			//LIST Opt	contains:
    int cmd;						// single char represents command OPTIONS
    char* arglist;					// comma separated list of ARGUMENTS to the option
    struct Opt* prev;				// ptr to PREVIOUS Opt
	} Opt;

typedef struct OptQueue{
    Opt* first;					
    Opt* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
										//    ^					  ^
	} OptQueue;							//   first				last		*/


int optQueueCreate(){
    ErrNULL(  optQueue=malloc(sizeof(OptQueue))  );

    optQueue->first = NULL;
    optQueue->last  = NULL;
    SUCCESS    return  0;
    ErrCLEANUP return -1; ErrCLEAN
	}


int optQueueDestroy(){
	ErrNULL( optQueue );
	Opt* curr=optQueue->last;
	Opt* temp;
	
	while( curr!=NULL){
		temp=curr->prev;
		free(curr);
		curr=temp;
		}
		
	free(optQueue);
    SUCCESS    return  0;
    ErrCLEANUP return -1; ErrCLEAN
	}

	
int enqOpt(int cmd, char* arglist){
    Opt* new=NULL;
    ErrNULL(  new=malloc(sizeof(struct Opt))  );
    	
    new->cmd=cmd;
    new->arglist=arglist;	// /!\ ARGLIST member only receives COMMAND LINE ARGUMENTS (from argv[]): they are FREEd AUTOMATICALLY
    new->prev=NULL;

    if (optQueue->first==NULL){					//if QUEUE EMPTY first and last point to the new element
        optQueue->first=new;
        optQueue->last=new;
    	}
    else{        								//if QUEUE has MULTIPLE NODES
        optQueue->first->prev=new;					//the first becomes the second in the optQueue
        optQueue->first=new;						//the new node becomes the first
        }

	SUCCESS    return  0;
    ErrCLEANUP return -1; ErrCLEAN
	}

Opt* deqOpt(){
    Opt* curr=optQueue->last;
    
	if(curr == NULL) return NULL;				//if the QUEUE is EMPTY returns NULL: OPTIONS LIST FULLY PARSED
														
	if(optQueue->first==optQueue->last){		//if CURR is the ONLY NODE LEFT in the optQueue, the first and last are resetted to NULL (EMPTY state)
		optQueue->first=NULL;
		optQueue->last=NULL;
		}
	else optQueue->last=optQueue->last->prev;	//if the QUEUE has MULTIPLE NODES
    
    curr->prev=NULL;
    
    return curr;								//the CALLER is in charge of freeing the OPT using optDestroy()
	}
