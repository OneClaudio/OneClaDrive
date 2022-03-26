#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "./errcheck.h"
#include "./idlist.h"
 
#define LOCK(l)   ErrERRNO(  pthread_mutex_lock(l)   );
#define UNLOCK(l) ErrERRNO(  pthread_mutex_unlock(l) );

	/*------------------------in-Header------------------------*/
/*
#define EMPTYLIST 1

typedef struct IdNode{			//LIST IDNODE	contains:
    int id;							//FILE DESCRIPTOR
    struct IdNode* prev;				// ptr to PREVIOUS IdNode
	} IdNode;

typedef struct IdList{			//THREAD SAFE LIST type
    IdNode* first;					
    IdNode* last;						//	enqId()-->	 x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]	-->deqId()
    pthread_mutex_t mutex;				//			    	^					^
	} IdList;							//			 	  first				  last
*/

	/*-------------------Create-/-Destroy----------------------*/

IdList* idListCreate(){
    IdList* list=NULL;
    ErrNULL( list=malloc(sizeof(IdList))  );

    list->first = NULL;
    list->last  = NULL;
    ErrERRNO(  pthread_mutex_init(&list->mutex, NULL)  );
    
    SUCCESS return list;									//on SUCCESS:	returns a ptr to the T SAFE LIST
    ErrCLEANUP return NULL; ErrCLEAN
	}



int idListDestroy(IdList* list){
	
//	int r;
//	while(  (r=deqId( list, &r))==0  );	//EMPTIES IDLIST until EMPTY or ERROR encountere
//	if( r!=EMPTYLIST ) ErrSHHH;

	IdNode* curr=list->last;
	IdNode* temp=NULL;
	while(curr!=NULL){
		temp=curr->prev;
		free(curr);
		curr=temp;
		}
	
	ErrERRNO(  pthread_mutex_destroy(&list->mutex)  );
	
	free(list);
	return 0;

ErrCLEANUP
	if(list) free(list);
	return -1;
ErrCLEAN
	}

	/*-------------------Functionalities-----------------------*/
	
int enqId(IdList* list, int id){			//enqId( list, val) adds node with id val in first position in the list
    										//ENQUEUE used by MANAGER THREAD to send a CID to the worker threads
    IdNode* new=NULL;				
    ErrNULL(  new=malloc(sizeof(IdNode))  );				
    new->id=id;									//CREATION of a new node with ID passed as parameter
    new->prev=NULL;
	
	
    LOCK(&list->mutex);

    if (list->first==NULL){						//if LIST is EMPTY first and last point to the new element
        list->first=new;
        list->last=new;
    	}
    else{        								//if the LIST has MULTIPLE NODES
        list->first->prev=new;						//the first becomes the second in the list
        list->first=new;							//the new node becomes the first
        }
      
    UNLOCK(&list->mutex);

	SUCCESS return 0;
	ErrCLEANUP return -1; ErrCLEAN	
	}



int deqId(IdList* list, int* id){			//deqId( list, valptr) stores the last element id in id and deletes the last node
														//DEQUEUE used bu WORKER THREADS to fetch a CID (client file desc)
    LOCK(&list->mutex);
	
    IdNode* victim=list->last;
    
	if(victim == NULL){						//if LIST is EMPTY unlocks and comunicates it 
        UNLOCK(&list->mutex);				// (content of ID ptr: UNCHANGED)
        return EMPTYLIST;
    	}
	else if(list->first==list->last){		//if victim is the ONLY ONE LEFT in the LIST the first and last ptrs are set to NULL (EMPTY state)
		list->first=NULL;							
		list->last=NULL;
		}
	else list->last = list->last->prev;		//if LIST has MULTIPLE NODES simply pop the last one
	
    UNLOCK(&list->mutex);
    
    *id = victim->id;						//the ID of the VICTIM NODE is sent back trough ID ptr
    free(victim);							//now the VICTIM NODE can be FREEd

	SUCCESS return 0;
	ErrCLEANUP return -1; ErrCLEAN
	}



bool findId(IdList* list, int id){		//needed to check if a specific CID has OPENED a FILE, this operation is READ ONLY	
//    LOCK(&list->mutex);					//locks not really needed, the whole STORAGE is WRITE LOCKED during OPERATIONS that need to write
    
    IdNode* curr=list->last;				//starting from the LAST NODE
    bool found=false;
    
    while(curr!=NULL){
    	if( curr->id == id ){
    		found=true;
    		break;
			}
    	curr=curr->prev;
		}
//	UNLOCK(&list->mutex);
												//TRUE:  ID PRESENT
    SUCCESS return found;						//FALSE: ID NOT PRESENT
    }



bool findRmvId(IdList* list, int id){	//needed to remove a specific CID from the openId IdList (to CLOSE a FILE)
//	LOCK(&list->mutex);						//locks not really needed, the whole STORAGE is WRITE LOCKED during CLOSE
	
	IdNode* curr=list->last;
	bool found=false;
	
	if(curr == NULL) return found;					//if the LIST is empty, returns 0


	if( curr->id == id){				//if the node to remove is the last, the deqId can handle it
		if(list->first==list->last){		//if victim is the ONLY ONE LEFT in the LIST the first and last ptrs are set to NULL (EMPTY state)
			list->first=NULL;							
			list->last=NULL;
			}
		else list->last=list->last->prev;	//if LIST has MULTIPLE NODES simply pop the last one
		free(curr);
		return true;				//TRUE:  ID FOUND and REMOVED
		}	
	
	
	while(curr->prev != NULL ){			//TRAVERSE the LIST until the END or when ID FOUND
		if( curr->prev->id == id ){
			found=true;						//when the VICTIM ID is found CURR points to the NODE BEFORE IT
			break;
			}
		curr=curr->prev;
		}

	if(found==true){
		IdNode* victim=curr->prev;
		
		if(victim == list->first){		//if the VICTIM is the FIRST NODE in the LIST, CURR becomes the FIRST
			list->first=curr;
			curr->prev=NULL;			//																	              ________
			}							//																	             v        \       ...
		else{							//if the VICTIM is IN THE MIDDLE of the LIST, it is JUMPED over:          ... <-[ ]<-[X] [ ]<-[ ] ...
			curr->prev=victim->prev;
			}

		free(victim);
		}
									//TRUE:  ID FOUND and REMOVED
    return found;					//FALSE: ID NOT PRESENT
	}

//TEST MAIN
/*
void printIdList(IdList* queue){
	IdNode* curr=queue->last;
	while( curr!=NULL){
		
		printf(" %d |", curr->id);
		curr=curr->prev;
		}
	printf("\n");
	}

int main(){
    // creo la coda
    IdList* l = idListCreate();

    // aggiungo un po' di elementi
    enqId(l, 1);  printIdList(l);    
    enqId(l, 2);  printIdList(l);    
    enqId(l, 8);  printIdList(l);    
    enqId(l, 4);  printIdList(l);
    enqId(l, 7);  printIdList(l);
    
    
    findRmvId(l, 8);	printIdList(l);
    findRmvId(l, 1);	printIdList(l);
    findRmvId(l, 7);	printIdList(l);
    
    if( findId(l,2) ) printf("2 trovato\n");
    if( findId(l,4) ) printf("4 trovato\n");
    
    findRmvId(l,2);	printIdList(l);
    findRmvId(l,4);	printIdList(l);
    	
	enqId(l, 4);   printIdList(l); 
	enqId(l,2);   printIdList(l);

    printf("EXITING\n");
    idListDestroy(l);

    return 0;
	}	*/
