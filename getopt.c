#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<errno.h>

/*
If getopt() encounters an option character that is not contained in optstring, it shall return the <question-mark> ( '?' ) character.
If it detects a missing option-argument, it shall return the <colon> character ( ':' ) if the first character of optstring was a <colon>, or a <question-mark> character ( '?' ) otherwise.
*/

int main (int argc, char **argv) {
	
	int opt;
	while( (opt=getopt(argc, argv, ":w:W:r:R::d:D:c:l:u:f:ph"))!= -1){	/*
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
            case 'r':
            case 'l':
            case 'u':
            case 'c':
            	printf("-%c: %s\n", opt, optarg);
				break;
			
			case 'R':										// TODO -R uses the optional argument '::'
				if(optarg==NULL) 	printf("-%c\n",opt);		// only works if the arg is attached to '-R'	es: '-R3' OK, '-R 3' ERR: 3 unknown arg
				else 		printf("-%c: %s\n", opt, optarg);	
				break;
				
			case 'f':
				printf("socket: %s\n", optarg);
				break;
				
			case 'd':
				printf("read dir: %s\n", optarg);
				break;
			
			case 'D':
				printf("bin dir: %s\n", optarg);
				break;
				
			case 'p':
				printf("print enabled\n");
				break;
						
			case 'h':
				printf(
					"-w dirname[,n=0]  Sends the files in the <dirname> folder to the server; <n> specifies an upper limit\n"
					"-W file1[,file2]  Sends the specified file list to the server\n"
					"-r file1[,file2]  Reads a list of files from the server\n"
					"-R [n=0]          Reads n files from the server; if n is unspecified or zero, it reads all of them\n"
					"-d dirname        Folder to save the files read by the -r and -R commands (optional)\n"
					"-D dirname        Folder to save files ejected from the server, for use with -w and -W (optional)\n"
					"-l file1[,file2]  Acquire the mutual exclusion of the specified file list\n"
					"-u file1[,file2]  Releases the mutual exclusion of the specified file list\n"
					"-c file1[,file2]  Deletes the specified file list from the server\n"
					"-t time           Time in milliseconds between two consecutive requests (optional)\n"
					"-h                Print this message and exit\n"
					"-p                Enable verbose mode\n"
					"-f socketname     Specifies the socket name used by the server\n");
				return 0;
				break;
				
			 case ':':		
  				printf("L'opzione '-%c' richiede un argomento\n", optopt);
				break;
			
			case('?'):
				printf("Opzione '-%c' non riconosciuta.\n", optopt);
				return EPERM;
				break;		
			
			default:
				printf("Come sei finito qui?\n");
				return EPERM;
				break;	
			}
		}
	
	if(optind < argc){
		printf("Sono presenti ulteriori opzioni/argomenti sconosciuti:\n");
		while(optind < argc){
			printf("%s\n", argv[optind]);
			optind++;
			}
		}
	return 0;
	}
