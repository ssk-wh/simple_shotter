#include "main_window.h"
#include "capture_overlay.h"
#include "../platform/platform_api.h"
#include "../utils/clipboard_manager.h"
#include "../utils/file_saver.h"
#include <QAction>
#include <QApplication>
#include <QPainter>
#include <QPixmap>

namespace easyshotter {

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_platformApi(PlatformApi::create())
{
    setWindowTitle("EasyShotter");
    hide(); // Tray-mode application, main window is hidden

    m_clipboardManager = new ClipboardManager(this);
    m_fileSaver = new FileSaver(this);

    setupTrayIcon();
    setupHotkey();
}

MainWindow::~MainWindow()
{
    if (m_hotkeyId >= 0) {
        m_platformApi->unregisterHotkey(m_hotkeyId);
    }
}

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Screenshot", this, &MainWindow::onStartCapture);
    m_trayMenu->addSeparator();
    auto* settingsAction = m_trayMenu->addAction("Settings");
    settingsAction->setEnabled(false); // Reserved for future
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("EasyShotter - Screenshot Tool");
    m_trayIcon->setIcon(createTrayIcon());
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    m_trayIcon->show();
}

void MainWindow::setupHotkey()
{
    // Register Ctrl+Shift+A as global hotkey for screenshot
    m_hotkeyId = m_platformApi->registerHotkey(
        Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier,
        [this]() { onStartCapture(); });
}

QIcon MainWindow::createTrayIcon() const
{
    // Draw a simple camera icon programmatically
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Camera body
    painter.setPen(QPen(QColor(0, 120, 215), 2));
    painter.setBrush(QColor(0, 120, 215));
    painter.drawRoundedRect(4, 10, 24, 16, 3, 3);

    // Camera lens
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(16, 18), 5, 5);

    // Camera top bump
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 120, 215));
    painter.drawRect(11, 6, 10, 6);

    painter.end();
    return QIcon(pixmap);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    // Hotkey events are handled by PlatformApi's native event filter
    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onStartCapture();
    }
}

void MainWindow::onStartCapture()
{
    if (!m_captureOverlay) {
        m_captureOverlay = new CaptureOverlay(m_platformApi.get(), nullptr);
        connect(m_captureOverlay, &CaptureOverlay::captureConfirmed,
                this, &MainWindow::onCaptureConfirmed);
        connect(m_captureOverlay, &CaptureOverlay::captureCancelled,
                this, &MainWindow::onCaptureCancelled);
    }
    m_captureOverlay->startCapture();
}

void MainWindow::onCaptureConfirmed(const QPixmap& pixmap, const QRect& region)
{
    Q_UNUSED(region)
    // Default action: copy to clipboard
    m_clipboardManager->copyToClipboard(pixmap);
    m_trayIcon->showMessage("EasyShotter", "Screenshot copied to clipboard!",
                            QSystemTrayIcon::Information, 2000);
}

void MainWindow::onCaptureCancelled()
{
    // Nothing to do, overlay already hidden
}

} // namespace easyshotter
