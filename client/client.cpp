#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "opencv_world4100.lib")
#pragma comment(lib, "opencv_world4100d.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345

// Function to capture the screen
cv::Mat captureScreen() {
    // Get screen dimensions
    HWND hwnd = GetDesktopWindow();
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmScreen);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld);

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative to ensure top-down image
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    cv::Mat img(height, width, CV_8UC3);
    GetDIBits(hdcScreen, hbmScreen, 0, height, img.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hbmScreen);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);

    return img;
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    // Convert the IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported." << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server." << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cv::Mat frame;
    std::vector<uchar> buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 90 };  // JPEG quality

    while (true) {
        frame = captureScreen();
        if (frame.empty()) {
            std::cerr << "Error capturing frame." << std::endl;
            break;
        }

        // Encode the frame as JPEG
        cv::imencode(".jpg", frame, buf, params);
        int buf_size = buf.size();

        // Send the size of the buffer first
        if (send(sock, reinterpret_cast<char*>(&buf_size), sizeof(buf_size), 0) == SOCKET_ERROR) {
            std::cerr << "Error sending buffer size." << std::endl;
            break;
        }

        // Send the encoded frame
        if (send(sock, reinterpret_cast<char*>(buf.data()), buf_size, 0) == SOCKET_ERROR) {
            std::cerr << "Error sending frame data." << std::endl;
            break;
        }

        // Optionally sleep to reduce CPU usage
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
