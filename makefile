# Too many C & H files, time to organize them into folders...

# Compile flags and directory names
# that contain source and headers
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g
SRC_DIR = source
INC_DIR = headers

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=%.o)
EXEC = assembler

all: $(EXEC)
# Get those O files out of here!
	@rm -f $(OBJECTS)

$(EXEC): $(OBJECTS)
	@echo "Linking $(EXEC)..."
	@$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(OBJECTS)

%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	@rm -f $(EXEC) $(OBJECTS)
	@echo "Cleaned up!"