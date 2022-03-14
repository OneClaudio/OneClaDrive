#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "./idlist.h"

int readn(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = read((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;  // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}

int writen(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = write((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        left -= r;
        bufptr += r;
    }
    return 1;
}

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

typedef struct file{
	char*  name;
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    idlist* idopen;
    int idlock;
    
    struct file* prev;
       
    } file;

typedef struct storagelist{		//THREAD SAFE STORAGE type
    file* first;
    file* last;
    
	pthread_rwlock_t stlock;
	
	size_t nfiles;
	size_t capacity;
	
	} storagelist;
	
file* file_create(char* filename){
	file* new=malloc(sizeof(file));
    if(new==NULL){
    	errno=ENOMEM;
    	fprintf(stderr,"Error: malloc inside storage addnew() call\n");
    	return NULL;
    	}
    
    
    
	new->name=strndup( filename, strlen(filename) );
	new->cont=NULL;
	new->size=0;
	new->idopen=idlist_create();
	new->idlock=0;
    
    //pthread_rwlock_init(&new->flock, NULL);
    
    new->prev=NULL;
    
    return new;
    }

void file_destroy(file* victim){
	free(victim->name);
	if(victim->cont) free(victim->cont);
	idlist_destroy(victim->idopen);
	//pthread_rwlock_destroy(&victim->flock);
	free(victim);
	}

storagelist* storagelist_create(){			//allocates a new THREAD SAFE STORAGE
    storagelist* storage=malloc(sizeof(storagelist));
    if(storage==NULL) return NULL;					//on ERROR:		returns NULL

    storage->first = NULL;
    storage->last  = NULL;
    
    storage->nfiles=0;
    storage->capacity=0;
    
    pthread_rwlock_init(&storage->stlock, NULL);
    
    return storage;									//on SUCCESS:	returns a ptr to the STORAGE
	}

void storagelist_destroy(storagelist* storage){		//deallocates a STORAGE

	file* curr=storage->last;
	
	file* prev=NULL;
	
	while( curr!=NULL){
		prev=curr->prev;
		printf("Deleting '%s'\n", curr->name);
		file_destroy(curr);
		curr=prev;
		}	
	
	pthread_rwlock_destroy(&storage->stlock);			//TODO
	free(storage);
	}
	
int addnew(storagelist* storage, file* new){

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
    return 0;									//SUCCESS
	}

int rmvlast(storagelist* storage, file* victim){	//RMVLAST( storage, victim) removes the LAST file from STORAGE to make room
														//its not deallocated but only passed to the thread via VICTIM ptr
    //WRLOCK(&storage->stlock);
	
    victim=storage->last;
    
	if(victim == NULL){								//if the STORAGE is empty exits with a code <--
        //UNLOCK(&storage->stlock);							//STORAGE EMPTY
        return -1;
    	}
	if(storage->first==storage->last){					//if victim is the last file in the storage, the first and last are resetted to null (initial state)
		storage->first=NULL;								//STORAGE 1 NODE
		storage->last=NULL;
		}
	else storage->last = storage->last->prev;				//STORAGE MULTIPLE NODES
	
    //UNLOCK(&storage->stlock);
    
    return 0;										//if the STORAGE wasnt empty exits with 0 <--
	}
	
file* findfile(storagelist* storage, char* filename){
	
	//RDLOCK(&storage->stlock);
    file* curr=storage->last;
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

void writefile( storagelist* storage, file* file, void* cont, size_t size){
	
	file->cont=cont;
	file->size+=size;
	
	storage->nfiles++;
	storage->capacity+=size;
	}

void readfile( storagelist* storage, file* file, void** cont, size_t* size){
	*size=file->size;
    *cont=malloc(*size);
    memcpy(*cont, file->cont, file->size);
	}

int openfile( storagelist* storage, file* file, int cid){
	return enq(file->idopen, cid);
	}

int closefile( storagelist* storage, file* file, int cid){
	return findrmv(file->idopen, cid);
	}
	
int lockfile( storagelist* storage, file* file, int cid){
	if( file->idlock != 0) return -1;
	
	file->idlock=cid;
	return 0;
	}
	
int unlockfile( storagelist* storage, file* file, int cid){
	if( file->idlock != cid) return -1;
	
	file->idlock=0;
	return 0;
	}

void printstorage(storagelist* storage){
	file* curr=storage->last;
	while( curr!=NULL){
		printf(" %s |", curr->name);
		curr=curr->prev;
		}
	printf("\n");
	}
	

int main(){
	
	storagelist* st=storagelist_create();

	do{		
		file* f1=file_create("file1");		
		addnew(st, f1);
		
		printstorage(st);
	
		file* f2=file_create("file2");
		addnew(st, f2);	
		
		printstorage(st);
	}while(0);
	
	do{
		file* f1=findfile(st,"file1");
		if (f1==NULL) break;
		int cid=4;
		
		printf("found file %s\n", f1->name);
		
		openfile(st, f1, cid);
		if( find(f1->idopen, cid) ) printf("%d succ opnd file f1\n", cid);
		
		lockfile(st, f1, cid);
		printf("%d has lockd f1\n", f1->idlock);
		
		char* buf=strdup("ALLAH is great!");
		writefile( st, f1, buf, strlen(buf)+1 );
		
		printf("f1 now contains: %s\n", (char*)f1->cont);
		
	} while(0); 
	
	do{
		file* f1=findfile(st,"file1");
		if(f1==NULL) break;
		
		int cid=4;
		void* buf=NULL;
		size_t size;
		
		if( f1->idlock==cid)
			readfile(st, f1, &buf, &size);

		printf("read from f1: %s\n", (char*)buf);	//TODO
		
		free(buf);
		
		printf("cont of f1 intact: %s\n", (char*)f1->cont);
		
		unlockfile(st, f1, cid);
		closefile(st, f1, cid);	
		
	}while(0);
	
	printstorage(st);
	storagelist_destroy(st);
	
	return 0;
	}

