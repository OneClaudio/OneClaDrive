
CC = gcc
LIBS = -pthread
CFLAGS = -g -Wall #-Wextra

INC = -I.


filestorage: filestorage.o idlist.o
	gcc $(LIBS) $(CFLAGS) -o $@ $^
	
filestorage.o: filestorage.c
	gcc $(LIBS) $(CFLAGS) $(INC) -c $^

idlist.o: idlist.c
	gcc $(LIBS) $(CFLAGS) -c $^
	

