#Version of GCC
First, you need to have a version of GCC installed on your computer. You can check if you have it installed by running the following command in your terminal:
```bash
gcc --version
```
The version of GCC should at least be GCC 8+ to run the code in this repository.

## Compiling the code

Second, there are two ways to compile the code in this repository. The first way is to use the Makefile provided in the repository. You need to check if you have make installed on your computer by running the following command in your terminal:
```bash
make --version
```
If you have make installed, you can compile the code by running the following command in your terminal:
```bash
make run
```

The second way is to compile the code manually. You can compile the code by running the following commands in your terminal:
```bash
g++ -std=c++0x -O2 -DNDEBUG -pthread src/proxy.cpp src/globalVar.cpp -o bin/proxy -lws2_32
g++ src/ui.cpp src/globalVar.cpp -o bin/app.exe -lgdi32 -mwindows
./bin/app.exe
```
