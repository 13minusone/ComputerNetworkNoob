#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#ifdef _WIN32
#include <winsock2.h>  // Must be first
#include <windows.h>
#endif

#include <vector>
#include <string>

// Constants
#define SHARED_MEM_NAME "ProxyBlacklistSharedMem"  // Changed from L"..." to "..."
#define MAX_BLACKLIST_ITEMS 100
#define MAX_ITEM_LENGTH 256

// Shared memory structure
struct SharedData {
    int count;
    char items[MAX_BLACKLIST_ITEMS][MAX_ITEM_LENGTH];
};
struct RequestProxy {
    char hostname[256];
    char port[6];
    char method[8];
    char client_ip[16];
    char response[65536];
    char request[65536];
};

// Global variables
extern std::vector<std::string> blacklist;
extern HANDLE hMapFile;
extern SharedData* pShared;

// Function declarations
void cleanupSharedMemory();
void initializeSharedMemory();
void cleanupSharedMemory();
void updateSharedMemory();
void readFromSharedMemory();
bool saveBlacklistToFile(const std::string& filename = "blacklist.txt");
bool loadBlacklistFromFile(const std::string& filename = "blacklist.txt");


#endif // GLOBAL_VAR_H