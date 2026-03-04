#pragma once

#include <QObject>
#include "platform/platform_api.h"

namespace easyshotter {

class WindowDetector : public QObject {
    Q_OBJECT
public:
    explicit WindowDetector(PlatformApi* api, QObject* parent = nullptr);
    ~WindowDetector();

    void refresh();
    WindowInfo findWindowAt(const QPoint& screenPos) const;
    const std::vector<WindowInfo>& windows() const;

private:
    PlatformApi* m_api;
    std::vector<WindowInfo> m_windows;
};

} // namespace easyshotter
