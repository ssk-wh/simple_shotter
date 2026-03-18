#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace simpleshotter {

class PlatformApi;
class CaptureOverlay;
class ClipboardManager;
class FileSaver;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onStartCapture();
    void onCaptureConfirmed(const QPixmap& pixmap, const QRect& region);
    void onCaptureSaveRequested(const QPixmap& pixmap, const QRect& region, int action);
    void onCaptureCancelled();

private:
    void setupTrayIcon();
    void setupHotkey();
    void setupSingleInstanceListener();
    QIcon createTrayIcon() const;

    std::unique_ptr<PlatformApi> m_platformApi;
    CaptureOverlay* m_captureOverlay = nullptr;
    ClipboardManager* m_clipboardManager = nullptr;
    FileSaver* m_fileSaver = nullptr;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;

    int m_hotkeyId = -1;

#ifdef Q_OS_WIN
    HWND m_hiddenWindow = nullptr;
    UINT m_captureMsg = 0;
    static LRESULT CALLBACK hiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace simpleshotter
