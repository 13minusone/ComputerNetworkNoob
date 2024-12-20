CC=g++
CFLAGS=-std=c++0x -O2 -DNDEBUG -pthread
LDFLAGS=-lws2_32

proxy: proxy.cpp
	$(CC) $(CFLAGS) proxy.cpp globalVar.cpp -o proxy $(LDFLAGS)

ui: ui.cpp
	$(CC) ui.cpp globalVar.cpp -o ui -lgdi32 -mwindows

run: proxy ui
	./ui

clean:
	rm -f proxy ui *.o

.PHONY: clean