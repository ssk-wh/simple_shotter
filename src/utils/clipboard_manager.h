#pragma once

#include <QObject>
#include <QPixmap>

namespace easyshotter {

class ClipboardManager : public QObject {
    Q_OBJECT
public:
    explicit ClipboardManager(QObject* parent = nullptr);
    ~ClipboardManager();

    void copyToClipboard(const QPixmap& pixmap);
};

} // namespace easyshotter
