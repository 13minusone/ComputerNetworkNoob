#include <winsock2.h>
#include <windows.h>
#include "globalVar.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <process.h>
#include <thread>

#define START_BUTTON 1
#define STOP_BUTTON 2
#define ADD_BLACKLIST_BUTTON 3

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hOutputBox, hStartButton, hStopButton, hBlacklistBox, hAddBlacklistButton, hStatusBox, hBlacklistLabel, hOutputLabel;
HANDLE hWritePipe = NULL;
HANDLE hReadPipe = NULL;
PROCESS_INFORMATION pi = {};
bool proxyRunning = false;

bool autoScroll = true;
WNDPROC oldOutputBoxProc;
std::string outputBuffer;

// Replace OutputBoxProc function with:

LRESULT CALLBACK OutputBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_VSCROLL:
        case WM_MOUSEWHEEL: {
            SCROLLINFO si = {sizeof(SCROLLINFO)};
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            
            // Calculate if we're near bottom (within 20 pixels)
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
            "proxy.exe", NULL, NULL, NULL, TRUE,
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

void AddBlacklist() {
    char buffer[4096];
    GetWindowText(hBlacklistBox, buffer, sizeof(buffer));
    std::stringstream ss(buffer);
    std::string item;
    blacklist.clear();

    while (std::getline(ss, item)) {
        if (!item.empty()) {
            // Remove carriage return if present
            if (!item.empty() && item.back() == '\r') {
                item.pop_back();
            }
            blacklist.push_back(item);
        }
    }
    
    saveBlacklistToFile("blacklist.txt");
    UpdateStatus("Blacklist updated and saved.");
    
    if (proxyRunning) {
        StopProxy();
        StartProxy();
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HBRUSH hBrush = CreateSolidBrush(RGB(240, 248, 255)); // Light blue background
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);

            CreateWindow(
                "STATIC", "Output:",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 100, 20,
                hwnd, NULL, NULL, NULL
            );

           hOutputBox = CreateWindowEx(
    WS_EX_CLIENTEDGE,
    "EDIT", "",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | 
    ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL,
    10, 30, 600, 200,
    hwnd, NULL, NULL, NULL
);
oldOutputBoxProc = (WNDPROC)SetWindowLongPtr(hOutputBox, GWLP_WNDPROC, (LONG_PTR)OutputBoxProc);  
            CreateWindow(
                "STATIC", "Blacklist:",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 240, 100, 20,
                hwnd, NULL, NULL, NULL
            );

            hBlacklistBox = CreateWindow(
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                10, 260, 600, 150,
                hwnd, NULL, NULL, NULL
            );

            hStartButton = CreateWindow(
                "BUTTON", "Start",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, 430, 150, 40,
                hwnd, (HMENU)START_BUTTON, NULL, NULL
            );

            hStopButton = CreateWindow(
                "BUTTON", "Stop",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_DISABLED,
                170, 430, 150, 40,
                hwnd, (HMENU)STOP_BUTTON, NULL, NULL
            );

            hAddBlacklistButton = CreateWindow(
                "BUTTON", "Update Blacklist",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                330, 430, 280, 40,
                hwnd, (HMENU)ADD_BLACKLIST_BUTTON, NULL, NULL
            );

            hStatusBox = CreateWindow(
                "STATIC", "Ready",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 490, 600, 20,
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

        case WM_DESTROY:
          if (proxyRunning) {
        StopProxy();
    }
    
    // Save blacklist before exit
    saveBlacklistToFile("blacklist.txt");
    
    PostQuitMessage(0);
    return 0;

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
        "Proxy Control",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 540,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    loadBlacklistFromFile();
        MSG msg = {};


    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
