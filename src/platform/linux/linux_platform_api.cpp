#include "linux_platform_api.h"

#if defined(__linux__)

#include <QGuiApplication>
#include <QScreen>
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QPainter>
#include <QX11Info>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <atspi/atspi.h>

#include <cstring>
#include <algorithm>

namespace simpleshotter {

// ---- Hotkey native event filter ----

class LinuxHotkeyFilter : public QAbstractNativeEventFilter {
public:
    std::unordered_map<int, LinuxPlatformApi::HotkeyBinding>* hotkeys = nullptr;

    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override
    {
        Q_UNUSED(result)
        if (eventType != "xcb_generic_event_t" || !hotkeys) return false;

        auto* event = static_cast<xcb_generic_event_t*>(message);
        uint8_t responseType = event->response_type & ~0x80;

        if (responseType == XCB_KEY_PRESS) {
            auto* keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
            // Strip lock modifiers (NumLock=Mod2, CapsLock=Lock, ScrollLock=Mod5)
            unsigned int cleanState = keyEvent->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2 | XCB_MOD_MASK_5);

            for (auto& [id, binding] : *hotkeys) {
                if (keyEvent->detail == binding.keycode &&
                    cleanState == binding.modifiers) {
                    binding.callback();
                    return true;
                }
            }
        }
        return false;
    }
};

// ---- Constructor / Destructor ----

LinuxPlatformApi::LinuxPlatformApi()
{
    // Use Qt's X11 connection so XGrabKey events arrive through Qt's event loop
    m_display = QX11Info::display();
    m_xcb = QX11Info::connection();
    if (m_display) {
        m_defaultScreen = DefaultScreen(m_display);
        m_rootWindow = static_cast<uint32_t>(DefaultRootWindow(m_display));
    }

    m_hotkeyFilter = new LinuxHotkeyFilter();
    m_hotkeyFilter->hotkeys = &m_hotkeys;
    QCoreApplication::instance()->installNativeEventFilter(m_hotkeyFilter);

    initAtSpi();
}

LinuxPlatformApi::~LinuxPlatformApi()
{
    QCoreApplication::instance()->removeNativeEventFilter(m_hotkeyFilter);

    // Ungrab all hotkeys
    if (m_display) {
        for (auto& [id, binding] : m_hotkeys) {
            XUngrabKey(m_display, binding.keycode, binding.modifiers,
                       DefaultRootWindow(m_display));
            // Also ungrab lock modifier variants
            XUngrabKey(m_display, binding.keycode, binding.modifiers | LockMask,
                       DefaultRootWindow(m_display));
            XUngrabKey(m_display, binding.keycode, binding.modifiers | Mod2Mask,
                       DefaultRootWindow(m_display));
            XUngrabKey(m_display, binding.keycode, binding.modifiers | LockMask | Mod2Mask,
                       DefaultRootWindow(m_display));
        }
        XFlush(m_display);
    }

    delete m_hotkeyFilter;

    // Don't close the display - it's owned by Qt
    m_display = nullptr;
    m_xcb = nullptr;
}

// ---- AT-SPI initialization ----

void LinuxPlatformApi::initAtSpi()
{
    int ret = atspi_init();
    m_atspiInited = (ret == 0 || ret == 1); // 0=first init, 1=already inited
}

// ---- XCB helpers ----

