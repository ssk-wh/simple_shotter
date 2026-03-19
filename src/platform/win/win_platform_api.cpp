#include "win_platform_api.h"

#ifdef _WIN32

#include <dwmapi.h>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QScreen>
#include <QWindow>
#include <QImage>
#include <QPainter>

namespace simpleshotter {

bool WinHotkeyFilter::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(result)
    if (eventType != "windows_generic_MSG") return false;

    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && hotkeyCallbacks) {
        int id = static_cast<int>(msg->wParam);
        auto it = hotkeyCallbacks->find(id);
        if (it != hotkeyCallbacks->end()) {
            it->second();
            return true;
        }
    }
    return false;
}

WinPlatformApi::WinPlatformApi()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                  reinterpret_cast<void**>(&m_uiAutomation));
    if (FAILED(hr)) {
        m_uiAutomation = nullptr;
    }

    m_hotkeyFilter.hotkeyCallbacks = &m_hotkeyCallbacks;
    QCoreApplication::instance()->installNativeEventFilter(&m_hotkeyFilter);
}

WinPlatformApi::~WinPlatformApi()
{
    QCoreApplication::instance()->removeNativeEventFilter(&m_hotkeyFilter);

    for (auto& [id, _] : m_hotkeyCallbacks) {
        UnregisterHotKey(nullptr, id);
    }
    m_hotkeyCallbacks.clear();

    if (m_uiAutomation) {
        m_uiAutomation->Release();
        m_uiAutomation = nullptr;
    }
    CoUninitialize();
}

BOOL CALLBACK WinPlatformApi::enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* windows = reinterpret_cast<std::vector<std::pair<HWND, int>>*>(lParam);
    windows->push_back({hwnd, static_cast<int>(windows->size())});
    return TRUE;
}

bool WinPlatformApi::isValidWindow(HWND hwnd) const
{
    if (!IsWindowVisible(hwnd)) return false;
    if (IsIconic(hwnd)) return false;

    // Exclude desktop and shell windows
    if (hwnd == GetDesktopWindow()) return false;
    if (hwnd == GetShellWindow()) return false;

    // Exclude desktop background windows (Progman, WorkerW)
    wchar_t className[256] = {};
    GetClassNameW(hwnd, className, 256);
    if (wcscmp(className, L"Progman") == 0) return false;
    if (wcscmp(className, L"WorkerW") == 0) return false;

    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;

    // Exclude cloaked windows (Win10/11 hidden UWP windows)
    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return false;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0) return false;

    return true;
}

QRect WinPlatformApi::getWindowRect(HWND hwnd) const
{
    RECT rect;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                        &rect, sizeof(rect));
    if (FAILED(hr)) {
        GetWindowRect(hwnd, &rect);
    }
    return QRect(rect.left, rect.top,
                 rect.right - rect.left, rect.bottom - rect.top);
}

void WinPlatformApi::updateScreenMappings()
{
    m_screenMappings.clear();

    // Collect physical monitor rects from Win32
    struct MonitorData { QRect rect; };
    std::vector<MonitorData> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto* list = reinterpret_cast<std::vector<MonitorData>*>(lParam);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(hMon, &mi);
        list->push_back({QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                               mi.rcMonitor.right - mi.rcMonitor.left,
                               mi.rcMonitor.bottom - mi.rcMonitor.top)});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));

    // Match each monitor with a QScreen by physical size
    QList<QScreen*> screens = QGuiApplication::screens();
    std::vector<bool> used(screens.size(), false);
    for (const auto& mon : monitors) {
        for (int i = 0; i < screens.size(); i++) {
            if (used[i]) continue;
            qreal dpr = screens[i]->devicePixelRatio();
            QSize physSize(qRound(screens[i]->geometry().width() * dpr),
                           qRound(screens[i]->geometry().height() * dpr));
            // Fuzzy match: allow ±1 pixel rounding difference
            if (qAbs(physSize.width() - mon.rect.width()) <= 1 &&
                qAbs(physSize.height() - mon.rect.height()) <= 1) {
                m_screenMappings.push_back({mon.rect, screens[i]->geometry(), dpr});
                used[i] = true;
                break;
            }
        }
    }
}

QRect WinPlatformApi::physicalToLogical(const QRect& physical) const
{
    QPoint center = physical.center();
    for (const auto& m : m_screenMappings) {
        if (m.physicalRect.contains(center)) {
            QPoint offset = physical.topLeft() - m.physicalRect.topLeft();
            return QRect(
                m.logicalRect.topLeft() + QPoint(qRound(offset.x() / m.dpr), qRound(offset.y() / m.dpr)),
                QSize(qRound(physical.width() / m.dpr), qRound(physical.height() / m.dpr))
            );
        }
    }
    // Fallback: primary screen DPR
    qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    return QRect(QPoint(qRound(physical.x() / dpr), qRound(physical.y() / dpr)),
                 QSize(qRound(physical.width() / dpr), qRound(physical.height() / dpr)));
}

