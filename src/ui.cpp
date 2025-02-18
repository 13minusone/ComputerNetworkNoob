#include <winsock2.h>
#include <windows.h>
#include "../include/globalVar.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <process.h>
#include <thread>
#include <algorithm>
#include <fstream>
#define START_BUTTON 1
#define STOP_BUTTON 2
#define ADD_BLACKLIST_BUTTON 3
#define HOST_RUNNING_TIMER 2
#define HOST_RUNNING_INTERVAL 1000 

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hOutputBox, hStartButton, hStopButton, hBlacklistBox, hAddBlacklistButton, hStatusBox, hBlacklistLabel, hOutputLabel;
HWND hUserGuide, hBlacklistStatus, hHostRunningBox, hHostRunningLabel;

bool hostRunningAutoScroll = true;
bool autoScroll = true;
bool proxyRunning = false;

WNDPROC oldOutputBoxProc;
WNDPROC oldHostRunningBoxProc;

HANDLE hWritePipe = NULL;
HANDLE hReadPipe = NULL;

PROCESS_INFORMATION pi = {};
std::string outputBuffer, hostRunningBuffer;

LRESULT CALLBACK HostRunningBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_VSCROLL:
        case WM_MOUSEWHEEL: {
            SCROLLINFO si = {sizeof(SCROLLINFO)};
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            hostRunningAutoScroll = (si.nPos + (int)si.nPage >= si.nMax - 20);
            break;
        }
    }
    return CallWindowProc(oldHostRunningBoxProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK OutputBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_VSCROLL:
        case WM_MOUSEWHEEL: {
            SCROLLINFO si = {sizeof(SCROLLINFO)};
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            
            autoScroll = (si.nPos + (int)si.nPage >= si.nMax - 20);
            break;
        }
    }
    return CallWindowProc(oldOutputBoxProc, hwnd, msg, wParam, lParam);
}


void UpdateStatus(const std::string& message) {
    SetWindowText(hStatusBox, message.c_str());
}

DWORD WINAPI ReadPipeThread(LPVOID param) {
    HANDLE hReadPipe = (HANDLE)param;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        outputBuffer += buffer;
        
        SendMessage(hOutputBox, WM_SETREDRAW, FALSE, 0);
        SetWindowText(hOutputBox, outputBuffer.c_str());
        
        if (autoScroll) {
            int lastLine = SendMessage(hOutputBox, EM_GETLINECOUNT, 0, 0) - 1;
            SendMessage(hOutputBox, EM_LINESCROLL, 0, lastLine);
        }
        
        SendMessage(hOutputBox, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hOutputBox, NULL, TRUE);
        UpdateWindow(hOutputBox);
    }
    return 0;
}


void StartProxy() {
    if (proxyRunning) return;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        UpdateStatus("Failed to create pipe");
        return;
    }

    STARTUPINFO si = { sizeof(si) };
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (CreateProcess(
            "bin\\proxy.exe", NULL, NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        proxyRunning = true;
        CreateThread(NULL, 0, ReadPipeThread, hReadPipe, 0, NULL);
        UpdateStatus("Proxy started");
        EnableWindow(hStartButton, FALSE);
        EnableWindow(hStopButton, TRUE);
    } else {
        UpdateStatus("Failed to start proxy");
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }
}

void StopProxy() {
    if (proxyRunning) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        proxyRunning = false;
        EnableWindow(hStartButton, TRUE);
        EnableWindow(hStopButton, FALSE);
        UpdateStatus("Proxy stopped");
    }
}

void UpdateBlacklistStatus() {
    std::string status = "Blacklist: " + std::to_string(blacklist.size()) + " domains blocked";
    SetWindowText(hBlacklistStatus, status.c_str());
}

void AddBlacklist() {
    char buffer[4096];
    GetWindowText(hBlacklistBox, buffer, sizeof(buffer));
    std::stringstream ss(buffer);
    std::string item;
    blacklist.clear();

    while (ss >> item) {
        if (!item.empty()) {
            if (item.back() == '\r' && item.back() == '\n') {
                item.pop_back();
            }
        }
        if (!item.empty()) 
                blacklist.push_back(item);

    }
    
    saveBlacklistToFile(folderName + "cache//blacklist.txt");
    UpdateBlacklistStatus();    
    if (proxyRunning) {
        StopProxy();
        StartProxy();
    }
}

