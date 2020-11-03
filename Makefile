# the compiler: gcc for C program
CC		= gcc
RM		= /bin/rm

# compiler flags:
CFLAGS	= -o

SERVER	= server
CLIENT	= client

SRC1	= $(SERVER).c
SRC2	= $(CLIENT).c

all		: $(SRC1) $(SRC2)
	$(CC) $(CFLAGS) $(SERVER) $(SRC1)
	$(CC) $(CFLAGS) $(CLIENT) $(SRC2) -lpthread

# Clean up
clean:
	$(RM) -f $(SERVER) $(CLIENT)
