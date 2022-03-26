
#define EMPTYLIST 1

typedef struct IdNode{			//LIST NODE	contains:
    int id;							//FILE DESCRIPTOR
    struct IdNode* prev;			// ptr to PREVIOUS node
	} IdNode;

typedef struct IdList{			//THREAD SAFE LIST type
    IdNode* first;					
    IdNode* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
    pthread_mutex_t mutex;					//    ^					  ^
	} IdList;								//   first				last
	
IdList* idListCreate();

int idListDestroy(IdList* list);

int enqId(IdList* list, int id);

int deqId(IdList* list, int* id);

bool findId(IdList* list, int id);

bool findRmvId(IdList* list, int id);
