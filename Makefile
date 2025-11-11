# free - BSD Memory Display Utility
# 
# This is free and unencumbered software released into the public domain.
# For more information, please refer to <http://unlicense.org/>

CC=cc
CFLAGS=-Wall -O2
TARGET=free

all: $(TARGET)

$(TARGET): free.c
	$(CC) $(CFLAGS) -o $(TARGET) free.c

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
