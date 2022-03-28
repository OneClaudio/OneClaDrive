#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <errcheck.h>
#include <idlist.h>
#include <filestorage.h>

	/*------------------------in-Header------------------------*/
/*	
typedef struct File{
	char  name[PATH_MAX];
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    IdList*	openIds;
    int 	lockId;
    
    struct File* prev;
       
    } File;

typedef struct Storage{
    File* first;
    File* last;
    
	pthread_rwlock_t stlock;
	
	size_t numfiles;
	size_t capacity;
	
	} Storage;	*/

	/*-------------------Create-/-Destroy----------------------*/

File* fileCreate(char* filename){
	File* new=NULL;
	ErrNULL(  new=calloc(1,sizeof(File))  );
    
	strncpy(new->name, filename, strlen(filename));
	new->cont=NULL;
	new->size=0;
	ErrNULL(  new->openIds=idListCreate()  );
	new->lockId=-1;
    
    new->prev=NULL;

SUCCESS
    return new; 
ErrCLEANUP
	if(new) free(new); 
	return NULL;
ErrCLEAN
    }



int fileDestroy(File* victim){
	ErrNEG1(  idListDestroy(victim->openIds)  );
	if(victim->cont) free(victim->cont);
	free(victim);
	
	SUCCESS    return 0;
	ErrCLEANUP return -1; ErrCLEAN
	}



int storageCreate(){
    ErrNULL(  storage=calloc(1,sizeof(Storage))  );

    storage->first = NULL;
    storage->last  = NULL;
    
    storage->numfiles=0;
    storage->capacity=0;
    
    ErrERRNO(  pthread_rwlock_init(&storage->lock, NULL)  );

SUCCESS
	return 0;
ErrCLEANUP
	if(storage) free(storage);
	return -1; 
ErrCLEAN
	}



int storageDestroy(){		//deallocates a STORAGE
	ErrNULL(storage);		//STORAGE must have been CREATED
	
	File* curr=storage->last;
	
	File* temp=NULL;
	while( curr!=NULL){
		temp=curr->prev;
		fileDestroy(curr);	//if this one FAILS, ERR is NOTIFIED INTERNALLY
		curr=temp;
		}	
	
	ErrERRNO(  pthread_rwlock_destroy(&storage->lock)  );
	free(storage);
	return 0;

ErrCLEANUP
	if(storage) free(storage);
	return -1;
ErrCLEAN
	}

	/*-------------------Functionalities-----------------------*/
	
int addNewFile( File* new){
	ErrNULL(storage);		//STORAGE must have been CREATED
	
    if (storage->first==NULL || storage->last==NULL){	//if STORAGE EMPTY first and last point to the new element
        storage->first=new;
        storage->last=new;
    	}
    else{        									//STORAGE MULTIPLE NODES
        storage->first->prev=new;						//the first becomes the second in the storage
        storage->first=new;								//the new file becomes the first
        }
   
	storage->numfiles++;
	return 0;

ErrCLEANUP
	fprintf(stderr,"Error: before using storage functionalities you must call storageCreate()\n");
	return -1;
ErrCLEAN
	}



File* getFile( char* filename){		//getFile() LOOKS FOR a FILE and RETURNS a REFERENCE to it
	
    File* curr=storage->last;

    while(curr!=NULL){
    	if( strcmp(curr->name, filename) == 0) break;
    	curr=curr->prev;
		}
									//curr==SEARCHED FILE (found it and broke out of while loop)
    return curr;					//curr==NULL 		  (if whole list traversed and the file wasnt found)
	}



File* rmvLastFile(){				//rmvLastFile() removes the LAST file from STORAGE. The removed file is RETURNED
										//the worker thread has the duty to destroy the file with fileDestroy()										
    File* victim=storage->last;
    	
	if(storage->first==NULL) return NULL;		//if the STORAGE is EMPTY returns NULL

	if(storage->first==storage->last){			//if victim is the ONLY FILE LEFT in the storage, the first and last are set to NULL (EMPTY state)
		storage->first=NULL;
		storage->last=NULL;
		}
	else storage->last = storage->last->prev;	//if the STORAGE has MULTIPLE FILES
    
    storage->numfiles--;
	storage->capacity -= victim->size;
    
    return victim;
	}



int rmvThisFile( File* victim){		//rmvThisFile() finds and removes a SPECIFIC FILE from STORAGE. Returns 0 if successful
	ErrNULL(storage)					//the worker must already have a file reference (must call getFile() first)
										//the worker thread has the duty to destroy the file with fileDestroy()							
    File* curr=storage->last;
    
    if( storage->last==NULL) return -1;			//-1: if STORAGE is EMPTY (VICTIM not FOUND)
	
	if( storage->last==victim ){				//if the VICTIM is the LAST the removeLastFile() can handle it
		ErrNULL(  rmvLastFile()  );				//also handles the case VICTIM is the ONLY ONE LEFT
		return 0;	//(returns only if rmvLastFile() has succeded)
		}
	
	while(curr!=NULL){							//traverse the FILE STORAGE list and search FILE
		if( curr->prev==victim)	break;			//when FILE is found CURR is the NODE before it
		curr=curr->prev;
		}
	if(curr){
		if(victim == storage->first){			//if the VICTIM is the FIRST in the list CURR becomes the FIRST
			storage->first=curr;
			curr->prev=NULL;					//																	              ________
			}									//																	             v        \       ...
		else curr->prev=victim->prev;			//if the VICTIM is IN THE MIDDLE of the LIST, it is JUMPED over:          ... <-[ ]<-[X] [ ]<-[ ] ...	
																	
		storage->numfiles--;
		storage->capacity -= victim->size;
		return  0;			// O: VICTIM FOUND and RMVD
		}
	else return -1;			//-1: VICTIM not FOUND
	
	ErrCLEANUP return -1; ErrCLEAN
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
	} */
