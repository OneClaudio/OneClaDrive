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
	ssize_t nread = 0, n;
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
