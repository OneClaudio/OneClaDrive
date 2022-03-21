
CC = gcc
LIBS = -pthread
CFLAGS = -std=gnu99 -g -Wall #-Wextra

INC = -I.



server: server.o filestorage.o idlist.o
	gcc $(LIBS) $(CFLAGS) $(INC) $^ -o $@

client:	dummyclient.o api.o
	gcc $(LIBS) $(CFLAGS) $(INC) $^ -o $@

#filestorage: filestorage.o idlist.o
#	gcc $(LIBS) $(CFLAGS) -o $@ $^
	
filestorage.o: filestorage.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $<

server.o: server.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $<

idlist.o: idlist.c
	gcc $(LIBS) $(CFLAGS) -c $<
	

