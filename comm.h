typedef struct {
	CmdCode code;
	int info;	//'open' flags   or   number of files in readn/cache alg
	char filename[MAXNAME];
	} Cmd;
	
typedef enum CmdCode{
	OPEN	,
	CLOSE	,
	WRITE	,
	APPEND	,
	READ	,
	READN	,
	LOCK	,
	UNLOCK	,
	REMOVE	,
	QUIT
	} CmdCode;

typedef enum Reply{
	ANOTHER	=-3,
	CACHE	=-2,
	RETRYLK	=-1,
	OK		= 0,
	NOTFOUND   ,
	EXISTS	   ,
	LOCKED	   ,
	NOTOPEN	   ,
	NOTLOCKED  ,
	TOOBIG
	} Reply;
