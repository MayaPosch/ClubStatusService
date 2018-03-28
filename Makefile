LDFLAGS := $(LDFLAGS) -lmosquittopp -lmosquitto -lPocoNet -lPocoNetSSL -lPocoUtil -lPocoData -lPocoDataSQLite -lPocoFoundation -lwiringPi
CFLAGS := $(CFLAGS) -g3 -std=c++11 -lpthread

TARGET = clubstatus
SOURCES := $(wildcard *.cpp)

CC = g++

all: 
	$(CC) -o $(TARGET) $(SOURCES) $(CFLAGS) $(LDFLAGS)

clean : 
	-rm -f *.o clubstatus

.PHONY: all clean
