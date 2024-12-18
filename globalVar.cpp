#include "globalVar.h"
#include <fstream>
#include <iostream>

std::vector<std::string> blacklist;
HANDLE hMapFile = NULL;
SharedData* pShared = NULL;

void cleanupSharedMemory() {
    if (pShared) {
        UnmapViewOfFile(pShared);
        pShared = NULL;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
}

void updateSharedMemory() {
    if (!hMapFile) {
        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(SharedData),
            SHARED_MEM_NAME);
    }

    if (hMapFile && !pShared) {
        pShared = (SharedData*)MapViewOfFile(hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedData));
    }

    if (pShared) {
        pShared->count = std::min((int)blacklist.size(), MAX_BLACKLIST_ITEMS);
        for (int i = 0; i < pShared->count; i++) {
            strncpy_s(pShared->items[i], blacklist[i].c_str(), MAX_ITEM_LENGTH - 1);
        }
    }
}

void readFromSharedMemory() {
    if (!hMapFile) {
        hMapFile = OpenFileMapping(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            SHARED_MEM_NAME);
    }

    if (hMapFile && !pShared) {
        pShared = (SharedData*)MapViewOfFile(hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedData));
    }

    if (pShared) {
        blacklist.clear();
        for (int i = 0; i < pShared->count; i++) {
            blacklist.push_back(pShared->items[i]);
        }
    }
}

bool saveBlacklistToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file) return false;
    
    for (const auto& domain : blacklist) {
        file << domain << std::endl;
    }
    return true;
}

bool loadBlacklistFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return false;

    blacklist.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            blacklist.push_back(line);
        }
    }
    updateSharedMemory();
    return true;
}