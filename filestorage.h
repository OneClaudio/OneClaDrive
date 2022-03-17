
typedef struct File{
	char*  name;
    void*  cont;
    size_t size;
    
    //pthread_rwlock_t flock;
    
    idlist*	openIds;
    int 	lockId;
    
    struct File* prev;
       
    } File;

File* file_create(char* filename){

void file_destroy(File* victim){




typedef struct Storage{		//THREAD SAFE STORAGE type
    File* first;
    File* last;
    
	pthread_rwlock_t stlock;
	
	size_t numfiles;
	size_t capacity;
	
	} Storage;
	
Storage* storage_create();

void storage_destroy(Storage* storage);




int addnew(Storage* storage, File* new);

File* rmvlast(Storage* storage);

File* getfile(Storage* storage, char* filename);




void writefile( Storage* storage, File* file, void* cont, size_t size);

void readfile( Storage* storage, File* file, void** cont, size_t* size);

int openfile( Storage* storage, File* file, int cid);

int closefile( Storage* storage, File* file, int cid);

int lockfile( Storage* storage, File* file, int cid);

int unlockfile( Storage* storage, File* file, int cid);
