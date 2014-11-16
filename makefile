#! /bin/bash
# project name (generate executable with this name)
TARGET   = sccn

# PF_RING Library
CPFRING = -I/usr/local/PF_RING/userland/lib -I/usr/local/PF_RING/kernel

# PF_RING libraries needed for linking
LPFRING = /usr/local/PF_RING/userland/lib/libpfring.a   
LPCAP = /usr/local/PF_RING/userland/libpcap-1.1.1-ring/libpcap.a

CC       = g++
# compiling flags here
CFLAGS   = -std=c++11 -Wall -I. $(CPFRING) -pthread -ggdb

LINKER   = g++ -O2 -Wl,--no-as-needed -o
# linking flags here
LFLAGS   = -Wall -I. $(CPFRING)

SRCDIR   = src
INCLUDEDIR   = include
OBJDIR   = obj
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
INCLUDES := $(wildcard $(INCLUDEDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
rm       = rm -f


$(BINDIR)/$(TARGET): $(OBJECTS)
	mkdir -p $(BINDIR)
	$(LINKER) $@ $(LFLAGS) $(OBJECTS) $(LPFRING) $(LPCAP) -lm -ggdb -lnuma -pthread -lrt

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(rm) $(OBJECTS)
	rmdir $(OBJDIR)

remove: clean
	$(rm) $(BINDIR)/$(TARGET)
	rmdir $(BINDIR)
