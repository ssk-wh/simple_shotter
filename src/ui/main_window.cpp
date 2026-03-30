#include "main_window.h"
#include "capture_overlay.h"
#include "../platform/platform_api.h"
#include "../utils/clipboard_manager.h"
#include "../utils/file_saver.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QPainter>
#include <QPixmap>
#include <QSettings>

namespace simpleshotter {

#ifdef Q_OS_WIN
static MainWindow* g_mainWindowInstance = nullptr;
#endif

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_platformApi(PlatformApi::create())
{
    setWindowTitle("SimpleShotter");
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

    auto* autoStartAction = m_trayMenu->addAction(QString::fromUtf8("开机自启动"));
    autoStartAction->setCheckable(true);
    autoStartAction->setChecked(isAutoStartEnabled());
    connect(autoStartAction, &QAction::toggled, this, &MainWindow::setAutoStartEnabled);

    m_trayMenu->addSeparator();
    m_trayMenu->addAction(QString::fromUtf8("退出"), qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("SimpleShotter - Screenshot Tool");
    m_trayIcon->setIcon(createTrayIcon());
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    m_trayIcon->show();
    m_trayIcon->showMessage("SimpleShotter", "SimpleShotter is running. Use F1 or click tray icon to capture.",
                            QSystemTrayIcon::Information, 3000);
}

void MainWindow::setupHotkey()
{
    // Register F1 as global hotkey for screenshot
    m_hotkeyId = m_platformApi->registerHotkey(
        Qt::Key_F1, Qt::NoModifier,
        [this]() { onStartCapture(); });
}

QIcon MainWindow::createTrayIcon() const
{
    // Try ICO file next to executable first (deployed)
    QString appDir = QCoreApplication::applicationDirPath();
    QIcon icon(appDir + "/app_icon.ico");
    if (!icon.isNull() && !icon.availableSizes().isEmpty()) return icon;

    // Try SVG from embedded Qt resource
    icon = QIcon(":/app-icon.svg");
    if (!icon.isNull() && !icon.availableSizes().isEmpty()) return icon;

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

static const char kAutoStartRegKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char kAppName[] = "SimpleShotter";

bool MainWindow::isAutoStartEnabled() const
{
#ifdef Q_OS_WIN
    QSettings reg(kAutoStartRegKey, QSettings::NativeFormat);
    return reg.contains(kAppName);
#else
    return false;
#endif
}

void MainWindow::setAutoStartEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings reg(kAutoStartRegKey, QSettings::NativeFormat);
    if (enabled) {
        QString appPath = QCoreApplication::applicationFilePath().replace('/', '\\');
        reg.setValue(kAppName, QString("\"%1\"").arg(appPath));
    } else {
        reg.remove(kAppName);
    }
#else
    Q_UNUSED(enabled)
#endif
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
    if (m_captureOverlay && m_captureOverlay->isVisible()) return;

    // Recreate overlay each time to avoid stale DPI/geometry context
    // when switching between screens with different resolutions
    delete m_captureOverlay;
    m_captureOverlay = new CaptureOverlay(m_platformApi.get(), m_trayIcon, nullptr);
    connect(m_captureOverlay, &CaptureOverlay::captureConfirmed,
            this, &MainWindow::onCaptureConfirmed);
    connect(m_captureOverlay, &CaptureOverlay::captureSaveRequested,
            this, [this](const QPixmap& pixmap, const QRect& region, CaptureOverlay::SaveAction action) {
                onCaptureSaveRequested(pixmap, region, static_cast<int>(action));
            });
    connect(m_captureOverlay, &CaptureOverlay::captureCancelled,
            this, &MainWindow::onCaptureCancelled);
    m_captureOverlay->startCapture();
}

void MainWindow::onCaptureConfirmed(const QPixmap& pixmap, const QRect& region)
{
    Q_UNUSED(region)
    // Default action: copy to clipboard
    m_clipboardManager->copyToClipboard(pixmap);
    m_trayIcon->showMessage("SimpleShotter", "Screenshot copied to clipboard!",
                            QSystemTrayIcon::Information, 2000);
}

void MainWindow::onCaptureSaveRequested(const QPixmap& pixmap, const QRect& region, int action)
{
    Q_UNUSED(region)
    if (action == static_cast<int>(CaptureOverlay::SaveAction::SaveToDesktop)) {
        QString savedPath = m_fileSaver->saveToDesktop(pixmap);
        if (!savedPath.isEmpty()) {
            // 多格式剪贴板：文本区粘贴得到路径，图片区粘贴得到图片
            QMimeData* mimeData = new QMimeData();
            mimeData->setText(savedPath);
            mimeData->setImageData(pixmap.toImage());
            QApplication::clipboard()->setMimeData(mimeData);
            m_trayIcon->showMessage("SimpleShotter",
                QString::fromUtf8("截图已保存到桌面（已复制到剪贴板）"),
                QSystemTrayIcon::Information, 3000);
        } else {
            m_trayIcon->showMessage("SimpleShotter",
                QString::fromUtf8("保存失败：") + m_fileSaver->getLastError(),
                QSystemTrayIcon::Warning, 3000);
        }
    } else if (action == static_cast<int>(CaptureOverlay::SaveAction::SaveToFolder)) {
        QString filePath = QFileDialog::getSaveFileName(
            nullptr,
            QString::fromUtf8("保存截图"),
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/" +
                FileSaver::generateFileName(FileSaver::Format::PNG),
            "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
        if (!filePath.isEmpty()) {
            // 检查文件是否已存在
            QFileInfo fileInfo(filePath);
            if (fileInfo.exists()) {
                QMessageBox::StandardButton ret = QMessageBox::question(this,
                    QString::fromUtf8("文件已存在"),
                    QString::fromUtf8("文件 %1 已存在，是否覆盖？").arg(fileInfo.fileName()),
                    QMessageBox::Yes | QMessageBox::No);
                if (ret != QMessageBox::Yes) {
                    // 用户选择不覆盖
                    return;
                }
            }

            QString savedPath = m_fileSaver->saveToFile(pixmap, filePath);
            if (!savedPath.isEmpty()) {
                // 多格式剪贴板：文本区粘贴得到路径，图片区粘贴得到图片
                QMimeData* mimeData = new QMimeData();
                mimeData->setText(savedPath);
                mimeData->setImageData(pixmap.toImage());
                QApplication::clipboard()->setMimeData(mimeData);
                m_trayIcon->showMessage("SimpleShotter",
                    QString::fromUtf8("已保存到：%1（已复制到剪贴板）").arg(fileInfo.fileName()),
                    QSystemTrayIcon::Information, 3000);
            } else {
                m_trayIcon->showMessage("SimpleShotter",
                    QString::fromUtf8("保存失败：") + m_fileSaver->getLastError(),
                    QSystemTrayIcon::Warning, 3000);
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
    m_captureMsg = RegisterWindowMessageW(L"SimpleShotter_StartCapture");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = hiddenWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SimpleShotter_HiddenWindow";
    RegisterClassW(&wc);

    m_hiddenWindow = CreateWindowW(
        L"SimpleShotter_HiddenWindow", L"SimpleShotter_HiddenWindow",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
        GetModuleHandleW(nullptr), nullptr);
#endif
}

} // namespace simpleshotter
