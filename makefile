# project name (generate executable with this name)
TARGET   = sccn

CC       = g++
# compiling flags here
CFLAGS   = -std=c++11 -Wall -I. -I/usr/local/boost_1_57_0/

LINKER   = g++ -o
# linking flags here
LFLAGS   = -Wall -I. -L/usr/local/lib -lm -lboost_system -lboost_thread -lboost_program_options

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
	echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	echo "Compiled "$<" successfully!"

clean:
	$(rm) $(OBJECTS)
	rmdir $(OBJDIR)
	echo "Cleanup complete!"

remove: clean
	$(rm) $(BINDIR)/$(TARGET)
	rmdir $(BINDIR)
	echo "Executable removed!"
