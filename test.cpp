#include <iostream>
#include <string>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#ifdef _WIN32
#ifdef __MINGW32__
#pragma comment(lib, "pthread")
#endif
#endif
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>  // Added for waitpid and WNOHANG
typedef int socket_t;
#define CLOSE_SOCKET close
#endif

#define MAX_BUFFER_SIZE 65536
#define DEFAULT_PROXY_PORT 8080
#define CONNECT_TIMEOUT_SEC 5
#define TRANSFER_TIMEOUT_SEC 30
#define SSL_BUFFER_SIZE 65536

void cleanup_socket(socket_t sock) {
    if (sock >= 0) CLOSE_SOCKET(sock);
}

#ifdef _WIN32
void initialize_winsock() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "Failed to initialize Winsock\n";
        exit(1);
    }
}

void cleanup_winsock() {
    WSACleanup();
}
#else
void initialize_winsock() {}
void cleanup_winsock() {}
#endif

#ifdef _WIN32
void handle_sigchld(int sig) {
    // Windows doesn't need this
}
#else
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
#endif

void handle_client(socket_t client_sock) {
    char buffer[MAX_BUFFER_SIZE];
    std::string request;
    
    // Read the request with timeout check
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        cleanup_socket(client_sock);
        return;
    }
    
    buffer[bytes] = '\0';
    request = buffer;
    
    // Basic validation of request
    if (request.empty() || request.find("\r\n") == std::string::npos) {
        cleanup_socket(client_sock);
        return;
    }

    // Parse request line
    std::istringstream request_stream(request);
    std::string method, url, version;
    request_stream >> method >> url >> version;
    if (method.empty() || url.empty() || version.empty()) {
        cleanup_socket(client_sock);
        return;
    }
    std::cout << "New request: " << method << " " << url << " " << version << std::endl;
    std::cout << "Request: " << request << std::endl;

    // Parse host
    size_t host_pos = request.find("Host: ");
    if (host_pos == std::string::npos) {
        cleanup_socket(client_sock);
        return;
    }

    size_t host_end = request.find("\r\n", host_pos);
    std::string host = request.substr(host_pos + 6, host_end - (host_pos + 6));
    
    // Split host and port
    std::string hostname = host;
    std::string port = "80";
    size_t colon_pos = host.find(":");
    if (colon_pos != std::string::npos) {
        hostname = host.substr(0, colon_pos);
        port = host.substr(colon_pos + 1);
    } else if (method == "CONNECT") {
        port = "443";
    }

    // Connect to remote server
    struct addrinfo hints = {}, *server_info;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), port.c_str(), &hints, &server_info) != 0) {
        cleanup_socket(client_sock);
        return;
    }

    socket_t server_sock = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_sock < 0) {
        freeaddrinfo(server_info);
        cleanup_socket(client_sock);
        return;
    }

    if (connect(server_sock, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        freeaddrinfo(server_info);
        cleanup_socket(server_sock);
        cleanup_socket(client_sock);
        return;
    }

    freeaddrinfo(server_info);

    // Add keep-alive for both sockets
    int keepalive = 1;
    int keepcnt = 3;
    int keepidle = 30;
    int keepintvl = 5;

    setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive));
    setsockopt(server_sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive));
    
    #ifdef _WIN32
    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&keepalive, sizeof(keepalive));
    setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&keepalive, sizeof(keepalive));
    #else
    setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
    setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPALIVE, &keepidle, sizeof(int));
    setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
    #endif

    if (method == "CONNECT") {
        // Set TCP_NODELAY for both sockets
        int flag = 1;
        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
        setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        // Handle HTTPS without logging
        send(client_sock, "HTTP/1.1 200 Connection Established\r\n\r\n", 39, 0);

        char ssl_buffer[SSL_BUFFER_SIZE];
        fd_set read_fds;
        struct timeval tv;
        int max_fd = (client_sock > server_sock ? client_sock : server_sock) + 1;
        
        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_sock, &read_fds);
            FD_SET(server_sock, &read_fds);

            tv.tv_sec = TRANSFER_TIMEOUT_SEC;
            tv.tv_usec = 0;

            int select_result = select(max_fd, &read_fds, NULL, NULL, &tv);
            if (select_result < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (select_result == 0) continue; // timeout, try again

            if (FD_ISSET(client_sock, &read_fds)) {
                int bytes = recv(client_sock, ssl_buffer, SSL_BUFFER_SIZE, 0);
                if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                if (bytes <= 0) break;
                
                int sent = 0;
                while (sent < bytes) {
                    int n = send(server_sock, ssl_buffer + sent, bytes - sent, 0);
                    if (n < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;
                        goto cleanup;
                    }
                    sent += n;
                }
            }

            if (FD_ISSET(server_sock, &read_fds)) {
                int bytes = recv(server_sock, ssl_buffer, SSL_BUFFER_SIZE, 0);
                if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                if (bytes <= 0) break;
                
                int sent = 0;
                while (sent < bytes) {
                    int n = send(client_sock, ssl_buffer + sent, bytes - sent, 0);
                    if (n < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;
                        goto cleanup;
                    }
                    sent += n;
                }
            }
        }
    } else {
        // Handle HTTP without logging for better performance
        int total_sent = 0;
        int len = request.length();
        while (total_sent < len) {
            int n = send(server_sock, request.c_str() + total_sent, len - total_sent, 0);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                goto cleanup;
            }
            total_sent += n;
        }
        
        char buffer[MAX_BUFFER_SIZE];
        while (true) {
            int bytes = recv(server_sock, buffer, sizeof(buffer), 0);
            if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            if (bytes <= 0) break;
            
            int sent = 0;
            while (sent < bytes) {
                int n = send(client_sock, buffer + sent, bytes - sent, 0);
                if (n < 0) {
                    if (errno == EINTR || errno == EAGAIN) continue;
                    goto cleanup;
                }
                sent += n;
            }
        }
    }

