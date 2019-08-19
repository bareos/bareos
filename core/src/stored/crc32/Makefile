# simple Makefile
CPP       = g++

# files
PROGRAM   = Crc32Test
LIBS      = -lrt
HEADERS   = Crc32.h
OBJECTS   = Crc32.o Crc32Test.o

# flags
FLAGS     = -O3 -Wall -pedantic -s
LINKFLAGS = -s

default: $(PROGRAM)
all: default

$(PROGRAM): $(OBJECTS) Makefile
	$(CPP) $(OBJECTS) $(LIBS) -o $(PROGRAM)

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(LINKFLAGS) -c $< -o $@

clean:
	-rm -f $(OBJECTS) $(PROGRAM)

run: $(PROGRAM)
	./$(PROGRAM)
