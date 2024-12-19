#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include "globalVar.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#endif

#define MAX_BUFFER_SIZE 65536
#define TRANSFER_TIMEOUT_SEC 5


bool check_blacklist(const std::string& url) {
    loadBlacklistFromFile("blacklist.txt");

    for (int i = 0; i < blacklist.size(); i++) {
        std::string domain = blacklist[i];
        for (int i = 0; i < domain.size(); i++) {
            if (domain[i] == ' ' || domain[i] == '\r' || domain[i] == '\n') {
                domain[i] = '\0';
                break;
            }
        }
        if (domain.empty()) continue;
        if (url.find(domain) != std::string::npos) {
            std::cout << "Blocked domain found: " << domain << "\n";
            return true;
        }
    }
    return false;
}

void handle_client(socket_t client_sock) {
    char buffer[MAX_BUFFER_SIZE];
    size_t total_received = 0;
    int bytes;
    std::string client_ip;
    
    // Get client IP
    sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    getpeername(client_sock, (struct sockaddr*)&addr, &addr_size);
    client_ip = inet_ntoa(addr.sin_addr);
    
    // Safely receive initial request
    while (total_received < MAX_BUFFER_SIZE - 1) {
        bytes = recv(client_sock, buffer + total_received, MAX_BUFFER_SIZE - 1 - total_received, 0);
        if (bytes <= 0) {
            CLOSE_SOCKET(client_sock);
            return;
        }
        total_received += bytes;
        
        // Check if we've received the end of the HTTP headers
        if (strstr(buffer, "\r\n\r\n")) break;
    }
    
    if (total_received >= MAX_BUFFER_SIZE - 1) {
        std::cerr << "Request too large\n";
        CLOSE_SOCKET(client_sock);
        return;
    }
    
    buffer[total_received] = '\0';
    std::string request = buffer;
    
    // Parse request
    std::istringstream request_stream(request);
    std::string method, url, version;
    request_stream >> method >> url >> version;
    
    size_t host_pos = request.find("Host: ");
    if (host_pos == std::string::npos) {
        CLOSE_SOCKET(client_sock);
        return;
    }

    size_t host_end = request.find("\r\n", host_pos);
    std::string host = request.substr(host_pos + 6, host_end - (host_pos + 6));
    
    // Split host and port
    std::string hostname = host;
    std::string port = (method == "CONNECT") ? "443" : "80";
    size_t colon_pos = host.find(":");
    if (colon_pos != std::string::npos) {
        hostname = host.substr(0, colon_pos);
        port = host.substr(colon_pos + 1);
    }

    if (check_blacklist(hostname)) {
        std::cout << "Blocked: " << hostname << std::endl;
        CLOSE_SOCKET(client_sock);
        return;
    }

    // Add connection request logging
    std::cout << "New " << (method == "CONNECT" ? "HTTPS" : "HTTP") 
              << " request to host: " << hostname 
              << ":" << port << " from " << client_ip << std::endl;
    // Connect to server
    // create a struct include hostname, port, method, client_ip and write it to request.bin
    
    struct addrinfo hints = {}, *server_info;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), port.c_str(), &hints, &server_info) != 0) {
        CLOSE_SOCKET(client_sock);
        return;
    }

    socket_t server_sock = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_sock < 0 || connect(server_sock, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        CLOSE_SOCKET(client_sock);
        freeaddrinfo(server_info);
        return;
    }

    freeaddrinfo(server_info);

    if (method == "CONNECT") {
        send(client_sock, "HTTP/1.1 200 Connection Established\r\n\r\n", 39, 0);
        std::cout << "Established HTTPS tunnel to: " << hostname << ":" << port << std::endl;
        
        fd_set read_fds;
        struct timeval timeout;
        
        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_sock, &read_fds);
            FD_SET(server_sock, &read_fds);

            timeout.tv_sec = 1;  // 1 second timeout
            timeout.tv_usec = 0;

            int activity = select(std::max(client_sock, server_sock) + 1, &read_fds, NULL, NULL, &timeout);
            
            if (activity < 0) {
                std::cout << "Select error for: " << hostname << std::endl;
                break;
            }
            
            // Check for disconnection during timeout
            if (activity == 0) {
                char test;
                if (recv(client_sock, &test, 1, MSG_PEEK) == 0) {
                    break;
                }
                continue;
            }

            if (FD_ISSET(client_sock, &read_fds)) {
                bytes = recv(client_sock, buffer, MAX_BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    break;
                }
                
                size_t sent = 0;
                while (sent < bytes) {
                    int n = send(server_sock, buffer + sent, bytes - sent, 0);
                    if (n <= 0) goto cleanup;
                    sent += n;
                }
            }

            if (FD_ISSET(server_sock, &read_fds)) {
                bytes = recv(server_sock, buffer, MAX_BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    break;
                }
                
                size_t sent = 0;
                while (sent < bytes) {
                    int n = send(client_sock, buffer + sent, bytes - sent, 0);
                    if (n <= 0) goto cleanup;
                    sent += n;
                }
            }
        }
    } else {
        std::cout << "Forwarding HTTP request to: " << hostname << ":" << port << std::endl;
        
        if (send(server_sock, request.c_str(), request.length(), 0) <= 0) {
            CLOSE_SOCKET(client_sock);
            CLOSE_SOCKET(server_sock);
            return;
        }
        bytes = recv(server_sock, buffer, MAX_BUFFER_SIZE - 1, 0);
        while ((bytes) > 0) {
            buffer[bytes] = '\0';  // Ensure null termination
            
            size_t sent = 0;
            while (sent < bytes) {
                int n = send(client_sock, buffer + sent, bytes - sent, 0);
                if (n <= 0) goto cleanup;
                sent += n;
            }
        }
    }

cleanup:
    CLOSE_SOCKET(server_sock);
    CLOSE_SOCKET(client_sock);
}

int main() {

    #ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "Failed to initialize Winsock\n";
        return 1;
    }
    #endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 ||
        listen(server_sock, SOMAXCONN) < 0) {
        std::cerr << "Failed to start server\n";
        CLOSE_SOCKET(server_sock);
        return 1;
    }
    std::cout << "Proxy server started on port 8080\n";
    

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        socket_t client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock >= 0) {
            std::thread(handle_client, client_sock).detach();

        }
    }

    CLOSE_SOCKET(server_sock);
    #ifdef _WIN32
    WSACleanup();
    #endif
    return 0;
}