#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

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

typedef struct node{			//LIST NODE	contains:
    int id;							//FILE DESCRIPTOR
    struct node* prev;				// ptr to PREVIOUS node
	} node;

typedef struct idlist{			//THREAD SAFE LIST type
    node* first;					
    node* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
    pthread_mutex_t mutex;				//    ^					  ^
	} idlist;							//   first				last

idlist* idlist_create(){			//allocates a new THREAD SAFE LIST
    idlist* list=malloc(sizeof(idlist));
    if(list==NULL) return NULL;						//on ERROR:		returns NULL

    list->first = NULL;
    list->last  = NULL;
    pthread_mutex_init(&list->mutex, NULL);
    
    return list;									//on SUCCESS:	returns a ptr to the T SAFE LIST
	}
	
int enq(idlist* list, int id){			//ENQ( list, val) adds node with id val in first position in the list
    node* new=malloc(sizeof(struct node));				//ENQUEUE used by MANAGER THREAD to give a CIDto the worker threads
    if(new==NULL){
    	errno=ENOMEM;
    	fprintf(stderr,"Error: malloc inside idlist enq() call\n");
    	return -1;
    	}
    new->id=id;
    new->prev=NULL;
	
/*	while(1){
		usleep(10000);
    	if( TRYLOCK(&list->mutex)==0) break;
    	} 	*/
    LOCK(&list->mutex);							//shield list from other threads
    //printf("Enqueueing %d\n", id);		//DeBuG

    if (list->first==NULL){						//if LIST EMPTY first and last point to the new element
        list->first=new;
        list->last=new;
    	}
    else{        										//LIST MULTIPLE NODES
        list->first->prev=new;						//the first becomes the second in the list
        list->first=new;							//the new node becomes the first
        }
        
    UNLOCK(&list->mutex);
    return 0;
	}

int deq(idlist* list, int* id){			//DEQ( list, valptr) stores the last element id in id and deletes the last node
														//DEQUEUE used bu WORKER THREADS to fetch a CID (client file desc)
/*	while(1){
		usleep(10000);
    	if( TRYLOCK(&list->mutex)==0) break;
    	} */
    LOCK(&list->mutex);
	
    node* curr=list->last;
    
	if(curr == NULL){								//if the LIST is empty exits with a code <--
        UNLOCK(&list->mutex);							//LIST EMPTY
        return -1;
    	}
	if(list->first==list->last){					//if curr is the last node in the list, the first and last are resetted to null (initial state)
		list->first=NULL;								//LIST 1 NODE
		list->last=NULL;
		}
	else list->last = list->last->prev;				//LIST MULTIPLE NODES
	
    UNLOCK(&list->mutex);
    
    *id = curr->id;							//the id of the EXTRACTED NODE is read and then freed
	//printf("Dequeueing %d\n", *id);	//DeBuG
    free(curr);
    
    return 0;										//if the LIST wasnt empty exits with 0 <--
	}

int find(idlist* list, int id){

    LOCK(&list->mutex);
    
    node* curr=list->last;
    int found=0;
    
    while(curr!=NULL || found==0){
    	if( curr->id == id ){
    		found=1;
    		break;
    		}
    	curr=curr->prev;
		}

	UNLOCK(&list->mutex);
    
    if( found ) return 1;			//1 if ID is in the list
    else return 0;    				//0 otherwise
    }

int findrmv(idlist* list, int id){
    LOCK(&list->mutex);
	
	node* curr=list->last;
	int found=0;
	
	if(curr == NULL){				//if the LIST is empty exits with a code <--
        UNLOCK(&list->mutex);			//LIST EMPTY
        return -1;
    	}
	
	int unused;
	if( curr->id == id){			//if the node to remove is the last, the DEQ can handle it
		UNLOCK(&list->mutex);
		deq(list, &unused);
		return 0;
		}
	
	while(curr->prev != NULL || found==0 ){
		printf("curr id: %d, prev id: %d\n", curr->id, curr->prev->id);
		if( curr->prev->id == id ){
			found=1;					//when ID is found CURR is the NODE before it
			break;
			}
		curr=curr->prev;
		}

	if(found){
		node* victim=curr->prev;
		if(victim == list->first){		//if the VICTIM is the FIRST in the list, CURR becomes the FIRST
			list->first=curr;
			curr->prev=NULL;
			}
		else{							//otherwise VICTIM is skipped
			curr->prev=victim->prev;
			}
		free(victim);			
		UNLOCK(&list->mutex);
		return 0;						//0 successful remove
		}
	else{
		UNLOCK(&list->mutex);
		return -1;						//-1 error id not found
		}
	}

int idlist_destroy(idlist* list){		//dealocates a THREAD SAFE LIST
	
	int unused;
	while( deq( list, &unused)==0);
		//printf("list still full, removed %d\n", unused);

	pthread_mutex_destroy(&list->mutex);
	free(list);
	return 0;
	}

//TEST MAIN
/*
void idlist_print(idlist* queue){
	node* curr=queue->last;
	while( curr!=NULL){
		
		printf(" %d |", curr->id);
		curr=curr->prev;
		}
	printf("\n");
	}

// main() - funzione main
int main(){
    // creo la coda
    idlist* l = idlist_create();

    // aggiungo un po' di elementi
    enq(l, 1);	idlist_print(l);    
    enq(l, 2);  idlist_print(l);    
    enq(l, 8);  idlist_print(l);    
    enq(l, 4);  idlist_print(l);
    enq(l, 7);	idlist_print(l);
    
    
    findrmv(l, 8);	idlist_print(l);
    findrmv(l, 1);	idlist_print(l);
    findrmv(l, 7);	idlist_print(l);
    
    if( find(l,2) ) printf("2 trovato\n");
    if( find(l,4) ) printf("4 trovato\n");
    
    findrmv(l,2);	idlist_print(l);
    findrmv(l,4);	idlist_print(l);
    	
	enq(l, 4);   idlist_print(l); 
	enq(l,2);   idlist_print(l);

    printf("EXITING\n");
    idlist_destroy(l);

    return 0;
	}	*/
