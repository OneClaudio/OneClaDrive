
CC = gcc
LIBS = -pthread
CFLAGS = -std=gnu99 -g -Wall -D_GNU_SOURCE #-Wextra

INC = -I.



server: server.o filestorage.o idlist.o
	gcc $(LIBS) $(CFLAGS) $(INC) $^ -o $@

dummy:	dummyclient.o  api.o
	gcc $(LIBS) $(CFLAGS) $(INC) $^ -o $@
	
client: client.o api.o optqueue.o
	gcc $(LIBS) $(CFLAGS) $(INC) $^ -o $@

#filestorage: filestorage.o idlist.o
#	gcc $(LIBS) $(CFLAGS) -o $@ $^
	
filestorage.o: filestorage.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $<

server.o: server.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $<

api.o: api.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $<
	

#idlist.o: idlist.c
#	gcc $(LIBS) $(CFLAGS) -c $<
	

