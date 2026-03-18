#include "screen_capture.h"

namespace simpleshotter {

ScreenCapture::ScreenCapture(PlatformApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

ScreenCapture::~ScreenCapture() = default;

QPixmap ScreenCapture::captureFullScreen()
{
    m_lastCapture = m_api->captureScreen();
    emit captureCompleted(m_lastCapture);
    return m_lastCapture;
}

QPixmap ScreenCapture::captureRegion(const QRect& region)
{
    m_lastCapture = m_api->captureRegion(region);
    emit captureCompleted(m_lastCapture);
    return m_lastCapture;
}

} // namespace simpleshotter
