CC=      gcc
LIBS=   -pthread
CFLAGS= -std=gnu99 -g -Wall -D_GNU_SOURCE #-Wextra -pedantic
#OPTFLAGS= -O3

INC_SH=  -I$(SHAREDDIR)
INC_S=   -I$(SDIR)
INC_C=   -I$(CDIR)
INC_API= -I$(APIDIR)

SHAREDDIR= ./SRC/SHARED
SHARED_H= $(SHAREDDIR)/utils.h  $(SHAREDDIR)/errcheck.h  $(SHAREDDIR)/comm.h


SDIR=    ./SRC/FSS
SOBJDIR= $(SDIR)/OBJ
SERVER_O= $(SOBJDIR)/server.o  $(SOBJDIR)/filestorage.o  $(SOBJDIR)/idlist.o
SERVER_H= $(SDIR)/filestorage.h   $(SDIR)/idlist.h


CDIR=    ./SRC/CLIENT
COBJDIR= $(CDIR)/OBJ
CLIENT_O=$(COBJDIR)/client.o $(COBJDIR)/optqueue.o
CLIENT_H= $(CDIR)/optqueue.h

APIDIR= ./SRC/API
APIFLAGS= -L$(APIDIR) -lAPI
	
$(SOBJDIR)/%.o:  $(SDIR)/%.c     $(SHARED_H) $(SERVER_H)
	$(CC) $(CFLAGS) $(INC_SH) $(INC_S) $< -c -o $@

$(COBJDIR)/%.o:  $(CDIR)/%.c     $(SHARED_H) $(CLIENT_H)
	$(CC) $(CFLAGS) $(INC_SH) $(INC_C) $(INC_API) $< -c -o $@
	
$(APIDIR)/api.o: $(APIDIR)/api.c $(SHARED_H)
	$(CC) $(CFLAGS) $(INC_SH) $(INC_API) $< -c -o $@

$(APIDIR)/libAPI.a: $(APIDIR)/api.o
	ar -rc $@ $<

BIN= ./BIN

server: $(SERVER_O) 
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@
	mv $@ $(BIN)/$@

client: $(CLIENT_O) $(APIDIR)/libAPI.a
	$(CC) $(CFLAGS) $(LIBS) $(APIFLAGS) $^ -o $@
	mv $@ $(BIN)/$@



.PHONY: clean cleanall test1

clean:
	rm -f $(SOBJDIR)/*.o  $(COBJDIR)/*.o

cleanall:	
	rm -rf $(BIN)/READ
	rm -rf $(BIN)/TRASH
	
test1:
	$(BIN)/server $(TEST)/config1.txt
	@chmod +x $(TEST)/test1.sh
	$(TEST)/test1.sh


