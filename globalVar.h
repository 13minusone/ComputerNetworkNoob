#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#ifdef _WIN32
#include <winsock2.h>  
#include <windows.h>
#endif

#include <vector>
#include <string>

// Constants
#define MAX_BLACKLIST_ITEMS 100
#define MAX_ITEM_LENGTH 256


// Global variables
extern std::vector<std::string> blacklist;

// Function declarations
void clearFile(std::string fileName);
bool saveBlacklistToFile(const std::string& filename = "blacklist.txt");
bool loadBlacklistFromFile(const std::string& filename = "blacklist.txt");

#endif 