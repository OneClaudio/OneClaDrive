typedef struct node{			//LIST NODE	contains:
    int id;							//FILE DESCRIPTOR
    struct node* prev;				// ptr to PREVIOUS node
	} node;

typedef struct idlist{			//THREAD SAFE LIST type
    node* first;					
    node* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
    pthread_mutex_t mutex;				//    ^					  ^
	} idlist;							//   first				last
	
idlist* idlist_create();

int enq(idlist* list, int id);

int deq(idlist* list, int* id);

int find(idlist* list, int id);

int findrmv(idlist* list, int id);

int idlist_destroy(idlist* list);