cleanup:
    cleanup_socket(server_sock);
    cleanup_socket(client_sock);
}

#ifdef _WIN32
// Thread function for Windows
unsigned __stdcall client_thread(void* arg) {
    socket_t client_sock = (socket_t)arg;
    handle_client(client_sock);
    return 0;
}
#endif

int main() {
    initialize_winsock();

    #ifndef _WIN32
    // Set up signal handler for child processes
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        std::cerr << "Failed to set up signal handler\n";
        return 1;
    }
    #endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::cerr << "Failed to create server socket\n";
        cleanup_winsock();
        return 1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DEFAULT_PROXY_PORT);
    // Add before bind()
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Increase buffer sizes
    int rcvbuf = 65536;
    int sndbuf = 65536;
    setsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));
    setsockopt(server_sock, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf, sizeof(sndbuf));

    // Remove the non-blocking mode setting
    // u_long mode = 1;
    // ioctlsocket(server_sock, FIONBIO, &mode);
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind server socket\n";
        cleanup_socket(server_sock);
        cleanup_winsock();
        return 1;
    }

    if (listen(server_sock, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on server socket\n";
        cleanup_socket(server_sock);
        cleanup_winsock();
        return 1;
    }

    std::cout << "Proxy server running on port " << DEFAULT_PROXY_PORT << std::endl;
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Add timeout for accept
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        if (select(server_sock + 1, &read_fds, NULL, NULL, &timeout) > 0) {
            socket_t client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_sock < 0) {
                std::cerr << "Failed to accept client connection\n";
                continue;
            }

            #ifdef _WIN32
            // Use Windows threads
            HANDLE thread_handle = (HANDLE)_beginthreadex(
                NULL,
                0,
                client_thread,
                (void*)client_sock,
                0,
                NULL
            );
            if (thread_handle == NULL) {
                std::cerr << "Failed to create thread\n";
                cleanup_socket(client_sock);
            } else {
                CloseHandle(thread_handle);  // Detach the thread
            }
            #else
            // Fork process to handle client
            pid_t pid = fork();
            if (pid < 0) {
                std::cerr << "Failed to fork process\n";
                cleanup_socket(client_sock);
                continue;
            }
            
            if (pid == 0) {  // Child process
                cleanup_socket(server_sock);  // Close server socket in child
                handle_client(client_sock);
                exit(0);
            } else {  // Parent process
                cleanup_socket(client_sock);  // Close client socket in parent
            }
            #endif
        }
    }
    cleanup_socket(server_sock);
    cleanup_winsock();
    return 0;
}
