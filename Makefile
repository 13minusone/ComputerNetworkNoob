CC=g++
CFLAGS=-O2 -DNDEBUG
LDFLAGS=-lws2_32

proxy: test.cpp
	$(CC) $(CFLAGS) test.cpp -o proxy $(LDFLAGS)

clean:
	rm -f proxy *.o

.PHONY: clean
