#pragma once

#include "platform/platform_api.h"
#include <unordered_map>

namespace easyshotter {

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
    int m_nextHotkeyId = 1;
    std::unordered_map<int, std::function<void()>> m_hotkeyCallbacks;
};

} // namespace easyshotter
