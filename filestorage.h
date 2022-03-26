#include <pthread.h>
#include <linux/limits.h>

typedef struct File{
	char  name[PATH_MAX];
    void*  cont;
    size_t size;
    
    IdList*	openIds;
    int 	lockId;
    
    struct File* prev;
    } File;

File* fileCreate(char* filename);

int fileDestroy(File* victim);



typedef struct Storage{		//THREAD SAFE STORAGE type
    File* first;
    File* last;
    
	pthread_rwlock_t lock;
	
	size_t numfiles;
	size_t capacity;
	} Storage;
	
Storage* storage;	//GLOBAL STORAGE DECLARATION

int storageCreate();

int storageDestroy();

void storagePrint();


int addNewFile(File* new);

File* getFile(char* filename);

File* rmvLastFile();

int rmvThisFile(File* victim);
