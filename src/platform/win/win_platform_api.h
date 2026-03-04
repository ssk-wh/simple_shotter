#pragma once

#include "platform/platform_api.h"

#ifdef _WIN32
#include <Windows.h>
#include <UIAutomation.h>
#include <QAbstractNativeEventFilter>
#include <unordered_map>
#endif

namespace easyshotter {

#ifdef _WIN32
class WinHotkeyFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;
    std::unordered_map<int, std::function<void()>>* hotkeyCallbacks = nullptr;
};
#endif

class WinPlatformApi : public PlatformApi {
public:
    WinPlatformApi();
    ~WinPlatformApi() override;

    QPixmap captureScreen() override;
    QPixmap captureRegion(const QRect& region) override;
    std::vector<WindowInfo> getVisibleWindows() override;
    WindowInfo windowAtPoint(const QPoint& screenPos) override;
    ControlInfo controlAtPoint(const QPoint& screenPos) override;
    std::vector<ControlInfo> getWindowControls(NativeWindowHandle window) override;
    int registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                       std::function<void()> callback) override;
    void unregisterHotkey(int hotkeyId) override;

private:
#ifdef _WIN32
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
    bool isValidWindow(HWND hwnd) const;
    QRect getWindowRect(HWND hwnd) const;

    IUIAutomation* m_uiAutomation = nullptr;
    int m_nextHotkeyId = 1;
    std::unordered_map<int, std::function<void()>> m_hotkeyCallbacks;
    WinHotkeyFilter m_hotkeyFilter;
#endif
};

} // namespace easyshotter
