#pragma once

#include "platform/platform_api.h"
#include <QPixmap>
#include <QImage>

namespace simpleshotter {

class MockPlatformApi : public PlatformApi {
public:
    // Configurable return values
    QPixmap mockScreenPixmap;
    QPixmap mockRegionPixmap;
    std::vector<WindowInfo> mockWindows;
    WindowInfo mockWindowAtPoint;
    ControlInfo mockControlAtPoint;
    std::vector<ControlInfo> mockControls;
    int mockHotkeyId = 1;
    int registerHotkeyCallCount = 0;
    int unregisterHotkeyCallCount = 0;
    int controlAtPointCallCount = 0;
    std::function<void()> lastHotkeyCallback;

    MockPlatformApi()
    {
        // Default: create a 1920x1080 non-null pixmap
        QImage img(1920, 1080, QImage::Format_ARGB32);
        img.fill(Qt::blue);
        mockScreenPixmap = QPixmap::fromImage(img);

        QImage regionImg(100, 100, QImage::Format_ARGB32);
        regionImg.fill(Qt::red);
        mockRegionPixmap = QPixmap::fromImage(regionImg);
    }

    QPixmap captureScreen() override
    {
        return mockScreenPixmap;
    }

    QPixmap captureRegion(const QRect& /*region*/) override
    {
        return mockRegionPixmap;
    }

    std::vector<WindowInfo> getVisibleWindows() override
    {
        return mockWindows;
    }

    WindowInfo windowAtPoint(const QPoint& /*screenPos*/) override
    {
        return mockWindowAtPoint;
    }

    ControlInfo controlAtPoint(const QPoint& /*screenPos*/) override
    {
        ++controlAtPointCallCount;
        return mockControlAtPoint;
    }

    std::vector<ControlInfo> getWindowControls(NativeWindowHandle /*window*/) override
    {
        return mockControls;
    }

    int registerHotkey(Qt::Key /*key*/, Qt::KeyboardModifiers /*modifiers*/,
                       std::function<void()> callback) override
    {
        ++registerHotkeyCallCount;
        lastHotkeyCallback = callback;
        return mockHotkeyId;
    }

    void unregisterHotkey(int /*hotkeyId*/) override
    {
        ++unregisterHotkeyCallCount;
    }
};

} // namespace simpleshotter