uint32_t LinuxPlatformApi::xcbAtom(const char* name)
{
    auto it = m_atomCache.find(name);
    if (it != m_atomCache.end()) return it->second;

    if (!m_xcb) return 0;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(m_xcb, 0, strlen(name), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(m_xcb, cookie, nullptr);
    uint32_t atom = reply ? reply->atom : 0;
    free(reply);

    m_atomCache[name] = atom;
    return atom;
}

QString LinuxPlatformApi::getWindowTitle(uint32_t window)
{
    if (!m_xcb) return {};

    // Try _NET_WM_NAME first (UTF-8)
    uint32_t netWmName = xcbAtom("_NET_WM_NAME");
    uint32_t utf8String = xcbAtom("UTF8_STRING");

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcb, 0, window, netWmName, utf8String, 0, 256);
    xcb_get_property_reply_t* reply = xcb_get_property_reply(m_xcb, cookie, nullptr);

    if (reply && xcb_get_property_value_length(reply) > 0) {
        QString title = QString::fromUtf8(
            static_cast<const char*>(xcb_get_property_value(reply)),
            xcb_get_property_value_length(reply));
        free(reply);
        return title;
    }
    free(reply);

    // Fallback to WM_NAME
    xcb_get_property_cookie_t cookie2 = xcb_get_property(
        m_xcb, 0, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 256);
    reply = xcb_get_property_reply(m_xcb, cookie2, nullptr);

    if (reply && xcb_get_property_value_length(reply) > 0) {
        QString title = QString::fromLatin1(
            static_cast<const char*>(xcb_get_property_value(reply)),
            xcb_get_property_value_length(reply));
        free(reply);
        return title;
    }
    free(reply);

    return {};
}

QRect LinuxPlatformApi::getWindowGeometry(uint32_t window)
{
    if (!m_xcb) return {};

    // Get frame extents from _NET_FRAME_EXTENTS
    uint32_t frameExtentsAtom = xcbAtom("_NET_FRAME_EXTENTS");
    xcb_get_property_cookie_t feCookie = xcb_get_property(
        m_xcb, 0, window, frameExtentsAtom, XCB_ATOM_CARDINAL, 0, 4);
    xcb_get_property_reply_t* feReply = xcb_get_property_reply(m_xcb, feCookie, nullptr);

    int frameLeft = 0, frameRight = 0, frameTop = 0, frameBottom = 0;
    if (feReply && xcb_get_property_value_length(feReply) == 16) {
        auto* extents = static_cast<uint32_t*>(xcb_get_property_value(feReply));
        frameLeft = extents[0];
        frameRight = extents[1];
        frameTop = extents[2];
        frameBottom = extents[3];
    }
    free(feReply);

    xcb_get_geometry_cookie_t geoCookie = xcb_get_geometry(m_xcb, window);
    xcb_get_geometry_reply_t* geoReply = xcb_get_geometry_reply(m_xcb, geoCookie, nullptr);
    if (!geoReply) return {};

    // Translate to root window coordinates
    xcb_translate_coordinates_cookie_t trCookie = xcb_translate_coordinates(
        m_xcb, window, m_rootWindow, 0, 0);
    xcb_translate_coordinates_reply_t* trReply = xcb_translate_coordinates_reply(
        m_xcb, trCookie, nullptr);

    QRect result;
    if (trReply) {
        int x = trReply->dst_x - frameLeft;
        int y = trReply->dst_y - frameTop;
        int w = geoReply->width + frameLeft + frameRight;
        int h = geoReply->height + frameTop + frameBottom;
        result = QRect(x, y, w, h);
        free(trReply);
    }

    free(geoReply);
    return result;
}

bool LinuxPlatformApi::isWindowMapped(uint32_t window)
{
    if (!m_xcb) return false;

    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(m_xcb, window);
    xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply(m_xcb, cookie, nullptr);
    if (!reply) return false;

    bool mapped = (reply->map_state == XCB_MAP_STATE_VIEWABLE);
    free(reply);
    return mapped;
}

uint32_t LinuxPlatformApi::getWindowPid(uint32_t window)
{
    if (!m_xcb) return 0;

    uint32_t pidAtom = xcbAtom("_NET_WM_PID");
    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcb, 0, window, pidAtom, XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t* reply = xcb_get_property_reply(m_xcb, cookie, nullptr);

    uint32_t pid = 0;
    if (reply && xcb_get_property_value_length(reply) == 4) {
        pid = *static_cast<uint32_t*>(xcb_get_property_value(reply));
    }
    free(reply);
    return pid;
}

// ---- Screen capture ----

