#pragma once

#include <QObject>
#include <QPixmap>

namespace simpleshotter {

class ClipboardManager : public QObject {
    Q_OBJECT
public:
    explicit ClipboardManager(QObject* parent = nullptr);
    ~ClipboardManager();

    void copyToClipboard(const QPixmap& pixmap);
};

} // namespace simpleshotter
