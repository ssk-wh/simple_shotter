#pragma once

#include <QRect>
#include <QPoint>
#include <QString>
#include <QPixmap>
#include <Qt>
#include <vector>
#include <memory>
#include <functional>

namespace simpleshotter {

using NativeWindowHandle = quintptr;

struct WindowInfo {
    NativeWindowHandle handle = 0;
    QString title;
    QRect rect;
    int zOrder = 0;
    bool isTopLevel = true;
};

enum class ControlType {
    Unknown,
    Window,
    Button,
    CheckBox,
    RadioButton,
    ComboBox,
    Edit,
    Label,
    List,
    ListItem,
    Menu,
    MenuItem,
    Tab,
    TabItem,
    Tree,
    TreeItem,
    Table,
    TableRow,
    TableCell,
    ScrollBar,
    Slider,
    ProgressBar,
    ToolBar,
    StatusBar,
    Group,
    Image,
    Link,
    Tooltip
};

struct ControlInfo {
    ControlType type = ControlType::Unknown;
    QRect rect;
    QString name;
    QString className;
};

class PlatformApi {
public:
    virtual ~PlatformApi() = default;

    virtual QPixmap captureScreen() = 0;
    virtual QPixmap captureRegion(const QRect& region) = 0;

    virtual std::vector<WindowInfo> getVisibleWindows() = 0;
    virtual WindowInfo windowAtPoint(const QPoint& screenPos) = 0;

    virtual ControlInfo controlAtPoint(const QPoint& screenPos) = 0;
    virtual std::vector<ControlInfo> getWindowControls(NativeWindowHandle window) = 0;

    virtual int registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                               std::function<void()> callback) = 0;
    virtual void unregisterHotkey(int hotkeyId) = 0;

    static std::unique_ptr<PlatformApi> create();
};

} // namespace simpleshotter
