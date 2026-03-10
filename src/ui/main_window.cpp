#include "main_window.h"
#include "capture_overlay.h"
#include "../platform/platform_api.h"
#include "../utils/clipboard_manager.h"
#include "../utils/file_saver.h"
#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPainter>
#include <QPixmap>

namespace easyshotter {

#ifdef Q_OS_WIN
static MainWindow* g_mainWindowInstance = nullptr;
#endif

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
    setupSingleInstanceListener();
}

MainWindow::~MainWindow()
{
    if (m_hotkeyId >= 0) {
        m_platformApi->unregisterHotkey(m_hotkeyId);
    }
#ifdef Q_OS_WIN
    g_mainWindowInstance = nullptr;
    if (m_hiddenWindow) {
        DestroyWindow(m_hiddenWindow);
        m_hiddenWindow = nullptr;
    }
#endif
}

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);
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
    m_trayIcon->showMessage("EasyShotter", "EasyShotter is running. Use Ctrl+Shift+A or click tray icon to capture.",
                            QSystemTrayIcon::Information, 3000);
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
    // Try to load icon from resource file first
    QIcon icon(":/resources/app_icon.ico");
    if (!icon.isNull() && !icon.availableSizes().isEmpty()) return icon;

    // Fallback: load from file path relative to executable
    QString iconPath = QApplication::applicationDirPath() + "/app_icon.ico";
    if (QFileInfo::exists(iconPath)) {
        icon = QIcon(iconPath);
        if (!icon.isNull()) return icon;
    }

    // Final fallback: draw programmatically
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 120, 215), 2));
    painter.setBrush(QColor(0, 120, 215));
    painter.drawRoundedRect(4, 10, 24, 16, 3, 3);
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(16, 18), 5, 5);
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
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        onStartCapture();
    }
}

void MainWindow::onStartCapture()
{
    if (!m_captureOverlay) {
        m_captureOverlay = new CaptureOverlay(m_platformApi.get(), nullptr);
        connect(m_captureOverlay, &CaptureOverlay::captureConfirmed,
                this, &MainWindow::onCaptureConfirmed);
        connect(m_captureOverlay, &CaptureOverlay::captureSaveRequested,
                this, [this](const QPixmap& pixmap, const QRect& region, CaptureOverlay::SaveAction action) {
                    onCaptureSaveRequested(pixmap, region, static_cast<int>(action));
                });
        connect(m_captureOverlay, &CaptureOverlay::captureCancelled,
                this, &MainWindow::onCaptureCancelled);
    }
    if (m_captureOverlay->isVisible()) return;
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

void MainWindow::onCaptureSaveRequested(const QPixmap& pixmap, const QRect& region, int action)
{
    Q_UNUSED(region)
    if (action == static_cast<int>(CaptureOverlay::SaveAction::SaveToDesktop)) {
        if (m_fileSaver->saveToDesktop(pixmap)) {
            m_trayIcon->showMessage("EasyShotter",
                QString::fromUtf8("截图已保存到桌面"),
                QSystemTrayIcon::Information, 2000);
        }
    } else if (action == static_cast<int>(CaptureOverlay::SaveAction::SaveToFolder)) {
        QString filePath = QFileDialog::getSaveFileName(
            nullptr,
            QString::fromUtf8("保存截图"),
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/" +
                FileSaver::generateFileName(FileSaver::Format::PNG),
            "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
        if (!filePath.isEmpty()) {
            if (m_fileSaver->saveToFile(pixmap, filePath)) {
                m_trayIcon->showMessage("EasyShotter",
                    QString::fromUtf8("截图已保存到 ") + filePath,
                    QSystemTrayIcon::Information, 2000);
            }
        }
    }
}

void MainWindow::onCaptureCancelled()
{
    // Nothing to do, overlay already hidden
}

#ifdef Q_OS_WIN
LRESULT CALLBACK MainWindow::hiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_mainWindowInstance && msg == g_mainWindowInstance->m_captureMsg) {
        g_mainWindowInstance->onStartCapture();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

void MainWindow::setupSingleInstanceListener()
{
#ifdef Q_OS_WIN
    g_mainWindowInstance = this;
    m_captureMsg = RegisterWindowMessageW(L"EasyShotter_StartCapture");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = hiddenWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"EasyShotter_HiddenWindow";
    RegisterClassW(&wc);

    m_hiddenWindow = CreateWindowW(
        L"EasyShotter_HiddenWindow", L"EasyShotter_HiddenWindow",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
        GetModuleHandleW(nullptr), nullptr);
#endif
}

} // namespace easyshotter
