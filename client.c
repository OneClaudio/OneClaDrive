#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include "./api.h"
#include "./utils.h"		//READ(), WRITE(), FREAD(), FWRITE()
#include "./errcheck.h"
#include "./optqueue.h"

#define SPATHNAME "./server_sol"

//Used for API calls inside the main command execution while loop
//silently BREAKS out of loop: SKIPS the additional errno printings and disruptive behav inside ErrNEG1 for all API calls
//																  (the API already prints all non disruptive logic errors)
#define ErrBREAK( rv ){						\
	if( (rv)==-1 ) break;					\
	else nanosleep(&sleeptime, NULL);		\
	}

//like the previous one but silently CONTINUES loop
#define ErrCONT( rv ){						\
	if( (rv)==-1) continue;					\
	else nanosleep(&sleeptime, NULL);		\
	}

bool printmode=false;
char* trashdir=NULL;
int n_w=-1;
struct timespec sleeptime;

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
				n_w--;
				n_sent++;
				ErrCONT(  openFile(  entpathname, O_CREATE | O_LOCK, trashdir)  );		//nanosleep(&sleeptime, NULL) bundled inside ErrBREAK
				ErrCONT(  writeFile( entpathname, trashdir)  );
				ErrCONT(  closeFile( entpathname)  ); 
				}
			}
		}
	ErrNZERO( errno );
	
	ErrNEG1(  closedir(dir)  );
	return n_sent;
		
ErrCLEANUP
	fprintf(stderr,"Error: -w failed after %d files sent\n", n_sent);
	return -1; //->zero
ErrCLEAN
	}

