CFLAGS=-fpic -shared -pedantic -Wall -ggdb
TARGET=libirc.so
PREFIX=/usr/local

all: libirc.so

$(TARGET): libirc.o
	ld -shared -o $(TARGET) libirc.o

clean:
	rm -f $(TARGET)
	rm *.o

install: all
	cp $(TARGET) $(PREFIX)/lib/$(TARGET)
	cp irc.h $(PREFIX)/include/irc.h
