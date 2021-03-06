cc = gcc
src = src/*.c
out = i2a

cflags = -std=gnu11 -pedantic -Wall -O2 -mtune=native -pipe
cflags += `pkg-config --cflags MagickWand`

ldflags = `pkg-config --libs MagickWand`
ldflags += -laa

dst=/

all: $(src)
	$(cc) $(cflags) $(ldflags) $(src) -o $(out)
install:
	install -m 755 $(out) $(dst)/usr/bin/
uninstall:
	rm -f $(dst)/usr/bin/$(out)
clean:
	rm -f $(out)