int main (int argc, char **argv){

	char* 	socketname=NULL;
/*	bool */	printmode=false;		//moved to global bc used inside -w SENDdir()
	
	bool found_h=false;
	bool found_f=false;
	bool found_p=false;
	
	ErrNEG1(  optQueueCreate()  );
	
	int opt;
	while( (opt=getopt(argc, argv, ":w:W:a:r:R::l:u:c:d:D:t:f:ph"))!= -1){	/*
															: 	-> 1 argomento obbligatorio
															:: 	-> 1 argomento opzionale
															"nm"-> nessun argomento
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
            case 't':
            	enqOpt( opt, optarg);
            	break;

			case 'R':								//	-R uses the optional argument '::'
													//		only works if the arg is attached to '-R'	es: '-R3' OK, '-R 3' ERR: 3 unknown arg
				if( optarg==NULL) enqOpt( opt, NULL);
				else              enqOpt( opt, optarg);
				break;			
				
			case 'f':
				if( found_f ){ fprintf(stderr,"Warning: socket pathname already specified, ignored\n"); break; }
				socketname=optarg;
				found_f=true;
				break;
			
			case 'p':
				if( found_p){ fprintf(stderr,"Warning: print mode already specified, ignored\n"); break; }
				printmode=true;
				found_p=true;
				break;
						
			case 'h':
				if( found_h){ fprintf(stderr,"Warning: help prompt already visualized\n"); break; }
				
				found_h=true;
				printf(
					"------------------------------------------------------------------------------------------------------------------"
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
					"------------------------------------------------------------------------------------------------------------------");
				break;
				
			 case ':':		
  				printf("L'opzione '-%c' richiede un argomento\n", optopt);
  				errno=EINVAL;
  				return -1;
				break;
			
			case('?'):
				printf("Opzione '-%c' non riconosciuta.\n", optopt);
				errno=EINVAL;
				return -1;
				break;		
			
			default:
				printf("Come sei finito qui?\n");
				errno=EPERM;
				return -1;
				break;	
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
	
	if(socketname==NULL) socketname=SPATHNAME;
	
	ErrNEG1(  ezOpen(socketname)  );

	char* readdir=NULL;
/*	char* */trashdir=NULL;															//moved to global bc used inside -w SENDdir()
	
/*	static struct timespec sleeptime;	*/	//INTERVAL time between two API calls	//moved to global bc used inside -w SENDdir()
	sleeptime.tv_sec=0;
	sleeptime.tv_nsec=0;
	
	void* readbuf=NULL;
	size_t readsize=0;
	
	
	
	
	
	Opt* curr=NULL;
	while(	(curr=deqOpt()) !=NULL){
		ErrLOCAL;
		char* argcpy=NULL;
		if(curr->arglist!=NULL)
			argcpy=strdupa(curr->arglist);
		
		switch(curr->cmd){
			case 't':{		// -t updates sleeptime timer (in milliseconds)
				int msec=0;
				if( !isNumber(argcpy) )	fprintf(stderr, "Warning: argument of '-t' is NaN (milliseconds), sleep time unchanged\n");
				else if( (msec=atoi(argcpy)) < 0) fprintf(stderr,"Warning: argument of '-t' is negative (but should represent time),\
																  sleep time unchanged\n");
				else{
					sleeptime.tv_sec=  msec/1000;
					sleeptime.tv_nsec=(msec%1000)*1000000;
					}
				} break;	
			
			case 'd':		// -d updates readdir directory path
				readdir=strdupa(argcpy); break;
			
			case 'D':		// -D updates trashdir directory path
				trashdir=strdupa(argcpy); break;
			
			
			
			case 'R':{		// -R has an optional number argument
				int n_R=-1;		//default behaviour: read all files
				if( argcpy != NULL){
					if( !isNumber(argcpy) ){
						fprintf(stderr,"Warning: argument of '-R' is NaN\n");
						break;
						}
					else{
						n_R=atoi(argcpy);		//if n specified correctly: send at most n files
						if(n_w==0) n_w=-1;		// because 0 means send all files
						}
					}
				
				int n_read;
				ErrBREAK(  n_read=readNFiles( n_R, readdir)  );
				printf(" %d files read from FSS\n", n_read);
				} break;
				
			
			case 'w':{		// -W has a single arg or 2 comma separated args
				char* senddir=strtok_r(argcpy,",",&argcpy);	//it's impossible that is NULL
				char* n_str=strtok_r(argcpy,",",&argcpy);
				printf("NSTR=%s\n", n_str);
				
				if( strtok_r(argcpy,",",&argcpy) != NULL ){
					fprintf(stderr,"Error: -W option has more than 2 arguments in its argument list\n");
					break;
					}
					
			/*  int n_w;	*/					//moved to global bc used inside -w SENDdir()
				n_w=-1;					//default behaviour: send all files (gets dSAVEfileecremented indefinitely inside SENDdir)
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
				
				printf("N=%d\n", n_w);
				int n_sent;
				ErrBREAK(  n_sent=SENDdir(senddir)  );
				printf(" %d files sent to FSS\n", n_sent);
				} break;
			
			case 'a':{		// -a has 2 comma separated args
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
				
				FILE* file=fopen(src, "r");		
				if(file==NULL){								//ERR: file not found or other (fopen SETS ERRNO)
					if(errno==ENOENT){
						fprintf(stderr, "Error: file doesnt exist, skipping command\n");
						break;
						}
					else ErrFAIL;
					}

				struct stat srcstat;
				ErrNEG1(  stat(src, &srcstat)  );			//ERR: filestat corrupted	(stat SETS ERRNO)
				size_t size=srcstat.st_size;
	
				void* cont=NULL;
				ErrNULL(  cont=calloc(1, size)  );			//ERR: malloc failure (malloc SETS ERRNO=ENOMEM)
				FREAD(cont, size, 1, file);					//ERR: file read failure (freadfull SETS ERRNO=EIO)
				
				ErrBREAK(  appendToFile( dest, cont, size, trashdir)  );				
				} break;
				
			
			default:{		// -w -r -l -u -c  all these have a comma separated list of arguments
				char* file=NULL;
				while( (file=strtok_r( argcpy, ",", &argcpy)) !=NULL ){		//for each arg:
					ErrLOCAL;

					switch(curr->cmd){
						case 'W':
							ErrBREAK(  openFile(  file, O_CREATE | O_LOCK, trashdir)  );	//nanosleep(&sleeptime, NULL) bundled inside ErrBREAK
							ErrBREAK(  writeFile( file, trashdir)  );
							ErrBREAK(  closeFile( file)  );
							break;
						
						case 'r':
							ErrBREAK(  openFile( file, 0, trashdir)  );
							ErrBREAK(  readFile( file, &readbuf, &readsize)  );
							
							//printf("CONT: %s, SIZE: %d, PATHNAME: %s, SAVEDIR: %s\n", (char*)readbuf, readsize, file, readdir);
							ErrNEG1(  SAVEfile( readbuf, readsize, file, readdir)  );
							
							ErrBREAK(  closeFile( file)  );
							break;
						
						case 'l':
							ErrBREAK(  openFile( file, 0, trashdir)  );
							ErrBREAK(  lockFile( file)  );
							break;
							
						case 'u':
							ErrBREAK(  unlockFile( file)  );
							ErrBREAK(  closeFile( file)  );
							break;
						
						case 'c':
							ErrBREAK(  removeFile( file)  );
							break;
						}
					//free(file)????		//TODO check leaks with valgrind
				
					continue;
					ErrCLEANUP				//TODO better break jumps error msg etc
					fprintf( stderr, "Error: -%c %s FAILED (ERRNO=%d) %s\n", curr->cmd, file, errno, strerror(errno) );
					//free(file);
					//optDestroy(curr);
					break;
					ErrCLEAN
					}
				}
			}
		continue;
		ErrCLEANUP
		fprintf( stderr, "Error: -%c %s FAILED (ERRNO=%d) %s\n", curr->cmd, curr->arglist, errno, strerror(errno) );
		//optDestroy(curr);
		ErrCLEAN
		}
		
	ErrNEG1(  closeConnection(socketname)  );
	
	optQueueDestroy();
	return 0;

ErrCLEANUP
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