QPoint WinPlatformApi::logicalToPhysical(const QPoint& logical) const
{
    for (const auto& m : m_screenMappings) {
        if (m.logicalRect.contains(logical)) {
            QPoint offset = logical - m.logicalRect.topLeft();
            return m.physicalRect.topLeft() + QPoint(qRound(offset.x() * m.dpr), qRound(offset.y() * m.dpr));
        }
    }
    qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    return QPoint(qRound(logical.x() * dpr), qRound(logical.y() * dpr));
}

QPixmap WinPlatformApi::captureScreen()
{
    updateScreenMappings();

    // 1. Win32 BitBlt capture at physical resolution
    int px = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int py = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int pw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int ph = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, pw, ph);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, hBitmap));
    BitBlt(memDC, 0, 0, pw, ph, screenDC, px, py, SRCCOPY);
    SelectObject(memDC, oldBitmap);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = pw;
    bi.biHeight = -ph;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage image(pw, ph, QImage::Format_ARGB32);
    GetDIBits(memDC, hBitmap, 0, ph, image.bits(),
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    QPixmap physCapture = QPixmap::fromImage(image);

    // 2. Compute logical virtual desktop
    QRect logicalVirtual;
    for (QScreen* screen : QGuiApplication::screens()) {
        logicalVirtual = logicalVirtual.united(screen->geometry());
    }

    // 3. Composite at logical resolution — each screen's physical pixels
    //    are scaled to fit its logical area. DPR=1, size=logical desktop.
    QPixmap result(logicalVirtual.size());
    result.fill(Qt::black);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const auto& mapping : m_screenMappings) {
        QRect src = mapping.physicalRect.translated(-px, -py);
        QRect tgt = mapping.logicalRect.translated(-logicalVirtual.topLeft());
        painter.drawPixmap(tgt, physCapture, src);
    }
    painter.end();

    return result;
}

QPixmap WinPlatformApi::captureRegion(const QRect& region)
{
    QPixmap full = captureScreen();
    return full.copy(region);
}

std::vector<WindowInfo> WinPlatformApi::getVisibleWindows()
{
    std::vector<std::pair<HWND, int>> rawWindows;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&rawWindows));

    std::vector<WindowInfo> result;
    int zOrder = 0;
    for (auto& [hwnd, _] : rawWindows) {
        if (!isValidWindow(hwnd)) continue;

        WindowInfo info;
        info.handle = reinterpret_cast<quintptr>(hwnd);
        info.rect = physicalToLogical(getWindowRect(hwnd));
        info.zOrder = zOrder++;
        info.isTopLevel = true;

        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        info.title = QString::fromWCharArray(title);

        result.push_back(info);
    }
    return result;
}

WindowInfo WinPlatformApi::windowAtPoint(const QPoint& screenPos)
{
    auto windows = getVisibleWindows();
    for (const auto& win : windows) {
        if (win.rect.contains(screenPos)) {
            return win;
        }
    }
    return {};
}

