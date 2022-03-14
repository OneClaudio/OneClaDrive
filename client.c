#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/un.h>

#define PIPE(p) if( pipe(p) == -1 ){									\
		fprintf(stderr, "Error while opening pipe: "); perror(NULL);	\
		return -1;														\
		}

#define CHKPIDF(pid) if( pid<0){										\
		fprintf(stderr, "Error during fork: "); perror(NULL);			\
		return -1;														\
		}
		
#define CHKPIDW(pid) if( pid<0){										\
		fprintf(stderr, "Error during wait: "); perror(NULL);			\
		return -1;														\
		}

#define BIND( id, addr, l) errno=0;				\
	if( bind( id, addr, l) != 0 ){				\
		perror("Error: couldnt name socket\n"); \
		exit(EXIT_FAILURE);						\
		}
		
#define LISTEN( id, bl) errno=0;									\
	if( listen( id, bl) != 0 ){										\
		perror("Error: couldnt listen on the specified socket\n"); 	\
		exit(EXIT_FAILURE);											\
		}
		
#define CONNECT( id, addr, l) errno=0;								\
	if( connect( id, addr, l) !=0){									\
		perror("Error: couldnt connect to the specified socket\n");	\
		exit(EXIT_FAILURE);											\
		}

#define WRITE( id, addr, l) errno=0;								\
	if( write( id, addr, l) <0 ){									\
		perror("Error during write\n");								\
		break;														\
		}

#define READ( id, addr, l) errno=0;									\
	if( read( id, addr, l) <0 ){									\
		perror("ErrorMAX_INPUT during read\n");						\
		break;														\
		}

#define SPATHNAME "./server_sol"

#define MAXLMSG 128	

int main(){
	
	char msg[MAXLMSG];
	
	int sid=socket(AF_UNIX, SOCK_STREAM, 0);
	if( sid<0){					\
		perror("Error: couldnt open socket\n");
		exit(EXIT_FAILURE);
		}
	
	struct sockaddr_un saddr;						//sid: server id (fd), SOCKET assigns it a free channel
	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family=AF_UNIX;
	strcpy(saddr.sun_path, SPATHNAME);
	
	CONNECT(sid, (struct sockaddr *)&saddr, SUN_LEN(&saddr) );		//through SPATHNAME which is the same socket name of the server side socket
	printf("Connection accepted from server\n");							//the client CONNECTS to the same open socket in the server
	
	while(1){
		printf("> ");
	
		memset(msg, '\0', MAXLMSG);				//resets result memory after each cycle to prevent the reading of garbage
	
		fgets(msg, MAXLMSG, stdin);				//read next line
		if(msg==NULL) continue;
	
		if( msg[0]=='\0') continue;
		if( msg[strlen(msg)-1]=='\n')		
			msg[strlen(msg)-1]='\0';
		else{
			fprintf(stderr, "Error: command longer than %d chars\n", MAXLMSG);
			continue;
			}
		
		int l=strlen(msg);
		
		WRITE(sid, &l, sizeof(int));				//sends to the server whatever is written by the client, even if its QUIT
		WRITE(sid, &msg, l);
		
		if( strncmp(msg, "quit", 4) == 0 )	break;	//if the client sent QUIT dont read/print anything from the server and skip tho the CLOSE
				
		READ(sid, &l, sizeof(int));					//READ the server's response
		READ(sid, &msg, l);
		msg[l]='\0';

		printf("# %s\n", msg);
		}
		
	printf("Closing connection with server\n");
	close(sid);
	printf("Client closed\n");
	return 0;
	}
/*
Realizzare un programma C che implementa un server che rimane sempre attivo in attesa di richieste da parte di uno o piu' processi client su una socket di tipo AF_UNIX.
Ogni client richiede al server la trasformazione di tutti i caratteri di una stringa da minuscoli a maiuscoli (es. ciao –> CIAO).
Per ogni nuova connessione il server lancia un thread POSIX che gestisce tutte le richieste del client (modello “un thread per connessione” – i thread sono spawnati in modalità detached) e quindi termina la sua esecuzione quando il client chiude la connessione.
Per testare il programma, lanciare piu' processi client ognuno dei quali invia una o piu' richieste al server multithreaded.
*/
