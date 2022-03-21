ssize_t writefull(int fd, const void *buf, size_t size){
	ssize_t nwritten=0;
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

ssize_t readfull(int fd, void *buf, size_t size){
	ssize_t nread=0;
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

ssize_t freadfull(void* buf, size_t size, size_t nblocks, FILE* fd){
	ssize_t nread=0;
	
	nread=fread( buf, size, nblocks, fd);
	if(nread<size){
		if( feof(fd) ){
			fprintf(stderr, "Error: Unexpected EOF in fread\n");
			return 0;
			}
		if(ferror(fd)) fprintf(stderr, "Error: Reading error in fread\n");
		errno=EIO;
		return -1;
		}
	return 0;
	}

ssize_t fwritefull(void* buf, size_t size, size_t nblocks, FILE* fd){
	ssize_t nwritten=0;
	
	nwritten=fread( buf, size, nblocks, fd);
	if(nwritten<size){
		if( feof(fd) ) fprintf(stderr, "Error: Unexpected EOF in fwrite\n");
		if(ferror(fd)) fprintf(stderr, "Error: Reading error in fwrite\n");
		errno=EIO;
		return -1;
		}
	return 0;
	}
