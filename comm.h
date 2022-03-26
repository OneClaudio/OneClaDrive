
#include <linux/limits.h>	//PATH_MAX

typedef enum CmdCode{
	IDLE    ,
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
	ALROPEN	   ,
	NOTLOCKED  ,
	ALRLOCKED  ,
	EMPTY	   ,
	NOTEMPTY   ,
	TOOBIG
	} Reply;
	
#define	O_CREATE 0x1
#define O_LOCK	 0x2

typedef struct {
	CmdCode code;
	int info;		//'open' flags   or   number of files in readn/cache alg
	char filename[PATH_MAX];
	} Cmd;
