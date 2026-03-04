#include "win_platform_api.h"

#ifdef _WIN32

#include <dwmapi.h>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QScreen>
#include <QWindow>
#include <QImage>

namespace easyshotter {

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

    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;

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

QPixmap WinPlatformApi::captureScreen()
{
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, hBitmap));

    BitBlt(memDC, 0, 0, w, h, screenDC, x, y, SRCCOPY);

    SelectObject(memDC, oldBitmap);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage image(w, h, QImage::Format_ARGB32);
    GetDIBits(memDC, hBitmap, 0, h, image.bits(),
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    return QPixmap::fromImage(image);
}

QPixmap WinPlatformApi::captureRegion(const QRect& region)
{
    QPixmap full = captureScreen();
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    QRect adjusted = region.translated(-vx, -vy);
    return full.copy(adjusted);
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
        info.rect = getWindowRect(hwnd);
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

    IUIAutomationElement* pElement = nullptr;
    POINT pt = {screenPos.x(), screenPos.y()};
    HRESULT hr = m_uiAutomation->ElementFromPoint(pt, &pElement);
    if (FAILED(hr) || !pElement) return info;

    RECT boundingRect;
    hr = pElement->get_CurrentBoundingRectangle(&boundingRect);
    if (SUCCEEDED(hr)) {
        info.rect = QRect(boundingRect.left, boundingRect.top,
                          boundingRect.right - boundingRect.left,
                          boundingRect.bottom - boundingRect.top);
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

    IUIAutomationElement* pChild = nullptr;
    pWalker->GetFirstChildElement(pWindowElement, &pChild);
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
            if (controlType == UIA_ButtonControlTypeId) info.type = ControlType::Button;
            else if (controlType == UIA_EditControlTypeId) info.type = ControlType::Edit;
            else if (controlType == UIA_WindowControlTypeId) info.type = ControlType::Window;
        }

        if (info.rect.width() > 0 && info.rect.height() > 0) {
            result.push_back(info);
        }

        IUIAutomationElement* pNext = nullptr;
        pWalker->GetNextSiblingElement(pChild, &pNext);
        pChild->Release();
        pChild = pNext;
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

std::unique_ptr<PlatformApi> PlatformApi::create()
{
    return std::make_unique<WinPlatformApi>();
}

} // namespace easyshotter

#endif // _WIN32
