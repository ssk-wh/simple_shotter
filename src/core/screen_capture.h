#pragma once

#include <QObject>
#include <QPixmap>
#include "platform/platform_api.h"

namespace easyshotter {

class ScreenCapture : public QObject {
    Q_OBJECT
public:
    explicit ScreenCapture(PlatformApi* api, QObject* parent = nullptr);
    ~ScreenCapture();

    QPixmap captureFullScreen();
    QPixmap captureRegion(const QRect& region);

signals:
    void captureCompleted(const QPixmap& pixmap);

private:
    PlatformApi* m_api;
    QPixmap m_lastCapture;
};

} // namespace easyshotter
