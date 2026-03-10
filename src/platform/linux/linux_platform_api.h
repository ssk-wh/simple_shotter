#pragma once

#include "platform/platform_api.h"
#include <unordered_map>

#if defined(__linux__)

struct xcb_connection_t;
struct _XDisplay;

namespace easyshotter {

class LinuxHotkeyFilter;

class LinuxPlatformApi : public PlatformApi {
public:
    LinuxPlatformApi();
    ~LinuxPlatformApi() override;

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
    // X11/XCB helpers
    uint32_t xcbAtom(const char* name);
    QString getWindowTitle(uint32_t window);
    QRect getWindowGeometry(uint32_t window);
    bool isWindowMapped(uint32_t window);
    uint32_t getWindowPid(uint32_t window);

    // AT-SPI helpers
    void initAtSpi();
    static ControlType mapAtspiRole(int role);
    void collectAccessibleControls(void* accessible, std::vector<ControlInfo>& result,
                                   int depth, const QRect& windowRect);

    // XCB connection
    xcb_connection_t* m_xcb = nullptr;
    _XDisplay* m_display = nullptr;
    uint32_t m_rootWindow = 0;
    int m_defaultScreen = 0;

    // XCB atom cache
    std::unordered_map<std::string, uint32_t> m_atomCache;

    // AT-SPI
    bool m_atspiInited = false;

    // Hotkeys
    int m_nextHotkeyId = 1;
    struct HotkeyBinding {
        unsigned int keycode;
        unsigned int modifiers;
        std::function<void()> callback;
    };
    std::unordered_map<int, HotkeyBinding> m_hotkeys;
    LinuxHotkeyFilter* m_hotkeyFilter = nullptr;
};

} // namespace easyshotter

#endif // __linux__
