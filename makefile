#! /bin/bash
# project name (generate executable with this name)
TARGET   = sccn

CC       = g++
# compiling flags here
CFLAGS   = -std=c++11 -Wall -I. -pthread -ggdb

LINKER   = g++ -o
# linking flags here
LFLAGS   = -Wall -I. -lm -ggdb -pthread

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
	$(LINKER) $@ $(LFLAGS) $(OBJECTS)

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(rm) $(OBJECTS)
	rmdir $(OBJDIR)

remove: clean
	$(rm) $(BINDIR)/$(TARGET)
	rmdir $(BINDIR)
