#include "window_detector.h"

namespace simpleshotter {

WindowDetector::WindowDetector(PlatformApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

WindowDetector::~WindowDetector() = default;

void WindowDetector::refresh()
{
    m_windows = m_api->getVisibleWindows();
}

WindowInfo WindowDetector::findWindowAt(const QPoint& screenPos) const
{
    for (const auto& win : m_windows) {
        if (win.rect.contains(screenPos)) {
            return win;
        }
    }
    return {};
}

const std::vector<WindowInfo>& WindowDetector::windows() const
{
    return m_windows;
}

} // namespace simpleshotter
