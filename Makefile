CC=g++
CFLAGS=-std=c++0x -O2 -DNDEBUG -pthread
LDFLAGS=-lws2_32

proxy: test.cpp
	$(CC) $(CFLAGS) test.cpp globalVar.cpp -o proxy $(LDFLAGS)

clean:
	rm -f proxy *.o

.PHONY: clean
