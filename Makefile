# free - Cross-Platform Memory Display Utility
# 
# SPDX-License-Identifier: BSD-2-Clause

CC=cc
CFLAGS=-Wall -O2 -Wextra
TARGET=free

all: $(TARGET)

# Build with platform-specific flags
# On SunOS/Illumos: make LDFLAGS=-lkstat
$(TARGET): free.c
	$(CC) $(CFLAGS) -o $(TARGET) free.c $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
