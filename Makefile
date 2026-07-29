CC       := gcc
VERSION  := $(shell date +%Y/%m)
CFLAGS   := -O3 -march=native -flto -pipe -Wall -Wextra -DVERSION=\"$(VERSION)\"
LDLIBS   := -lX11 -lXrandr -lncurses
TARGET   := x11bbradj
SRC      := main.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)
	strip --strip-all $(TARGET)

clean:
	rm -f $(TARGET)