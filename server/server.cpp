#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <iostream>
#include <ws2tcpip.h>
#include <vector>
#include <windows.h>
#include <tchar.h>
#include <thread>
#include <queue>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "opencv_world4100.lib")
#pragma comment(lib, "opencv_world4100d.lib")

#define PORT 12345
SOCKET listenSocket = INVALID_SOCKET;
SOCKET clientSocket = INVALID_SOCKET;

HDC hdcMem;
BITMAPINFO bmi;
HWND hStatic;
HINSTANCE hInst;
std::queue<cv::Mat> de;
HWND hMainWindow;

int acceptConnections() {
    WSADATA wsaData;
    struct sockaddr_in serverAddr;
    int addrLen = sizeof(serverAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // Create a socket
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the sockaddr_in structure
    serverAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    serverAddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(listenSocket, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for incoming connections..." << std::endl;

    // Accept a client socket
    clientSocket = accept(listenSocket, (struct sockaddr*)&serverAddr, &addrLen);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected." << std::endl;

    return 0;
}

HBITMAP MatToHBITMAP(const cv::Mat& mat) {
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = mat.cols;
    bmi.bmiHeader.biHeight = -mat.rows; // Negative to ensure top-down image
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
    if (!hBitmap) {
        std::cerr << "Failed to create DIB section." << std::endl;
        return NULL;
    }

    // Copy data to the bitmap
    HDC hdcBmp = CreateCompatibleDC(hdcMem);
    HGDIOBJ oldBmp = SelectObject(hdcBmp, hBitmap);
    SetDIBits(hdcBmp, hBitmap, 0, mat.rows, mat.data, &bmi, DIB_RGB_COLORS);
    SelectObject(hdcBmp, oldBmp);
    DeleteDC(hdcBmp);

    return hBitmap;
}

void UpdateStaticControl(const cv::Mat& frame) {
    HBITMAP newBitmap = MatToHBITMAP(frame);
    if (newBitmap) {
        SendMessage(hStatic, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)newBitmap);
        DeleteObject(newBitmap);
    }
}

void recv_video() {
    cv::Mat frame;
    std::vector<uchar> buf;
    std::vector<uchar> imgBuf;
    int buf_size;

    while (true) {
        // Receive the size of the incoming data
        if (recv(clientSocket, reinterpret_cast<char*>(&buf_size), sizeof(buf_size), 0) <= 0) {
            std::cerr << "Failed to receive buffer size." << std::endl;
            break;
        }

        // Receive the actual data
        buf.resize(buf_size);
        int totalReceived = 0;
        while (totalReceived < buf_size) {
            int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(buf.data()) + totalReceived, buf_size - totalReceived, 0);
            if (bytesReceived == SOCKET_ERROR) {
                std::cerr << "Failed to receive data." << std::endl;
                break;
            }
            totalReceived += bytesReceived;
        }

        // Decode the received frame
        imgBuf.assign(buf.begin(), buf.end());
        cv::Mat decodedImg = cv::imdecode(imgBuf, cv::IMREAD_COLOR);

        if (!decodedImg.empty()) {
            de.push(decodedImg);
        }
        else {
            std::cerr << "Failed to decode frame." << std::endl;
        }
    }

    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Initialize memory DC
        hdcMem = CreateCompatibleDC(NULL);

        // Create a static control for image display
        hStatic = CreateWindow(
            _T("STATIC"), L"",
            WS_CHILD | WS_VISIBLE | SS_BITMAP,
            10, 10, 460, 240,
            hWnd, NULL, hInst, NULL
        );

        SetTimer(hWnd, 1, 30, NULL); // Timer to update image every 30 ms

        break;
    }
    case WM_TIMER: {
        if (!de.empty()) {
            cv::Mat frame = de.front();
            de.pop();
            UpdateStaticControl(frame);
        }
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance; // Save instance handle

    // Define and register the window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("SimpleWindowClass");

    RegisterClass(&wc);

    // Create the window
    hMainWindow = CreateWindowEx(
        0, _T("SimpleWindowClass"), _T("Simple Window with Static Element"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        NULL, NULL, hInstance, NULL
    );

    if (!hMainWindow) {
        return FALSE;
    }

    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);

    if (acceptConnections() != 0) {
        return 1;
    }

    std::thread(recv_video).detach();

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
