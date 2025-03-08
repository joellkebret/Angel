# Name: Joell Kebret
# Student ID: 1275379

# Compiler and Flags
CC      = gcc
CFLAGS  = -Wall -std=c11 -g -fPIC
INC     = include/
SRC     = src/
BIN     = bin/
 
# Ensure bin directory exists before compiling
$(BIN):
	mkdir -p $(BIN)

# 1) 'make parser' - Build the shared library libvcparser.so in bin/
parser: $(BIN)libvcparser.so

# Build the shared library from object files
$(BIN)libvcparser.so: $(BIN)LinkedListAPI.o $(BIN)VCParser.o $(BIN)VCHelpers.o | $(BIN)
	$(CC) -shared -o $(BIN)libvcparser.so $(BIN)LinkedListAPI.o $(BIN)VCParser.o $(BIN)VCHelpers.o

# 2) Object file rules - Compile .c files into bin/*.o 
$(BIN)LinkedListAPI.o: $(SRC)LinkedListAPI.c $(INC)LinkedListAPI.h | $(BIN)
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)LinkedListAPI.c -o $(BIN)LinkedListAPI.o

$(BIN)VCParser.o: $(SRC)VCParser.c $(INC)VCParser.h $(INC)LinkedListAPI.h | $(BIN)
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCParser.c -o $(BIN)VCParser.o

$(BIN)VCHelpers.o: $(SRC)VCHelpers.c $(INC)VCParser.h | $(BIN)
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCHelpers.c -o $(BIN)VCHelpers.o

# 3) 'make clean' - Remove all .o and .so files from bin/
clean:
	rm -rf $(BIN)/*.o $(BIN)/*.so

