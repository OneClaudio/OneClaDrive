#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "./idlist.h"
#include "./comm.h"
#include "./filestorage.h"

/*	
typedef struct File{
	char*  name;
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    IdList*	openIds;
    int 	lockId;
    
    struct File* prev;
       
    } File;

typedef struct Storage{		//THREAD SAFE STORAGE type
    File* first;
    File* last;
    
	pthread_rwlock_t stlock;
	
	size_t numfiles;
	size_t capacity;
	
	} Storage;	*/
	
File* fileCreate(char* filename){
	File* new=calloc(1,sizeof(File));
    if(new==NULL){
    	errno=ENOMEM;
    	fprintf(stderr,"Error: malloc inside storage addNewFile() call\n");
    	return NULL;
    	}
    
    
    
	new->name=strdup( filename);
	new->cont=NULL;
	new->size=0;
	new->openIds=idListCreate();
	new->lockId=-1;
    
    //pthread_rwlock_init(&new->flock, NULL);
    
    new->prev=NULL;
    
    return new;
    }

void fileDestroy(File* victim){
	free(victim->name);
	if(victim->cont) free(victim->cont);
	idListDestroy(victim->openIds);
	//pthread_rwlock_destroy(&victim->flock);
	free(victim);
	}

int storageCreate(){			//allocates a new THREAD SAFE STORAGE
    storage=calloc(1,sizeof(Storage));
    if(storage==NULL) return -1;					//on ERROR:		returns NULL

    storage->first = NULL;
    storage->last  = NULL;
    
    storage->numfiles=0;
    storage->capacity=0;
    
    pthread_rwlock_init(&storage->lock, NULL);
    
    return 0;									//on SUCCESS:	returns a ptr to the STORAGE
	}

void storageDestroy(){		//deallocates a STORAGE

	File* curr=storage->last;
	
	File* prev=NULL;
	
	while( curr!=NULL){
		prev=curr->prev;
		printf("Deleting '%s'\n", curr->name);
		fileDestroy(curr);
		curr=prev;
		}	
	
	pthread_rwlock_destroy(&storage->lock);			//TODO
	free(storage);
	}
	
int addNewFile( File* new){

    //WRLOCK(&storage->stlock);						//shield storage ptrs from other threads

    if (storage->first==NULL || storage->last==NULL){	//if STORAGE EMPTY first and last point to the new element
        storage->first=new;
        storage->last=new;
    	}
    else{        									//STORAGE MULTIPLE NODES
        storage->first->prev=new;						//the first becomes the second in the storage
        storage->first=new;								//the new file becomes the first
        }
        
    //UNLOCK(&storage->stlock);
   
	storage->numfiles++;
    
    return 0;									//SUCCESS
	}

File* rmvLastFile(){	//rmvLastFile( storage, victim) removes the LAST file from STORAGE to make room
														//its not deallocated but only passed to the thread via VICTIM ptr
    //WRLOCK(&storage->stlock);
	
    File* victim=storage->last;
    
	if(victim == NULL){								//if the STORAGE is empty exits with a code <--
        //UNLOCK(&storage->stlock);							//STORAGE EMPTY
        return NULL;
    	}
	if(storage->first==storage->last){					//if victim is the last file in the storage, the first and last are resetted to null (initial state)
		storage->first=NULL;								//STORAGE 1 NODE
		storage->last=NULL;
		}
	else storage->last = storage->last->prev;				//STORAGE MULTIPLE NODES
	
    //UNLOCK(&storage->stlock);
    
    storage->numfiles--;
	storage->capacity -= victim->size;
    
    return victim;										//if the STORAGE wasnt empty exits with 0 <--
	}
	
File* getFile( char* filename){
	
	//RDLOCK(&storage->stlock);
    File* curr=storage->last;
    //UNLOCK(&storage->stlock);
    
    int found=0;
    
    while(found==0){
    	
    	//RDLOCK(&curr->flock);
    	if(curr==NULL) break;
    	
    	if( strcmp(curr->name, filename /*, strlen(filename)*/) == 0){
    		//UNLOCK(&curr->flock);
    		found=1;    				
    		break;
    		}
    		
    	//UNLOCK(&curr->flock);

    	curr=curr->prev;
		}	
    
    if( found ) return curr;			//PTR if ID is in the list
    else return NULL;    				//NULL otherwise
	}

int rmvThisFile( File* victim){
	
    File* curr=storage->last;
	
	if( storage->last==victim ){		//if the VICTIM is the last the removeLastFile() can handle it
		rmvLastFile();							//Return ptr discarded bc its equal to victim	
		free(victim);
		return 0;
		}
	
	while(1){
		if( curr->prev==victim)	break;	//when ID is found CURR is the NODE before it
		curr=curr->prev;
		}
	
	if(victim == storage->first){		//if the VICTIM is the FIRST in the list CURR becomes the FIRST
		storage->first=curr;
		curr->prev=NULL;
		}
	else curr->prev=victim->prev;		//otherwise VICTIM is skipped
	
	storage->numfiles--;
	storage->capacity -= victim->size;
	free(victim);
	return 0;
	}

void storagePrint(){
	File* curr=storage->last;
	while( curr!=NULL){
		printf("'%s'\n", curr->name);
		curr=curr->prev;
		}
	printf("\n");
	}

/*
int main(){
	
	storageCreate();

	do{		
		File* f1=fileCreate("file1");		
		addNewFile(f1);
		
		storagePrint();
	
		File* f2=fileCreate("file2");
		addNewFile(f2);	
		
		storagePrint();
	}while(0);
	
	do{
		File* f1=getFile("file1");
		if (f1==NULL) break;
		int cid=4;
		
		printf("found file %s\n", f1->name);
		
		openfile(f1, cid);
		if( findId(f1->openIds, cid) ) printf("%d succ opnd file f1\n", cid);
		
		lockfile(f1, cid);
		printf("%d has lockd f1\n", f1->lockId);
		
		char* buf=strdup("ALLAH is great!");
		writefile( f1, buf, strlen(buf)+1 );
		
		printf("f1 now contains: %s\n", (char*)f1->cont);
		
	} while(0); 
	
	do{
		File* f1=getFile("file1");
		if(f1==NULL) break;
		
		int cid=4;
		void* buf=NULL;
		size_t size;
		
		if( f1->lockId==cid)
			readfile(f1, &buf, &size);

		printf("read from f1: %s\n", (char*)buf);	//TODO
		
		free(buf);
		
		printf("cont of f1 intact: %s\n", (char*)f1->cont);
		
		unlockfile(f1, cid);
		closefile(f1, cid);	
		
	}while(0);
	
	storagePrint();
	
	File* ftrash=getFile("file1");
	rmvThisFile(ftrash);
	fileDestroy(ftrash);
	
	storagePrint();
	
	ftrash=rmvLastFile();
	fileDestroy(ftrash);
	
	storagePrint();
	storageDestroy();
	
	return 0;
	}	*/
