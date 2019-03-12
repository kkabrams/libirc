CFLAGS=-fpic -shared -pedantic -Wall -g3
TARGET=libirc.so
PREFIX:=/usr/local

all: libirc.so

$(TARGET): libirc.o
	ld -shared -o $(TARGET) libirc.o

clean:
	rm -f $(TARGET)
	rm *.o

install: all
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	install $(TARGET) $(PREFIX)/lib/$(TARGET)
	install irc.h $(PREFIX)/include/irc.h
