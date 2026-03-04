#include "linux_platform_api.h"

#if defined(__linux__)

#include <QGuiApplication>
#include <QScreen>

namespace easyshotter {

LinuxPlatformApi::LinuxPlatformApi() = default;
LinuxPlatformApi::~LinuxPlatformApi() = default;

QPixmap LinuxPlatformApi::captureScreen()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return {};
    return screen->grabWindow(0);
}

QPixmap LinuxPlatformApi::captureRegion(const QRect& region)
{
    QPixmap full = captureScreen();
    return full.copy(region);
}

std::vector<WindowInfo> LinuxPlatformApi::getVisibleWindows()
{
    // TODO: implement using XCB xcb_query_tree
    return {};
}

WindowInfo LinuxPlatformApi::windowAtPoint(const QPoint& screenPos)
{
    Q_UNUSED(screenPos)
    // TODO: implement using XCB
    return {};
}

ControlInfo LinuxPlatformApi::controlAtPoint(const QPoint& screenPos)
{
    Q_UNUSED(screenPos)
    // TODO: implement using AT-SPI2
    return {};
}

std::vector<ControlInfo> LinuxPlatformApi::getWindowControls(NativeWindowHandle window)
{
    Q_UNUSED(window)
    // TODO: implement using AT-SPI2
    return {};
}

int LinuxPlatformApi::registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                                      std::function<void()> callback)
{
    Q_UNUSED(key)
    Q_UNUSED(modifiers)
    Q_UNUSED(callback)
    // TODO: implement using XGrabKey
    return -1;
}

void LinuxPlatformApi::unregisterHotkey(int hotkeyId)
{
    Q_UNUSED(hotkeyId)
    // TODO: implement using XUngrabKey
}

std::unique_ptr<PlatformApi> PlatformApi::create()
{
    return std::make_unique<LinuxPlatformApi>();
}

} // namespace easyshotter

#endif // __linux__
