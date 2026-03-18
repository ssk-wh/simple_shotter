#include "control_detector.h"

namespace simpleshotter {

ControlDetector::ControlDetector(PlatformApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
    // Do not start timer here; first findControlAt() call should always
    // query the API without being throttled.
}

ControlDetector::~ControlDetector() = default;

ControlInfo ControlDetector::findControlAt(const QPoint& screenPos)
{
    if (screenPos == m_lastPos && m_lastResult.rect.isValid()) {
        return m_lastResult;
    }

    if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < THROTTLE_MS) {
        return m_lastResult;
    }

    m_throttleTimer.restart();
    m_lastPos = screenPos;
    m_lastResult = m_api->controlAtPoint(screenPos);
    return m_lastResult;
}

} // namespace simpleshotter