ControlInfo WinPlatformApi::controlAtPoint(const QPoint& screenPos)
{
    ControlInfo info;
    if (!m_uiAutomation) return info;

    // Convert logical screen coords to physical for Win32 API
    QPoint physPos = logicalToPhysical(screenPos);
    IUIAutomationElement* pElement = nullptr;
    POINT pt = {physPos.x(), physPos.y()};
    HRESULT hr = m_uiAutomation->ElementFromPoint(pt, &pElement);
    if (FAILED(hr) || !pElement) return info;

    RECT boundingRect;
    hr = pElement->get_CurrentBoundingRectangle(&boundingRect);
    if (SUCCEEDED(hr)) {
        QRect physRect(boundingRect.left, boundingRect.top,
                       boundingRect.right - boundingRect.left,
                       boundingRect.bottom - boundingRect.top);
        info.rect = physicalToLogical(physRect);
    }

    CONTROLTYPEID controlType;
    hr = pElement->get_CurrentControlType(&controlType);
    if (SUCCEEDED(hr)) {
        switch (controlType) {
        case UIA_ButtonControlTypeId:    info.type = ControlType::Button; break;
        case UIA_CheckBoxControlTypeId:  info.type = ControlType::CheckBox; break;
        case UIA_RadioButtonControlTypeId: info.type = ControlType::RadioButton; break;
        case UIA_ComboBoxControlTypeId:  info.type = ControlType::ComboBox; break;
        case UIA_EditControlTypeId:      info.type = ControlType::Edit; break;
        case UIA_TextControlTypeId:      info.type = ControlType::Label; break;
        case UIA_ListControlTypeId:      info.type = ControlType::List; break;
        case UIA_ListItemControlTypeId:  info.type = ControlType::ListItem; break;
        case UIA_MenuControlTypeId:      info.type = ControlType::Menu; break;
        case UIA_MenuItemControlTypeId:  info.type = ControlType::MenuItem; break;
        case UIA_TabControlTypeId:       info.type = ControlType::Tab; break;
        case UIA_TabItemControlTypeId:   info.type = ControlType::TabItem; break;
        case UIA_TreeControlTypeId:      info.type = ControlType::Tree; break;
        case UIA_TreeItemControlTypeId:  info.type = ControlType::TreeItem; break;
        case UIA_TableControlTypeId:     info.type = ControlType::Table; break;
        case UIA_ScrollBarControlTypeId: info.type = ControlType::ScrollBar; break;
        case UIA_SliderControlTypeId:    info.type = ControlType::Slider; break;
        case UIA_ProgressBarControlTypeId: info.type = ControlType::ProgressBar; break;
        case UIA_ToolBarControlTypeId:   info.type = ControlType::ToolBar; break;
        case UIA_StatusBarControlTypeId: info.type = ControlType::StatusBar; break;
        case UIA_GroupControlTypeId:     info.type = ControlType::Group; break;
        case UIA_ImageControlTypeId:     info.type = ControlType::Image; break;
        case UIA_HyperlinkControlTypeId: info.type = ControlType::Link; break;
        case UIA_ToolTipControlTypeId:   info.type = ControlType::Tooltip; break;
        case UIA_WindowControlTypeId:    info.type = ControlType::Window; break;
        default:                         info.type = ControlType::Unknown; break;
        }
    }

    BSTR name = nullptr;
    hr = pElement->get_CurrentName(&name);
    if (SUCCEEDED(hr) && name) {
        info.name = QString::fromWCharArray(name);
        SysFreeString(name);
    }

    BSTR className = nullptr;
    hr = pElement->get_CurrentClassName(&className);
    if (SUCCEEDED(hr) && className) {
        info.className = QString::fromWCharArray(className);
        SysFreeString(className);
    }

    pElement->Release();
    return info;
}

static void collectControlsRecursive(IUIAutomationTreeWalker* pWalker,
                                      IUIAutomationElement* pParent,
                                      std::vector<ControlInfo>& result,
                                      int depth)
{
    if (depth > 8) return;  // Limit recursion depth

    IUIAutomationElement* pChild = nullptr;
    pWalker->GetFirstChildElement(pParent, &pChild);
    while (pChild) {
        ControlInfo info;

        RECT boundingRect;
        if (SUCCEEDED(pChild->get_CurrentBoundingRectangle(&boundingRect))) {
            info.rect = QRect(boundingRect.left, boundingRect.top,
                              boundingRect.right - boundingRect.left,
                              boundingRect.bottom - boundingRect.top);
        }

        BSTR name = nullptr;
        if (SUCCEEDED(pChild->get_CurrentName(&name)) && name) {
            info.name = QString::fromWCharArray(name);
            SysFreeString(name);
        }

        CONTROLTYPEID controlType;
        if (SUCCEEDED(pChild->get_CurrentControlType(&controlType))) {
            switch (controlType) {
            case UIA_ButtonControlTypeId:    info.type = ControlType::Button; break;
            case UIA_CheckBoxControlTypeId:  info.type = ControlType::CheckBox; break;
            case UIA_RadioButtonControlTypeId: info.type = ControlType::RadioButton; break;
            case UIA_ComboBoxControlTypeId:  info.type = ControlType::ComboBox; break;
            case UIA_EditControlTypeId:      info.type = ControlType::Edit; break;
            case UIA_TextControlTypeId:      info.type = ControlType::Label; break;
            case UIA_ListControlTypeId:      info.type = ControlType::List; break;
            case UIA_ListItemControlTypeId:  info.type = ControlType::ListItem; break;
            case UIA_MenuControlTypeId:      info.type = ControlType::Menu; break;
            case UIA_MenuItemControlTypeId:  info.type = ControlType::MenuItem; break;
            case UIA_TabControlTypeId:       info.type = ControlType::Tab; break;
            case UIA_TabItemControlTypeId:   info.type = ControlType::TabItem; break;
            case UIA_TreeControlTypeId:      info.type = ControlType::Tree; break;
            case UIA_TreeItemControlTypeId:  info.type = ControlType::TreeItem; break;
            case UIA_ToolBarControlTypeId:   info.type = ControlType::ToolBar; break;
            case UIA_StatusBarControlTypeId: info.type = ControlType::StatusBar; break;
            case UIA_GroupControlTypeId:     info.type = ControlType::Group; break;
            default:                         info.type = ControlType::Unknown; break;
            }
        }

        if (info.rect.width() > 0 && info.rect.height() > 0) {
            result.push_back(info);
        }

        // Recurse into children
        collectControlsRecursive(pWalker, pChild, result, depth + 1);

        IUIAutomationElement* pNext = nullptr;
        pWalker->GetNextSiblingElement(pChild, &pNext);
        pChild->Release();
        pChild = pNext;
    }
}

