#include "clipboard_manager.h"
#include <QClipboard>
#include <QApplication>

namespace easyshotter {

ClipboardManager::ClipboardManager(QObject* parent)
    : QObject(parent)
{
}

ClipboardManager::~ClipboardManager() = default;

void ClipboardManager::copyToClipboard(const QPixmap& pixmap)
{
    QApplication::clipboard()->setPixmap(pixmap);
}

} // namespace easyshotter
