typedef struct Opt{			//LIST Opt	contains:
    int cmd;							// single char represents command OPTIONS
    char* arglist;						// comma separated list of ARGUMENTS to the option
    struct Opt* prev;				// ptr to PREVIOUS Opt
	} Opt;

typedef struct OptQueue{			//THREAD SAFE LIST type
    Opt* first;					
    Opt* last;							// x-[ ]<-[ ]<-[ ]<-[ ]<-[ ]
										//    ^					  ^
	} OptQueue;							//   first				last

OptQueue* optQueue;

int optQueueCreate();
int optQueueDestroy();

int enqOpt(int cmd, char* arglist);
Opt* deqOpt();
void optDestroy(Opt* victim);