QPixmap LinuxPlatformApi::captureScreen()
{
    // Capture the entire virtual desktop across all screens
    QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) return {};

    if (screens.size() == 1) {
        return screens.first()->grabWindow(0);
    }

    // Multi-monitor: compute virtual geometry and composite
    QRect virtualGeometry;
    for (QScreen* screen : screens) {
        virtualGeometry = virtualGeometry.united(screen->geometry());
    }

    QPixmap composite(virtualGeometry.size());
    composite.fill(Qt::black);
    QPainter painter(&composite);

    for (QScreen* screen : screens) {
        QPixmap screenPixmap = screen->grabWindow(0);
        QPoint offset = screen->geometry().topLeft() - virtualGeometry.topLeft();
        painter.drawPixmap(offset, screenPixmap);
    }

    return composite;
}

QPixmap LinuxPlatformApi::captureRegion(const QRect& region)
{
    QPixmap full = captureScreen();
    // Adjust for virtual desktop offset
    QRect virtualGeometry;
    for (QScreen* screen : QGuiApplication::screens()) {
        virtualGeometry = virtualGeometry.united(screen->geometry());
    }
    QRect adjusted = region.translated(-virtualGeometry.topLeft());
    return full.copy(adjusted);
}

// ---- Window enumeration ----

std::vector<WindowInfo> LinuxPlatformApi::getVisibleWindows()
{
    std::vector<WindowInfo> result;
    if (!m_xcb) return result;

    // Use _NET_CLIENT_LIST_STACKING for proper Z-order
    uint32_t clientListAtom = xcbAtom("_NET_CLIENT_LIST_STACKING");
    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_xcb, 0, m_rootWindow, clientListAtom, XCB_ATOM_WINDOW, 0, 1024);
    xcb_get_property_reply_t* reply = xcb_get_property_reply(m_xcb, cookie, nullptr);

    if (!reply || xcb_get_property_value_length(reply) == 0) {
        free(reply);
        // Fallback to _NET_CLIENT_LIST
        clientListAtom = xcbAtom("_NET_CLIENT_LIST");
        cookie = xcb_get_property(m_xcb, 0, m_rootWindow, clientListAtom,
                                  XCB_ATOM_WINDOW, 0, 1024);
        reply = xcb_get_property_reply(m_xcb, cookie, nullptr);
    }

    if (!reply) return result;

    int count = xcb_get_property_value_length(reply) / sizeof(uint32_t);
    auto* windows = static_cast<uint32_t*>(xcb_get_property_value(reply));

    uint32_t wmWindowType = xcbAtom("_NET_WM_WINDOW_TYPE");
    uint32_t wmTypeNormal = xcbAtom("_NET_WM_WINDOW_TYPE_NORMAL");
    uint32_t wmTypeDialog = xcbAtom("_NET_WM_WINDOW_TYPE_DIALOG");
    uint32_t wmTypeUtility = xcbAtom("_NET_WM_WINDOW_TYPE_UTILITY");
    uint32_t wmState = xcbAtom("_NET_WM_STATE");
    uint32_t wmStateHidden = xcbAtom("_NET_WM_STATE_HIDDEN");

    // Iterate in reverse (topmost first)
    for (int i = count - 1; i >= 0; i--) {
        uint32_t win = windows[i];

        if (!isWindowMapped(win)) continue;

        // Check window type - skip dock, desktop, etc.
        xcb_get_property_cookie_t typeCookie = xcb_get_property(
            m_xcb, 0, win, wmWindowType, XCB_ATOM_ATOM, 0, 16);
        xcb_get_property_reply_t* typeReply = xcb_get_property_reply(m_xcb, typeCookie, nullptr);
        bool validType = true;
        if (typeReply && xcb_get_property_value_length(typeReply) > 0) {
            int typeCount = xcb_get_property_value_length(typeReply) / sizeof(uint32_t);
            auto* types = static_cast<uint32_t*>(xcb_get_property_value(typeReply));
            validType = false;
            for (int t = 0; t < typeCount; t++) {
                if (types[t] == wmTypeNormal || types[t] == wmTypeDialog || types[t] == wmTypeUtility) {
                    validType = true;
                    break;
                }
            }
        }
        free(typeReply);
        if (!validType) continue;

        // Check window state - skip hidden windows
        xcb_get_property_cookie_t stateCookie = xcb_get_property(
            m_xcb, 0, win, wmState, XCB_ATOM_ATOM, 0, 16);
        xcb_get_property_reply_t* stateReply = xcb_get_property_reply(m_xcb, stateCookie, nullptr);
        bool hidden = false;
        if (stateReply && xcb_get_property_value_length(stateReply) > 0) {
            int stateCount = xcb_get_property_value_length(stateReply) / sizeof(uint32_t);
            auto* states = static_cast<uint32_t*>(xcb_get_property_value(stateReply));
            for (int s = 0; s < stateCount; s++) {
                if (states[s] == wmStateHidden) {
                    hidden = true;
                    break;
                }
            }
        }
        free(stateReply);
        if (hidden) continue;

        QRect geom = getWindowGeometry(win);
        if (geom.width() <= 0 || geom.height() <= 0) continue;

        WindowInfo info;
        info.handle = static_cast<quintptr>(win);
        info.title = getWindowTitle(win);
        info.rect = geom;
        info.zOrder = static_cast<int>(result.size());
        info.isTopLevel = true;

        result.push_back(info);
    }

    free(reply);
    return result;
}

