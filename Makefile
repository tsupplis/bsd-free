# free - BSD Memory Display Utility
# 
# SPDX-License-Identifier: BSD-2-Clause

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