void loadHostRunningFromFile() {
    std::ifstream file(folderName + "cache/Host running.txt");
    std::fstream fileCache(folderName + "cache/cache.txt", std::ios::in);

    if (!fileCache.is_open()) {
        return;
    }
    std::string cache_string;
    cache_string.clear();

    if (file.is_open()) {
        hostRunningBuffer.clear();
        std::string line1, line;
        line.clear();
        line1.clear();

         // cahce the host running buffer
        while (std::getline(fileCache, line1)) {
            if (line1.empty()) continue;
                line1 += '\n';
                line += line1;
        }
        fileCache.close();
        std::istringstream iss(line);
        while(iss >>line){
            cache_string += line + "\r\n";
        }
        line.clear();
        line1.clear();
        while (std::getline(file, line1)) {
            if (line1.empty()) continue;
                line1 += '\n';
                line += line1;
        }
        std::ofstream fileCache(folderName + "cache/cache.txt");
        std::istringstream isss(line);
        while(isss >>line){
            if (line.empty()) continue;
            fileCache << line << '\n';
            hostRunningBuffer += line + "\r\n";
        }        
        
        fileCache.close();
        file.close();

        if (IsWindow(hHostRunningBox) && hostRunningBuffer != cache_string) {
            SendMessage(hHostRunningBox, WM_SETREDRAW, FALSE, 0);
            SetWindowText(hHostRunningBox, hostRunningBuffer.c_str());
            if (hostRunningAutoScroll) {
                int lastLine = SendMessage(hHostRunningBox, EM_GETLINECOUNT, 0, 0) - 1;
                SendMessage(hHostRunningBox, EM_LINESCROLL, 0, lastLine);
            }
            SendMessage(hHostRunningBox, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(hHostRunningBox, NULL, TRUE);
        }

    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TIMER:
            if (wParam == HOST_RUNNING_TIMER) {
                loadHostRunningFromFile();
            }
        break;
        case WM_CREATE: {
            HBRUSH hBrush = CreateSolidBrush(RGB(240, 248, 255));
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);

            CreateWindow(
                "STATIC", "Request:",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 100, 100,
                hwnd, NULL, NULL, NULL
            );

           hOutputBox = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | 
                ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL,
                10, 30, 550, 200,
                hwnd, NULL, NULL, NULL
            );

            hHostRunningLabel = CreateWindow(
                "STATIC", "Host Running:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                560, 10, 290, 20,
                hwnd, NULL, NULL, NULL
            );

            hHostRunningBox = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                560, 30, 320, 200,
                hwnd, NULL, NULL, NULL
            );

            oldHostRunningBoxProc = (WNDPROC)SetWindowLongPtr(hHostRunningBox, GWLP_WNDPROC, (LONG_PTR)HostRunningBoxProc);
            oldOutputBoxProc = (WNDPROC)SetWindowLongPtr(hOutputBox, GWLP_WNDPROC, (LONG_PTR)OutputBoxProc);  
            
            CreateWindow(
                "STATIC", "Blacklist:",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 240, 100, 100,
                hwnd, NULL, NULL, NULL
            );

            hBlacklistBox = CreateWindow(
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                10, 260, 870, 200,
                hwnd, NULL, NULL, NULL
            );

            hStartButton = CreateWindow(
                "BUTTON", "Start",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, 480, 150, 40,
                hwnd, (HMENU)START_BUTTON, NULL, NULL
            );

            hStopButton = CreateWindow(
                "BUTTON", "Stop",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_DISABLED,
                170, 480, 150, 40,
                hwnd, (HMENU)STOP_BUTTON, NULL, NULL
            );

            hAddBlacklistButton = CreateWindow(
                "BUTTON", "Update Blacklist",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                330, 480, 280, 40,
                hwnd, (HMENU)ADD_BLACKLIST_BUTTON, NULL, NULL
            );


            hStatusBox = CreateWindow(
                "STATIC", "Ready",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 530, 900, 20,
                hwnd, NULL, NULL, NULL
            );

            hBlacklistStatus = CreateWindow(
                "STATIC", "Blacklist: 0 domains blocked",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 555, 900, 20,
                hwnd, NULL, NULL, NULL
            );
            hUserGuide = CreateWindow(
                "STATIC",
                "!!!You must configure the proxy settings before running this program !!!\r\n"
                "Open Windows Settings --> Network and Internet --> Proxy, and set the IP address to 127.0.0.1 and port to 8080\r\n\n"
                "Proxy System Guide: \r\n"
                "1. Click Start to activate the proxy and view incoming requests in the Request panel.\r\n"
                "2. Add blocked domains to Blacklist.\r\n"
                "3. Use 'Update Blacklist' button to modify blocked websites.\r\n"
                "4. Click Stop to deactivate the proxy when needed.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 590, 900, 150,
                hwnd, NULL, NULL, NULL
            );

            SetWindowText(hStartButton, "\nStart\n\n");
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == START_BUTTON) {
                StartProxy();
            } 
            else if (LOWORD(wParam) == STOP_BUTTON) {
                StopProxy();
            }
            else if (LOWORD(wParam) == ADD_BLACKLIST_BUTTON) {
                AddBlacklist();
            }
            break;
        }

        case WM_DESTROY: {
            if (proxyRunning) {
                StopProxy();
            }
            clearFile(folderName + "cache//Host running.txt");
            clearFile(folderName + "cache//blacklist.txt");
            clearFile(folderName + "cache//cache.txt");
            PostQuitMessage(0);
            break;
        }
    
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ProxyWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

  HWND hwnd = CreateWindow(
    "ProxyWindowClass",
    "Proxy System",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Remove WS_MAXIMIZEBOX and WS_THICKFRAME
    CW_USEDEFAULT, CW_USEDEFAULT, 900, 800,
    NULL, NULL, hInstance, NULL
);

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    SetTimer(hwnd, HOST_RUNNING_TIMER, HOST_RUNNING_INTERVAL, NULL);
    loadBlacklistFromFile("..//ComputerNetWorkNoob//cache//blacklist.txt"); 
        MSG msg = {};


    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
