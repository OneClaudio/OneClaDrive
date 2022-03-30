#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>

#include <api.h>
#include <utils.h>		//READ(), WRITE(), FREAD(), FWRITE()
#include <errcheck.h>
#include <optqueue.h>

#define SOCKETPATHN "./serversocket"

volatile sig_atomic_t SigPipe=0;

void handlepipe(int unused){
	SigPipe=1;
	fprintf(stderr, "ERR: UNEXPECTED SERVER CRASH\n");
	}

//Used for API calls inside the main command execution while loop
//on ERROR: PRINTS API  BREAKS out of switch loop, PRINTS ERROR and SLEEPS (as much as required by -t OPTION) / on SUCCESS: SLEEPS anyway
#define BrkErrAPI( call ){						\
	if( (call)==-1 ){						\
		fprintf( stderr, "C%d Error: (-%c) %s FAILED (ERRNO=%d) %s\n", CPID, curr->cmd, file, errno, strerror(errno) );		\
		nanosleep(&sleeptime, NULL);		\
		break;								\
		}									\
	else nanosleep(&sleeptime, NULL);		\
	}

//like the previous one but CONTINUES (eg: inside while )
#define CntErrAPI( call ){					\
	if( (call)==-1){						\
		fprintf( stderr, "C%d Error: %s on file %s FAILED (ERRNO=%d) %s\n", CPID, #call, ent->d_name, errno, strerror(errno) );		\
		nanosleep(&sleeptime, NULL);		\
		continue;							\
		}									\
	else nanosleep(&sleeptime, NULL);		\
	}

char* trashdir=NULL;
int n_w=-1;
struct timespec sleeptime;
pid_t CPID=NOTSET;	//Client Process ID

static inline double timespectot( struct timespec* t){
	return t->tv_sec+(t->tv_nsec/1000000000.0);
	}

int SENDdir(char* senddir){		//used in -w option
	
	int n_sent=0;				//counts the num of SENT files
	
	DIR* dir=NULL;
	ErrNULL(  dir=opendir(senddir)  );
	
	struct dirent* ent;				
	while( (errno=0, ent=readdir(dir)) != NULL && n_w!=0){
		//if( strcmp(ent->d_name,".")!=0  ||  strcmp(ent->d_name,"..")!=0 ) continue;
		
		if( strlen(senddir)+1+strlen(ent->d_name) > PATH_MAX-1 ){
			fprintf(stderr,"Error: resulting pathname longer than %d chars, file '%s' wont be saved\n", PATH_MAX-1, ent->d_name);
			continue;
			}
		
		char entpathname[PATH_MAX];
		memset( entpathname, '\0', PATH_MAX);
		strcpy( entpathname, senddir );
		strcat( entpathname, "/");
		strcat( entpathname, ent->d_name);
	
		struct stat entstat;
		ErrNEG1( stat(entpathname, &entstat)  );
		
		if( strcmp(ent->d_name,".")!=0  &&  strcmp(ent->d_name,"..")!=0 ){
			if( S_ISDIR(entstat.st_mode) ) n_sent+= SENDdir(entpathname);
			else{
				n_w--;			//with these increments here the behaviour is:	do N WRITE ATTEMPTS
				n_sent++;		//if these are moved after the 3 API CALLS:		do N SUCCESSFUL WRITES
				if(PRINT) printf("\n\tCREATE>WRITE>CLOSE of FILE %s\n", entpathname);
				CntErrAPI(  openFile(  entpathname, O_CREATE | O_LOCK, trashdir)  );	if(PRINT) printf("\tOPEN: SUCCESS\n");
				CntErrAPI(  writeFile( entpathname, trashdir)  );						if(PRINT) printf("\tWRITE: SUCCESS");
				CntErrAPI(  closeFile( entpathname)  );								if(PRINT) printf("\tCLOSE: SUCCESS\n");
				}
			}
		}
	ErrNZERO( errno );
	
	ErrNEG1(  closedir(dir)  );
	return n_sent;
		
ErrCLEANUP
	if(dir) closedir(dir);
	return -1;
ErrCLEAN
	}