WindowInfo LinuxPlatformApi::windowAtPoint(const QPoint& screenPos)
{
    auto windows = getVisibleWindows();
    for (const auto& win : windows) {
        if (win.rect.contains(screenPos)) {
            return win;
        }
    }
    return {};
}

// ---- AT-SPI control detection ----

ControlType LinuxPlatformApi::mapAtspiRole(int role)
{
    switch (role) {
    case ATSPI_ROLE_PUSH_BUTTON:
    case ATSPI_ROLE_TOGGLE_BUTTON:     return ControlType::Button;
    case ATSPI_ROLE_CHECK_BOX:         return ControlType::CheckBox;
    case ATSPI_ROLE_RADIO_BUTTON:      return ControlType::RadioButton;
    case ATSPI_ROLE_COMBO_BOX:         return ControlType::ComboBox;
    case ATSPI_ROLE_TEXT:
    case ATSPI_ROLE_ENTRY:
    case ATSPI_ROLE_PASSWORD_TEXT:      return ControlType::Edit;
    case ATSPI_ROLE_LABEL:
    case ATSPI_ROLE_STATIC:            return ControlType::Label;
    case ATSPI_ROLE_LIST:
    case ATSPI_ROLE_LIST_BOX:          return ControlType::List;
    case ATSPI_ROLE_LIST_ITEM:         return ControlType::ListItem;
    case ATSPI_ROLE_MENU:
    case ATSPI_ROLE_MENU_BAR:          return ControlType::Menu;
    case ATSPI_ROLE_MENU_ITEM:
    case ATSPI_ROLE_CHECK_MENU_ITEM:
    case ATSPI_ROLE_RADIO_MENU_ITEM:   return ControlType::MenuItem;
    case ATSPI_ROLE_PAGE_TAB_LIST:     return ControlType::Tab;
    case ATSPI_ROLE_PAGE_TAB:          return ControlType::TabItem;
    case ATSPI_ROLE_TREE:
    case ATSPI_ROLE_TREE_TABLE:        return ControlType::Tree;
    case ATSPI_ROLE_TABLE:             return ControlType::Table;
    case ATSPI_ROLE_TABLE_ROW:         return ControlType::TableRow;
    case ATSPI_ROLE_TABLE_CELL:        return ControlType::TableCell;
    case ATSPI_ROLE_SCROLL_BAR:        return ControlType::ScrollBar;
    case ATSPI_ROLE_SLIDER:            return ControlType::Slider;
    case ATSPI_ROLE_PROGRESS_BAR:      return ControlType::ProgressBar;
    case ATSPI_ROLE_TOOL_BAR:          return ControlType::ToolBar;
    case ATSPI_ROLE_STATUS_BAR:        return ControlType::StatusBar;
    case ATSPI_ROLE_PANEL:
    case ATSPI_ROLE_FILLER:            return ControlType::Group;
    case ATSPI_ROLE_IMAGE:
    case ATSPI_ROLE_ICON:              return ControlType::Image;
    case ATSPI_ROLE_LINK:              return ControlType::Link;
    case ATSPI_ROLE_TOOL_TIP:          return ControlType::Tooltip;
    case ATSPI_ROLE_FRAME:
    case ATSPI_ROLE_DIALOG:
    case ATSPI_ROLE_WINDOW:            return ControlType::Window;
    default:                           return ControlType::Unknown;
    }
}

