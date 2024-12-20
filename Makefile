CC=g++
CFLAGS=-std=c++0x -O2 -DNDEBUG -pthread
LDFLAGS=-lws2_32
BIN_DIR=bin
SRC_DIR=src

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

proxy: $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/proxy.cpp $(SRC_DIR)/globalVar.cpp -o $(BIN_DIR)/proxy $(LDFLAGS)

app: $(BIN_DIR)
	$(CC) $(SRC_DIR)/ui.cpp $(SRC_DIR)/globalVar.cpp -o $(BIN_DIR)/app.exe -lgdi32 -mwindows

run: proxy app
	./$(BIN_DIR)/app.exe

clean:
	rm -rf $(BIN_DIR)

.PHONY: clean run