std::vector<ControlInfo> WinPlatformApi::getWindowControls(NativeWindowHandle window)
{
    std::vector<ControlInfo> result;
    if (!m_uiAutomation) return result;

    HWND hwnd = reinterpret_cast<HWND>(window);
    IUIAutomationElement* pWindowElement = nullptr;
    HRESULT hr = m_uiAutomation->ElementFromHandle(hwnd, &pWindowElement);
    if (FAILED(hr) || !pWindowElement) return result;

    IUIAutomationTreeWalker* pWalker = nullptr;
    hr = m_uiAutomation->get_ControlViewWalker(&pWalker);
    if (FAILED(hr) || !pWalker) {
        pWindowElement->Release();
        return result;
    }

    collectControlsRecursive(pWalker, pWindowElement, result, 0);

    // Convert all control rects from physical to logical coordinates
    for (auto& ctrl : result) {
        ctrl.rect = physicalToLogical(ctrl.rect);
    }

    pWalker->Release();
    pWindowElement->Release();
    return result;
}

int WinPlatformApi::registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                                    std::function<void()> callback)
{
    UINT nativeMod = 0;
    if (modifiers & Qt::AltModifier)     nativeMod |= MOD_ALT;
    if (modifiers & Qt::ControlModifier) nativeMod |= MOD_CONTROL;
    if (modifiers & Qt::ShiftModifier)   nativeMod |= MOD_SHIFT;
    nativeMod |= MOD_NOREPEAT;

    UINT nativeKey = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        nativeKey = 'A' + (key - Qt::Key_A);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        nativeKey = VK_F1 + (key - Qt::Key_F1);
    } else if (key == Qt::Key_Print) {
        nativeKey = VK_SNAPSHOT;
    }

    int id = m_nextHotkeyId++;
    if (!RegisterHotKey(nullptr, id, nativeMod, nativeKey)) {
        return -1;
    }

    m_hotkeyCallbacks[id] = std::move(callback);
    return id;
}

void WinPlatformApi::unregisterHotkey(int hotkeyId)
{
    UnregisterHotKey(nullptr, hotkeyId);
    m_hotkeyCallbacks.erase(hotkeyId);
}

std::vector<ControlInfo> PlatformApi::getWindowControlsAsync(NativeWindowHandle window)
{
    std::vector<ControlInfo> result;

    // Initialize COM for this thread (MTA is fine for background threads)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IUIAutomation* pAutomation = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                  reinterpret_cast<void**>(&pAutomation));
    if (SUCCEEDED(hr) && pAutomation) {
        HWND hwnd = reinterpret_cast<HWND>(window);
        IUIAutomationElement* pWindowElement = nullptr;
        hr = pAutomation->ElementFromHandle(hwnd, &pWindowElement);
        if (SUCCEEDED(hr) && pWindowElement) {
            IUIAutomationTreeWalker* pWalker = nullptr;
            hr = pAutomation->get_ControlViewWalker(&pWalker);
            if (SUCCEEDED(hr) && pWalker) {
                collectControlsRecursive(pWalker, pWindowElement, result, 0);
                pWalker->Release();
            }
            pWindowElement->Release();
        }
        pAutomation->Release();
    }

    // Convert physical to logical coordinates using per-monitor DPI
    for (auto& ctrl : result) {
        HMONITOR hMon = MonitorFromPoint(
            {ctrl.rect.left(), ctrl.rect.top()}, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96, dpiY = 96;
        // GetDpiForMonitor requires shcore.dll (Win 8.1+)
        typedef HRESULT(WINAPI* GetDpiFunc)(HMONITOR, int, UINT*, UINT*);
        static auto getDpi = reinterpret_cast<GetDpiFunc>(
            GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor"));
        if (getDpi) getDpi(hMon, 0 /*MDT_EFFECTIVE_DPI*/, &dpiX, &dpiY);
        qreal dpr = dpiX / 96.0;
        if (dpr > 1.0) {
            ctrl.rect = QRect(
                qRound(ctrl.rect.x() / dpr), qRound(ctrl.rect.y() / dpr),
                qRound(ctrl.rect.width() / dpr), qRound(ctrl.rect.height() / dpr));
        }
    }

    CoUninitialize();
    return result;
}

std::unique_ptr<PlatformApi> PlatformApi::create()
{
    return std::make_unique<WinPlatformApi>();
}

} // namespace simpleshotter

#endif // _WIN32
