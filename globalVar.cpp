#include "globalVar.h"
#include <fstream>
#include <iostream>
#include <sstream>
std::vector<std::string> blacklist;

void clearBlacklistFile() {
    std::ofstream file("blacklist.txt", std::ofstream::out | std::ofstream::trunc);
    file.close();
}


bool saveBlacklistToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file) return false;
    
    for (const auto& domain : blacklist) {
        if (!domain.empty())  
            file << domain << '\n';
    }
    return true;
} 

bool loadBlacklistFromFile(const std::string& filename) {
    std::fstream file(filename, std::ios::in);
    if (!file) return false;

    blacklist.clear();
    std::string line1, line;
    
    while (std::getline(file, line1)) {
        line1 += '\n';
        line += line1;
    }
    std::istringstream iss(line);
    while(iss >> line) {
        // remove https:// and http://
        if (line.find("https://") != std::string::npos) {
            line = line.substr(8);
        } else if (line.find("http://") != std::string::npos) {
            line = line.substr(7);
        }
        blacklist.push_back(line);
    }
    return true;
}

