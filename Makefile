# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -D_POSIX_C_SOURCE=200809L
LDFLAGS = 

# Source files (update these with your actual filenames)
SERVER_SRC = src/server.c
SERVER_HDR = src/server.h
SERVER_BIN = bin/server

CLIENT_SRC = src/client.c
CLIENT_HDR = src/client.h
CLIENT_BIN = bin/client

# Default target - builds both programs
all: $(SERVER_BIN) $(CLIENT_BIN)

# Build first program
$(SERVER_BIN): $(SERVER_SRC) $(SERVER_HDR)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(LDFLAGS)

# Build second program
$(CLIENT_BIN): $(CLIENT_SRC) $(CLIENT_HDR)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC) $(LDFLAGS)

# Clean up generated files
clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

# Force rebuild
rebuild: clean all

# Individual targets
server: $(SERVER_BIN)
client: $(CLIENT_BIN)

# Phony targets
.PHONY: all clean rebuild server client