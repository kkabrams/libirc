LDFLAGS=-lirc -Llibirc
CFLAGS=-fpic -shared -pedantic -Wall
TARGET=libirc.so

all: libirc.c
	$(CC) $(CFLAGS) -o $(TARGET) libirc.c

clean:
	rm -f libirc.so

install: all
	cp $(TARGET) /usr/local/lib/$(TARGET)
	cp irc.h /usr/local/include/irc.h
