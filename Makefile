CC = g++ -Wall
FLAG = -std=c++11
LIBSRC = srftp.cpp clftp.cpp

all: srftp clftp 

srftp: iosafe.o srftp.o
	$(CC) $(FLAG)  srftp.cpp -lpthread -L./ -o srftp

clftp: 
	$(CC) $(FLAG) clftp.cpp -o clftp


TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS=$(LIBSRC) Makefile README performance.jpg

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

clean:
	rm -rf ./srftp ./clftp ex5.tar


.PHONY: clftp clftp.o srftp srftp.o iosafe.o
