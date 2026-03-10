#include <QApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include "ui/main_window.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    // Single instance check using named mutex
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"EasyShotter_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running, send it a message to trigger capture
        // FindWindowExW with HWND_MESSAGE is required to find message-only windows
        HWND hwnd = FindWindowExW(HWND_MESSAGE, nullptr, L"EasyShotter_HiddenWindow", nullptr);
        if (hwnd) {
            UINT msg = RegisterWindowMessageW(L"EasyShotter_StartCapture");
            PostMessageW(hwnd, msg, 0, 0);
        }
        CloseHandle(hMutex);
        return 0;
    }
#endif

    QApplication app(argc, argv);
    app.setApplicationName("EasyShotter");
    app.setApplicationVersion("0.2.0");
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "EasyShotter",
            "System tray is not available on this system.");
        return 1;
    }

    easyshotter::MainWindow mainWindow;

    int ret = app.exec();

#ifdef Q_OS_WIN
    if (hMutex) {
        CloseHandle(hMutex);
    }
#endif
    return ret;
}
