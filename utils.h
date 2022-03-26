#ifndef UTILITIES_H
#define UTILITIES_H

//others
#include <limits.h>

#define NOTSET -1

static inline int isNumber( char* str){
	char* e=NULL;
	errno=0;
	long int val=strtol(str, &e,10); 	//dont care about converted value rn
	if( e==NULL || !(*e=='\n' || *e=='\0') ){
		fprintf(stderr, "Error: string %s is NaN\n", str);
		return 0;
		}
	if( errno == ERANGE && (val == LONG_MAX || val == LONG_MIN) ){
		fprintf(stderr, "Error: string %s is an out of range number\n", str);
		return 0;
		}
	if( errno != 0 && val == 0){
		fprintf(stderr, "Error: the string si empty\n");
		return 0;
		}
	return 1;
	}

/*------------------------------------------------READ/WRITE-SC------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------------------------*/
static inline ssize_t writefull(int fd, const void *buf, size_t size){
	size_t nwritten=0;
	ssize_t n;
	do{
		n=write(fd, buf, size-nwritten);			//book uses:   buf  ->  &((const char *)buf)[nwritten] for extra safety
		if( n==-1){									//-1 means ERROR
			if (errno==EINTR || errno==EAGAIN) continue;	//but these are NON DISRUPTIVE and the read can be tried again
			else return -1;									//otherwise FATAL ERROR
			}
				
		nwritten+=n;
		
		}while( nwritten<size );

	return nwritten;
	}

static inline ssize_t readfull(int fd, void *buf, size_t size){
	size_t nread=0;
	ssize_t n;
	do{
		n=read(fd, buf, size-nread);				//book uses:   buf  ->  &((char *)buf)[nwritten] for extra safety
		if( n==-1){									//-1 means ERROR
			if(errno == EINTR || errno==EAGAIN) continue;	//but these are NON DISRUPTIVE and the read can be tried again
			else return -1;									//otherwise FATAL ERROR
			}
		
		if (n==0)	break;							//0 means EOF
		
		nread+=n;
			
		} while( nread<size );
		
	return nread;
	}
	
#define WRITE( id, addr, size) ErrNEG1( writefull(id, addr, size)  );
	/*	Strict version:		   ErrDIFF( writefull(id, addr, size),  size);	*/
		
#define READ( id, addr, size) ErrNEG1( readfull(id, addr, size)  );
	/*	Strict version:		  ErrDIFF( readfull(id, addr, size),  size);	*/

/*-----------------------------------------------FREAD/FWRITE-LIB----------------------------------------------------*/
/*-------------------------------------------------------------------------------------------------------------------*/
//TODO get to the end of this vvvv
static inline ssize_t freadfull(void* buf, size_t size, size_t nblocks, FILE* fd){
	ssize_t nread=0;
	nread=fread( buf, size, nblocks, fd);
	if(nread<size){
		if( feof(fd) ){
			fprintf(stderr, "Error: Unexpected EOF in fread\n");
			return 0;
			}
		else if(ferror(fd)){
			fprintf(stderr, "Error: Reading error in fread\n");
			errno=EIO;
			return -1;
			}
		}
	return 0;
	}

static inline ssize_t fwritefull(void* buf, size_t size, size_t nblocks, FILE* fd){
	ssize_t nwritten=0;
	
	nwritten=fread( buf, size, nblocks, fd);
	if(nwritten<size){
		if( feof(fd) ){
			fprintf(stderr, "Error: Unexpected EOF in fread\n");
			return 0;
			}
		else if(ferror(fd)){
			fprintf(stderr, "Error: Reading error in fread\n");
			errno=EIO;
			return -1;
			}
		}
	return 0;
	}

#define FWRITE( addr, size, n, id) ErrNEG1(     fwrite( addr, size, n, id) );
	/*	strict version:		           ErrNEG1( fwritefull( addr, size, n, id) );	*/
		



#define FREAD( addr, size, n, id) ErrNEG1(     fread( addr, size, n, id)  );
	/*	strict version:		          ErrNEG1( freadfull( addr, size, n, id)  );	*/

#endif