void LinuxPlatformApi::collectAccessibleControls(
    void* accessible, std::vector<ControlInfo>& result,
    int depth, const QRect& windowRect)
{
    if (depth > 8 || !accessible) return;

    auto* acc = static_cast<AtspiAccessible*>(accessible);
    GError* error = nullptr;

    int childCount = atspi_accessible_get_child_count(acc, &error);
    if (error) {
        g_error_free(error);
        return;
    }

    for (int i = 0; i < childCount; i++) {
        error = nullptr;
        AtspiAccessible* child = atspi_accessible_get_child_at_index(acc, i, &error);
        if (error) {
            g_error_free(error);
            continue;
        }
        if (!child) continue;

        // Get role
        error = nullptr;
        AtspiRole role = atspi_accessible_get_role(child, &error);
        if (error) {
            g_error_free(error);
            g_object_unref(child);
            continue;
        }

        // Get component interface for position/size
        AtspiComponent* comp = atspi_accessible_get_component_iface(child);
        if (comp) {
            error = nullptr;
            AtspiRect* extents = atspi_component_get_extents(
                comp, ATSPI_COORD_TYPE_SCREEN, &error);
            if (extents && !error) {
                QRect ctrlRect(extents->x, extents->y, extents->width, extents->height);

                // Only include controls within the window bounds and with valid size
                if (ctrlRect.width() > 0 && ctrlRect.height() > 0 &&
                    windowRect.intersects(ctrlRect)) {
                    ControlInfo info;
                    info.type = mapAtspiRole(role);
                    info.rect = ctrlRect;

                    // Get name
                    error = nullptr;
                    gchar* name = atspi_accessible_get_name(child, &error);
                    if (name && !error) {
                        info.name = QString::fromUtf8(name);
                        g_free(name);
                    }
                    if (error) g_error_free(error);

                    // Get role name as className
                    error = nullptr;
                    gchar* roleName = atspi_accessible_get_role_name(child, &error);
                    if (roleName && !error) {
                        info.className = QString::fromUtf8(roleName);
                        g_free(roleName);
                    }
                    if (error) g_error_free(error);

                    result.push_back(info);
                }
                g_free(extents);
            }
            if (error) g_error_free(error);
            g_object_unref(comp);
        }

        // Recurse
        collectAccessibleControls(child, result, depth + 1, windowRect);
        g_object_unref(child);
    }
}

ControlInfo LinuxPlatformApi::controlAtPoint(const QPoint& screenPos)
{
    ControlInfo info;
    if (!m_atspiInited) return info;

    // First find which window contains the point, then only search that app's tree
    auto windows = getVisibleWindows();
    uint32_t targetPid = 0;
    QRect targetWindowRect;
    for (const auto& win : windows) {
        if (win.rect.contains(screenPos)) {
            targetPid = getWindowPid(static_cast<uint32_t>(win.handle));
            targetWindowRect = win.rect;
            break;
        }
    }
    if (targetPid == 0) return info;

    AtspiAccessible* desktop = atspi_get_desktop(0);
    if (!desktop) return info;

    GError* error = nullptr;
    int appCount = atspi_accessible_get_child_count(desktop, &error);
    if (error) {
        g_error_free(error);
        g_object_unref(desktop);
        return info;
    }

    ControlInfo bestMatch;
    int bestArea = INT_MAX;

    for (int i = 0; i < appCount; i++) {
        error = nullptr;
        AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, &error);
        if (error || !app) {
            if (error) g_error_free(error);
            continue;
        }

        // Filter by PID to only traverse the target app
        error = nullptr;
        guint pid = atspi_accessible_get_process_id(app, &error);
        if (error) {
            g_error_free(error);
            g_object_unref(app);
            continue;
        }
        if (pid != targetPid) {
            g_object_unref(app);
            continue;
        }

        std::vector<ControlInfo> controls;
        collectAccessibleControls(app, controls, 0, targetWindowRect);

        for (const auto& ctrl : controls) {
            if (ctrl.rect.contains(screenPos)) {
                int area = ctrl.rect.width() * ctrl.rect.height();
                if (area < bestArea && area > 0) {
                    bestArea = area;
                    bestMatch = ctrl;
                }
            }
        }

        g_object_unref(app);
        break;  // Found the target app, no need to continue
    }

    g_object_unref(desktop);
    return bestMatch;
}

