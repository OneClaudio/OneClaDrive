	
typedef struct File{
	char*  name;
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    IdList*	openIds;
    int 	lockId;
    
    struct File* prev;
       
    } File;

File* fileCreate(char* filename);

void fileDestroy(File* victim);




typedef struct Storage{		//THREAD SAFE STORAGE type
    File* first;
    File* last;
    
	pthread_rwlock_t lock;
	
	size_t numfiles;
	size_t capacity;
	
	} Storage;
	
extern Storage* storage;	//GLOBAL STORAGE DECLARATION

int storageCreate();

void storageDestroy();




int addNewFile(File* new);

File* rmvLastFile();

File* getFile(char* filename);

int rmvThisFile(File* victim);




void writefile( File* file, void* cont, size_t size);

void readfile( File* file, void** cont, size_t* size);

int openfile( File* file, int cid);

int closefile( File* file, int cid);

int lockfile( File* file, int cid);

int unlockfile( File* file, int cid);
