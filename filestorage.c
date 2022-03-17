#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "./idlist.h"

/*
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
	}	*/
	
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

typedef struct File{
	char*  name;
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    idlist*	openIds;
    int 	lockId;
    
    struct File* prev;
       
    } File;

typedef struct Storage{		//THREAD SAFE STORAGE type
    File* first;
    File* last;
    
	pthread_rwlock_t stlock;
	
	size_t numfiles;
	size_t capacity;
	
	} Storage;
	
File* file_create(char* filename){
	File* new=malloc(sizeof(File));
    if(new==NULL){
    	errno=ENOMEM;
    	fprintf(stderr,"Error: malloc inside storage addnew() call\n");
    	return NULL;
    	}
    
    
    
	new->name=strndup( filename, strlen(filename) );
	new->cont=NULL;
	new->size=0;
	new->openIds=idlist_create();
	new->lockId=0;
    
    //pthread_rwlock_init(&new->flock, NULL);
    
    new->prev=NULL;
    
    return new;
    }

void file_destroy(File* victim){
	free(victim->name);
	if(victim->cont) free(victim->cont);
	idlist_destroy(victim->openIds);
	//pthread_rwlock_destroy(&victim->flock);
	free(victim);
	}

Storage* storage_create(){			//allocates a new THREAD SAFE STORAGE
    Storage* storage=malloc(sizeof(Storage));
    if(storage==NULL) return NULL;					//on ERROR:		returns NULL

    storage->first = NULL;
    storage->last  = NULL;
    
    storage->numfiles=0;
    storage->capacity=0;
    
    pthread_rwlock_init(&storage->stlock, NULL);
    
    return storage;									//on SUCCESS:	returns a ptr to the STORAGE
	}

void storage_destroy(Storage* storage){		//deallocates a STORAGE

	File* curr=storage->last;
	
	File* prev=NULL;
	
	while( curr!=NULL){
		prev=curr->prev;
		printf("Deleting '%s'\n", curr->name);
		file_destroy(curr);
		curr=prev;
		}	
	
	pthread_rwlock_destroy(&storage->stlock);			//TODO
	free(storage);
	}
	
int addnew(Storage* storage, File* new){

    //WRLOCK(&storage->stlock);						//shield storage ptrs from other threads

    if (storage->first==NULL){						//if STORAGE EMPTY first and last point to the new element
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

File* rmvlast(Storage* storage){	//RMVLAST( storage, victim) removes the LAST file from STORAGE to make room
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
	
File* getfile(Storage* storage, char* filename){
	
	//RDLOCK(&storage->stlock);
    File* curr=storage->last;
    //UNLOCK(&storage->stlock);
    
    int found=0;
    
    while(found==0){
    	
    	//RDLOCK(&curr->flock);
    	if(curr==NULL) break;
    	
    	if( strncmp(curr->name, filename, strlen(filename)) ){
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

void rmv(Storage* storage, File* victim){
	
    File* curr=storage->last;
	
	if( curr==victim ){					//if the VICTIM is the last, the DEQ can handle it
		rmvlast(storage);					//return ptr discarded, who caresb
		return;
		}
	
	while(1){
		if( curr->prev==victim)	break;	//when ID is found CURR is the NODE before it
		curr=curr->prev;
		}
	
	if(victim == storage->first){		//if the VICTIM is the FIRST in the list, CURR becomes the FIRST
		storage->first=curr;
		curr->prev=NULL;
		}
	else curr->prev=victim->prev;		//otherwise VICTIM is skipped
	
	storage->numfiles--;
	storage->capacity -= victim->size;
	}

/*
File* getrmv(Storage* storage, char* filename){
	
    File* curr=storage->last;
	int found=0;
	
	if( curr == NULL )				//if the LIST is empty exits with a code <--
		return NULL;					//LIST EMPTY
	
	if( strncmp(curr->name, filename, strlen(filename)) )			//if the node to remove is the last, the DEQ can handle it
		return rmvlast(storage);
	
	while(curr->prev != NULL || found==0 ){
		if( strncmp(curr->prev->name, filename, strlen(filename)) ){
			found=1;					//when ID is found CURR is the NODE before it
			break;
			}
		curr=curr->prev;
		}
	
	if(found){
		File* victim=curr->prev;
		
		if(victim == storage->first){		//if the VICTIM is the FIRST in the list, CURR becomes the FIRST
			storage->first=curr;
			curr->prev=NULL;
			}
		else curr->prev=victim->prev;	////otherwise VICTIM is skipped
		
		return victim;						//0 successful remove
		}
	else return NULL;						//NOT FOUND
	}
*/

void writefile( Storage* storage, File* file, void* cont, size_t size){
	
	file->cont=cont;
	file->size+=size;
	
	storage->capacity+=size;
	}
	
//TODO APPEND

void readfile( Storage* storage, File* file, void** cont, size_t* size){
	*size=file->size;
    *cont=malloc(*size);
    memcpy(*cont, file->cont, file->size);
	}

int openfile( Storage* storage, File* file, int cid){
	return enq(file->openIds, cid);
	}

int closefile( Storage* storage, File* file, int cid){
	return findrmv(file->openIds, cid);
	}
	
int lockfile( Storage* storage, File* file, int cid){
	if( file->lockId != 0) return -1;
	
	file->lockId=cid;
	return 0;
	}
	
int unlockfile( Storage* storage, File* file, int cid){
	if( file->lockId != cid) return -1;
	
	file->lockId=0;
	return 0;
	}

void storage_print(Storage* storage){
	File* curr=storage->last;
	while( curr!=NULL){
		printf(" %s |", curr->name);
		curr=curr->prev;
		}
	printf("\n");
	}

int main(){
	
	Storage* st=storage_create();

	do{		
		File* f1=file_create("file1");		
		addnew(st, f1);
		
		storage_print(st);
	
		File* f2=file_create("file2");
		addnew(st, f2);	
		
		storage_print(st);
	}while(0);
	
	do{
		File* f1=getfile(st,"file1");
		if (f1==NULL) break;
		int cid=4;
		
		printf("found file %s\n", f1->name);
		
		openfile(st, f1, cid);
		if( find(f1->openIds, cid) ) printf("%d succ opnd file f1\n", cid);
		
		lockfile(st, f1, cid);
		printf("%d has lockd f1\n", f1->lockId);
		
		char* buf=strdup("ALLAH is great!");
		writefile( st, f1, buf, strlen(buf)+1 );
		
		printf("f1 now contains: %s\n", (char*)f1->cont);
		
	} while(0); 
	
	do{
		File* f1=getfile(st,"file1");
		if(f1==NULL) break;
		
		int cid=4;
		void* buf=NULL;
		size_t size;
		
		if( f1->lockId==cid)
			readfile(st, f1, &buf, &size);

		printf("read from f1: %s\n", (char*)buf);	//TODO
		
		free(buf);
		
		printf("cont of f1 intact: %s\n", (char*)f1->cont);
		
		unlockfile(st, f1, cid);
		closefile(st, f1, cid);	
		
	}while(0);
	
	storage_print(st);
	
	File* ftrash=getfile(st, "file1");
	rmv(st, ftrash);
	file_destroy(ftrash);
	
	storage_print(st);
	
	ftrash=rmvlast(st);
	file_destroy(ftrash);
	
	storage_print(st);
	storage_destroy(st);
	
	return 0;
	}