std::vector<ControlInfo> LinuxPlatformApi::getWindowControls(NativeWindowHandle window)
{
    std::vector<ControlInfo> result;
    if (!m_atspiInited) return result;

    // Get the window geometry for bounds filtering
    QRect windowRect = getWindowGeometry(static_cast<uint32_t>(window));
    if (!windowRect.isValid()) return result;

    // Get the PID of the target window
    uint32_t targetPid = getWindowPid(static_cast<uint32_t>(window));

    AtspiAccessible* desktop = atspi_get_desktop(0);
    if (!desktop) return result;

    GError* error = nullptr;
    int appCount = atspi_accessible_get_child_count(desktop, &error);
    if (error) {
        g_error_free(error);
        g_object_unref(desktop);
        return result;
    }

    for (int i = 0; i < appCount; i++) {
        error = nullptr;
        AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, &error);
        if (error || !app) {
            if (error) g_error_free(error);
            continue;
        }

        // Match application by PID if available
        if (targetPid > 0) {
            error = nullptr;
            guint pid = atspi_accessible_get_process_id(app, &error);
            if (error) {
                g_error_free(error);
                g_object_unref(app);
                continue;
            }
            if (pid != targetPid) {
                g_object_unref(app);
                continue;
            }
        }

        // Traverse the app tree and collect controls within window bounds
        collectAccessibleControls(app, result, 0, windowRect);
        g_object_unref(app);

        // If we matched by PID, we found the app
        if (targetPid > 0) break;
    }

    g_object_unref(desktop);
    return result;
}

// ---- Hotkey registration ----

int LinuxPlatformApi::registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                                      std::function<void()> callback)
{
    if (!m_display) return -1;

    // Map Qt key to X11 keysym
    KeySym keysym = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        keysym = XK_a + (key - Qt::Key_A);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        keysym = XK_F1 + (key - Qt::Key_F1);
    } else if (key == Qt::Key_Print) {
        keysym = XK_Print;
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keysym = XK_0 + (key - Qt::Key_0);
    }

    if (keysym == 0) return -1;

    KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0) return -1;

    // Map Qt modifiers to X11 modifiers
    unsigned int xmod = 0;
    if (modifiers & Qt::ControlModifier) xmod |= ControlMask;
    if (modifiers & Qt::AltModifier)     xmod |= Mod1Mask;
    if (modifiers & Qt::ShiftModifier)   xmod |= ShiftMask;
    if (modifiers & Qt::MetaModifier)    xmod |= Mod4Mask;

    Window root = DefaultRootWindow(m_display);

    // Grab with and without lock modifiers to handle CapsLock/NumLock
    unsigned int lockMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (unsigned int lockMask : lockMasks) {
        XGrabKey(m_display, keycode, xmod | lockMask,
                 root, False, GrabModeAsync, GrabModeAsync);
    }
    XFlush(m_display);

    int id = m_nextHotkeyId++;
    m_hotkeys[id] = {static_cast<unsigned int>(keycode), xmod, std::move(callback)};
    return id;
}

void LinuxPlatformApi::unregisterHotkey(int hotkeyId)
{
    auto it = m_hotkeys.find(hotkeyId);
    if (it == m_hotkeys.end()) return;

    if (m_display) {
        Window root = DefaultRootWindow(m_display);
        unsigned int lockMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
        for (unsigned int lockMask : lockMasks) {
            XUngrabKey(m_display, it->second.keycode,
                       it->second.modifiers | lockMask, root);
        }
        XFlush(m_display);
    }

    m_hotkeys.erase(it);
}

// ---- Factory ----

std::unique_ptr<PlatformApi> PlatformApi::create()
{
    return std::make_unique<LinuxPlatformApi>();
}

} // namespace simpleshotter

#endif // __linux__