int main (int argc, char **argv){

	char* 	socketname=NULL;
/*	bool */	PRINT=false;		//moved to API.h bc its needed inside the API to print the SIZEs on some actions
	
	bool found_h=false;
	bool found_f=false;
	bool found_p=false;
	
	ErrNEG1(  optQueueCreate()  );
	
	int opt;
	while( (opt=getopt(argc, argv, ":w:W:a:r:R::l:u:c:d:D:t:f:ph"))!= -1){	/*
															: 	-> 1 argomento obbligatorio
															:: 	-> 1 argomento opzionale
															"ph"-> nessun argomento
															-1	-> finite le opzioni
															
															first ':' -> serve per riconoscere il "missing argument" ma funziona solo sull'ultima opt
																		 le precedenti prendono come arg la opt successiva
															
															optarg: contiene un ptr all'argomento dell'opzione corrente
															optind: posizione corrente della funzione getopt sul vettore argv[]
															optopt: se getopt incontra un carattere non riconosciuto viene memorizzato qui */
		switch(opt){
			case 'w':
            case 'W':
            case 'a':
            case 'r':
            case 'l':
            case 'u':
            case 'c':
            case 'd':
            case 'D':
            case 't':									//-R uses the optional argument '::' => 'optarg' can be NULL and it's correct behaviour
            case 'R':									// /!\ only works if the arg is adjacent to '-R' ES OK:'-R3'  ERR:'-R 3' 3 unknown arg
            	enqOpt( opt, optarg);
            	break;			
				
			case 'f':
				if( found_f ){ fprintf(stderr,"Warning: socket pathname already specified, ignored\n"); break; }
				socketname=optarg;
				found_f=true;
				break;
			
			case 'p':
				if( found_p){ fprintf(stderr,"Warning: print mode already specified, ignored\n"); break; }
				PRINT=true;
				found_p=true;
				break;
						
			case 'h':
				if( found_h){ fprintf(stderr,"Warning: help prompt already visualized\n"); break; }
				
				found_h=true;
				printf(
					"------------------------------------------------------------------------------------------------------------------\n"
					"WARNING:           All options/actions are executed in the order in which are written (except -f -p -h)\n"
					"OPTIONS:\n"
					" -f socketname      Specifies the socket name used by the server [important] (default='./server_sol')\n"
					" -d dirname         Updates folder where to save the files read by the -r and -R commands (optional)\n"
					" -D dirname         Updates folder where to save files ejected from the server, for use with -w and -W (optional)\n"
					" -t time            Changes time in milliseconds between two consecutive requests (optional) (default=0)\n"
					" -p                 Enable verbose mode (optional)\n"
					"ACTIONS:\n"
					" -w dirname[,<n>]   Writes all files in 'dirname' to the server; if present stops at 'n' files sent (0 or absent means send all)\n"
					" -W f1[,f2,f3...]   Sends the specified file list to the server\n"
					" -r f1[,f2,f3...]   Reads a list of files from the server\n"
					" -R[<n>]            Reads 'n' files from the server; if 'n' is unspecified or <=zero reads all of them;\n"
					" -l f1[,f2,f3...]   Acquire read/write lock of the specified file list\n"
					" -u f1[,f2,f3...]   Releases read/write lock of the specified file list\n"
					" -c f1[,f2,f3...]   Deletes the specified file list from the server\n"
					"HELP:\n"
					" -h                 Print this message once\n"
					"------------------------------------------------------------------------------------------------------------------\n");
				break;
				
			 case ':':		
  				printf("L'opzione '-%c' richiede un argomento\n", optopt);
  				return -1; break;
			
			case('?'):
				printf("Opzione '-%c' non riconosciuta.\n", optopt);
				return -1; break;		
			
			default:
				printf("Come sei finito qui?\n");
				errno=EPERM; break;	
			}
		}
	
	
	
	if(optind < argc){
		fprintf(stderr,"Unknown options/arguments are present:\n");
		while(optind < argc){
			printf("%s\n", argv[optind]);
			optind++;
			}
		fprintf(stderr,"Reminder: option '-R' uses the optional arg function included in getopt() spec.\n"
					   "\t'-R   ': read all files\n"
					   "\t'-R3  ': read at most 3 files\n"
					   "\t'-R 3 ': '3' not recognized, could break all subsequent cmd line options, behaves like '-R  '\n"); 
		}


	char* readdir=NULL;
/*	char* */trashdir=NULL;				  //moved to global bc used inside -w auxiliary function ( SENDdir() )
	
/*	static struct timespec sleeptime;	*///INTERVAL time between two API calls
	sleeptime.tv_sec=0;					  //moved to global bc used inside -w SENDdir()
	sleeptime.tv_nsec=0;
	
	
	if(!found_f) socketname=SOCKETPATHN;	//If not present starts anyway with DEFAULT SOCKET macro
	fprintf(stdout,"CONN: CONNECTION to SERVER (%s)\n", socketname); fflush(stdout);
	ErrNEG1(  openConnection(socketname, 0, sleeptime)  );		//ignoring the timed connection for now
	
	struct sigaction s={0};		//HANDLING SIGPIPE
	s.sa_handler=&handlepipe;	
	ErrNEG1(  sigaction(SIGPIPE,  &s, NULL)  );
	
	CPID=getpid();
	
	
	Opt* curr=NULL;
	while(	(curr=deqOpt()) !=NULL){
		if(PRINT) printf("------------------ -%c:\n", curr->cmd);
		char* file=NULL; 		//used in the nested while loop when tokenizing each argument in single files
		
		char* argcpy=NULL;
		if(curr->arglist!=NULL)
			argcpy=strdupa(curr->arglist);		//AUTOMATIC STRDUP noice
		
		switch(curr->cmd){
		
		
		
			case 't':{		// -t updates sleeptime timer (in milliseconds)
				int msec=0;
				if( !isNumber(argcpy) )	fprintf(stderr, "Warning: argument of '-t' is NaN (milliseconds), sleep time unchanged\n");
				else if( (msec=atoi(argcpy)) < 0) fprintf(stderr,"Warning: argument of '-t' is negative (but should represent time),\
																  sleep time unchanged\n");
				else{
					sleeptime.tv_sec=  msec/1000;
					sleeptime.tv_nsec=(msec%1000)*1000000;
					
					if(PRINT) printf("CMD: Updated TIME INTERVAL between API CALLS: %f\n", timespectot(&sleeptime) );
					}
				} break;	
			
			
			case 'd':		// -d updates readdir directory path
				readdir=strdupa(argcpy); 
				if(PRINT) printf("CMD: Updated READ DIRECTORY: %s\n", readdir );
				break;
			
			
			case 'D':		// -D updates trashdir directory path
				trashdir=strdupa(argcpy);
				if(PRINT) printf("CMD: Updated CACHE EJECTION DIRECTORY: %s\n", readdir );				
				break;
			
			
			
			case 'R':{		// -R has an optional number argument
				int n_R=-1;		//default behaviour: read all files
				if( argcpy != NULL){
					if( !isNumber(argcpy) ){
						fprintf(stderr,"Warning: argument of '-R' is NaN\n");
						break;
						}
					else{
						n_R=atoi(argcpy);		//if n specified correctly: send at most n files
						if(n_R==0) n_R=-1;		// because 0 means send all files
						}
					}
				
				if(PRINT) printf("REQ: BULK READ of %s FILES from FSS\n\n", n_R<0 ? "ALL" : argcpy );
				
				int n_read;
				BrkErrAPI(  n_read=readNFiles( n_R, readdir)  ); // API CALL (eventual ERR PRINTING and immediate BREAK) (SLEEPS in both cases)
				
				if(PRINT){
					if(n_read<0) printf("ERR: BULK READ NOT COMPLETED\n");
					else       printf("\nRES: %d FILES READ SUCCESSFULLY\n", n_read);
					}
				} break;
				
			
			
			case 'w':{		// -W has a single arg or 2 comma separated args
				char* senddir=strtok_r(argcpy,",",&argcpy);	//it's impossible that is NULL
				char* n_str=strtok_r(argcpy,",",&argcpy);
				
				if( strtok_r(argcpy,",",&argcpy) != NULL ){
					fprintf(stderr,"Error: -W option has more than 2 arguments in its argument list\n");
					break;
					}
					
			/*  int n_w;	*/					//moved to global bc used inside -w SENDdir()
				n_w=-1;					//default behaviour: send all files (gets decremented indefinitely inside SENDdir)
				if( n_str!=NULL){
					if( !isNumber(n_str) ){
						fprintf(stderr,"Warning: second arg of '-W' is NaN\n");
						break;
						}													       
					else{
						n_w=atoi(n_str);		//if n specified correctly: send at most n files
						if(n_w==0) n_w=-1;		// because 0 means send all files
						}
					}
					
				if(PRINT) printf("REQ: FULL DIR WRITE of at most %s FILES from DIR %s\n", n_w<0 ? "ALL" : argcpy, senddir);
				
				int n_sent;
				n_sent=SENDdir(senddir);
				
				if(PRINT){
					if(n_sent<0) printf("ERR: DIR WRITE NOT COMPLETED\n");
					else       printf("\nRES: %d FILES SENT SUCCESSFULLY\n", n_sent);
					}
				} break;
			
			
			
			case 'a':{		// -a has 2 comma separated args
				ErrLOCAL
				char* src=strtok_r(argcpy,",",&argcpy);
				char* dest=strtok_r(argcpy,",",&argcpy);
				if( dest==NULL){
					fprintf(stderr,"Error: target file missing from -a option\n");
					break;
					}
				if( strtok_r(argcpy,",",&argcpy) != NULL ){
					fprintf(stderr,"Error: -a option has more than 2 arguments in its argument list\n");
					break;
					}
				

				struct stat srcstat;
				ErrNEG1(  stat(src, &srcstat)  );			//ERR: filestat corrupted	(stat SETS ERRNO)
				size_t size=srcstat.st_size;
	
				void* cont=NULL;
				ErrNULL(  cont=calloc(1, size)  );				//ERR: malloc failure (malloc SETS ERRNO=ENOMEM)
				
				
				
				
				FILE* source=fopen(src, "rb");		
				if(source==NULL){								//ERR: file not found or other (fopen SETS ERRNO)
					if(errno==ENOENT){
						fprintf(stderr, "Error: file to append doesnt exist, skipping command\n");
						break;
						}
					else ErrFAIL;
					}
				FREAD(cont, size, 1, source);					//ERR: file read failure (freadfull SETS ERRNO=EIO)
				ErrNZERO(  fclose(source)  );					//ERR: fclose() returns EOF on error (SETS ERRNO)
				
				if(PRINT) printf("API: APPEND FILE %s to FILE %s", src, dest);
				int r=-1;
				r=appendToFile( dest, cont, size, trashdir);	// /!\ BrkErrAPI didnt enter CLEANUP CODE
				if(PRINT){
					if(r<0) printf("ERR: APPEND FAILED\n");
					else    printf("RES: SUCCESS\t %zuB WRITTEN\n", size);
					}
				
			SUCCESS
				free(cont);
				break;
			ErrCLEANUP
				if(cont) free(cont);
				if(source) fclose(source);
				break;
			ErrCLEAN		
				}
				
			
			
			default:{		// -w -r -l -u -c  all these have a comma separated list of arguments
				char* file=NULL;
				while( (file=strtok_r( argcpy, ",", &argcpy)) !=NULL && !SigPipe ){		//FOR EACH FILE in its arglist:

					switch(curr->cmd){										//call the API function relative to the current OPT
						case 'W':
							if(PRINT) printf("REQ: CREATE>WRITE>CLOSE of FILE %s\n", file);
							
							BrkErrAPI(  openFile(  file, O_CREATE | O_LOCK, trashdir)  );	if(PRINT) printf("\tOPEN: SUCCESS\n");
							BrkErrAPI(  writeFile( file, trashdir)  );						if(PRINT){printf("\tWRITE: SUCCESS"); fflush(stdout); }
							BrkErrAPI(  closeFile( file)  );								if(PRINT) printf("\tCLOSE: SUCCESS\n");
							break;
						
						case 'r':{
							if(PRINT) printf("REQ: OPEN>READ>CLOSE of FILE %s\n", file);
							void* readbuf=NULL;
							size_t readsize=0;
							BrkErrAPI(  openFile( file, 0, trashdir)  );					if(PRINT) printf("\tOPEN: SUCCESS\n");
							BrkErrAPI(  readFile( file, &readbuf, &readsize)  );			if(PRINT) printf("\tREAD: SUCCESS");
																						if(PRINT){printf("\t %zuB READ", readsize); fflush(stdout); }
							ErrNEG1(  SAVEfile( readbuf, readsize, file, readdir)  );
							free(readbuf);
							
							BrkErrAPI(  closeFile( file)  );								if(PRINT) printf("\tCLOSE: SUCCESS\n");
							}
							break;
						
						case 'l':
							if(PRINT) printf("REQ: OPEN>LOCK of FILE %s\n", file);
							BrkErrAPI(  openFile( file, 0, trashdir)  );					if(PRINT) printf("\tOPEN: SUCCESS\n");
							BrkErrAPI(  lockFile( file)  );								if(PRINT) printf("\tLOCK: SUCCESS\n");
							break;
							
						case 'u':
							if(PRINT) printf("REQ: UNLOCK>CLOSE of FILE %s\n", file);
							BrkErrAPI(  unlockFile( file)  );
							BrkErrAPI(  closeFile( file)  );
							break;
						
						case 'c':
							if(PRINT) printf("REQ: REMOVAL of FILE %s\n", file);
							BrkErrAPI(  removeFile( file)  );								if(PRINT) printf("\tREMOVE: SUCCESS\n");
							break;
						}
					if(PRINT) printf("\n");
					if(PRINT) fflush(stdout);
					}
				} break;
				
				
			}
		free(curr);		//Destroys the dequeued OPT NODE
		}
		
	ErrNEG1(  closeConnection(socketname)  );
	
	if(PRINT) printf("END: CLIENT FINISHED all TASKS and DISCONNECTED");
	
	ErrNEG1(  optQueueDestroy()  );
	
	return 0;

ErrCLEANUP
	if(optQueue) optQueueDestroy();
	return -1;
ErrCLEAN
	}
	
/*
\tDefault behaviour: all files in the dir will be sent to the FSS\n

						if(n_w<0){
							fprintf(stderr,"Error: -w option requires n=0 or no n to send all files in a dir, or n!=0 to send at most n files\n");
							ErrFAIL;
							}

fprintf(stderr,"Errorr: -w error while opening specified dir, %s\n", strerror(errno));
*/
