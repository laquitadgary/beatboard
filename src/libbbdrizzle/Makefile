.SUFFIXES: .cpp .o .h

CC = g++

COMMON_CFLAGS = -Wall -Werror
OS	= ${shell uname}
ifeq (${OS}, Linux)
CFLAGS = $(COMMON_CFLAGS) -fpic
CLIBFLAGS = -shared
endif
ifeq (${OS}, Darwin)
CFLAGS = $(COMMON_CFLAGS) -Kpic
CLIBFLAGS = -G -dynamiclib
endif

ifeq ($(BB), 1)
BBHOME = /usr/bb
BBINCLUDES = -I$(BBHOME)/local/include
BBLIBS = -L$(BBHOME)/local/lib
else
BBINCLUDES=
BBLIBS=
endif

LDFLAGS = -ldrizzle
LIBS = -L/usr/local/lib -L. $(BBLIBS)
INCLUDES = -I. $(BBINCLUDES)
TARGET = libbbdrizzle.so
OBJS = drizzle_client.o
MAINTARGET = drizzle_client_main
MAINOBJS = drizzle_client_main.o
MAINLDFLAGS = $(LDFLAGS) -lbbdrizzle

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(OBJS) $(INCLUDES) $(LIBS) $(LDFLAGS) -o $(TARGET)

.cpp.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $(LIBS) $<

clean:
	rm -f $(TARGET) $(OBJS) $(MAINTARGET) $(MAINOBJS)

main: $(MAINTARGET)

$(MAINTARGET): $(MAINOBJS) $(TARGET)
	$(CC) $(CFLAGS) $(LIBS) $(MAINLDFLAGS) $(MAINOBJS) -o $(MAINTARGET)
