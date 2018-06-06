#

TARGET = clubstatus
SOURCES := $(wildcard *.cpp)

ifdef TEST
LDFLAGS := $(LDFLAGS) -lmosquittopp -lmosquitto -lPocoNet -lPocoNetSSL -lPocoUtil -lPocoData -lPocoDataSQLite -lPocoFoundation
INCLUDES := -Iwiring
SOURCES += $(wildcard wiring/*.cpp)
else
LDFLAGS := $(LDFLAGS) -lmosquittopp -lmosquitto -lPocoNet -lPocoNetSSL -lPocoUtil -lPocoData -lPocoDataSQLite -lPocoFoundation -lwiringPi
endif

CFLAGS := $(CFLAGS) -g3 -std=c++14 -lpthread $(INCLUDES)

CC = g++

all: 
	$(CC) -o $(TARGET) $(SOURCES) $(CFLAGS) $(LDFLAGS)

clean : 
	-rm -f *.o clubstatus

.PHONY: all clean
