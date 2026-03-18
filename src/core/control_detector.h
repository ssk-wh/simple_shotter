#pragma once

#include <QObject>
#include <QPoint>
#include <QElapsedTimer>
#include "platform/platform_api.h"

namespace simpleshotter {

class ControlDetector : public QObject {
    Q_OBJECT
public:
    explicit ControlDetector(PlatformApi* api, QObject* parent = nullptr);
    ~ControlDetector();

    ControlInfo findControlAt(const QPoint& screenPos);

private:
    static constexpr qint64 THROTTLE_MS = 80;

    PlatformApi* m_api;
    ControlInfo m_lastResult;
    QPoint m_lastPos;
    QElapsedTimer m_throttleTimer;
};

} // namespace simpleshotter
