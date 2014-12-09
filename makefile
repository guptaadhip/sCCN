#! /bin/bash
# project name (generate executable with this name)
TARGET   = sccn


CC       = g++
# compiling flags here
CFLAGS   = -std=c++11 -Wall -I. -pthread -ggdb

LINKER   = g++ -O2 -Wl,--no-as-needed -o
# linking flags here
LFLAGS   = -Wall -I.

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
	$(LINKER) $@ $(LFLAGS) $(OBJECTS)  -lm -ggdb -pthread

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(rm) $(OBJECTS)
	rm -rf $(OBJDIR)

remove: clean
	$(rm) $(BINDIR)/$(TARGET)
	rm -rf $(BINDIR)
