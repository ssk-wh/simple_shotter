# SimpleShotter 架构设计文档

## 1. 窗口检测方案

### 1.1 Windows 平台 —— Win32 API

**窗口枚举与层叠顺序**

使用 `EnumWindows` 枚举所有顶层窗口。该函数按 Z-Order 从前到后（从顶层到底层）回调，天然提供了层叠顺序。回调中通过以下过滤条件筛选可见窗口：

```cpp
// 过滤条件
bool isValidWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    if (IsIconic(hwnd)) return false;  // 最小化

    // 排除不可见的 shell 窗口（如 ApplicationFrameHost）
    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;

    // 排除零尺寸窗口
    RECT rect;
    GetWindowRect(hwnd, &rect);
    if (rect.right - rect.left <= 0 || rect.bottom - rect.top <= 0) return false;

    return true;
}
```

**获取窗口矩形**

- `GetWindowRect(hwnd, &rect)` —— 包含边框的完整矩形
- `DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect))` —— DWM 合成后的实际可见矩形（推荐，能正确处理 Win10/11 的隐形边框）

**根据鼠标位置匹配窗口**

由于 `EnumWindows` 按 Z-Order 遍历，遍历时对每个可见窗口检查鼠标坐标是否在其矩形内，第一个命中的就是最上层窗口：

```cpp
// 伪代码
WindowInfo findWindowAtPoint(POINT pt) {
    for (auto& win : windowList) {  // 已按 Z-Order 排序
        if (PtInRect(&win.rect, pt)) {
            return win;
        }
    }
}
```

也可以直接使用 `WindowFromPoint(pt)`，但它会穿透某些特殊窗口（分层窗口等），且不能获取完整的窗口列表用于高亮切换，因此建议自行枚举。

**子窗口处理**

对命中的顶层窗口，使用 `ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED)` 进一步定位子窗口，实现更精确的区域识别。

**DPI 感知**

Windows 10+ 存在 Per-Monitor DPI。程序需在 manifest 中声明 `dpiAwareness=PerMonitorV2`，或调用 `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`。获取窗口矩形后需考虑 DPI 缩放：

```cpp
UINT dpi = GetDpiForWindow(hwnd);
double scale = dpi / 96.0;
```

### 1.2 Linux 平台 —— X11/XCB

**窗口枚举**

使用 XCB（X11 的现代 C 绑定）替代传统 Xlib，性能更好：

```cpp
// 获取根窗口的所有子窗口（按 stacking order）
xcb_query_tree_reply_t* tree = xcb_query_tree_reply(
    conn, xcb_query_tree(conn, screen->root), nullptr);
xcb_window_t* children = xcb_query_tree_children(tree);
int count = xcb_query_tree_children_length(tree);
// children[count-1] 是最上层
```

`xcb_query_tree` 返回的子窗口列表按 stacking order 排列（最后一个是最上层），提供层叠顺序。

**窗口属性过滤**

```cpp
bool isValidWindow(xcb_connection_t* conn, xcb_window_t win) {
    // 检查 map state
    auto attrs = xcb_get_window_attributes_reply(
        conn, xcb_get_window_attributes(conn, win), nullptr);
    if (!attrs || attrs->map_state != XCB_MAP_STATE_VIEWABLE) return false;

    // 检查 _NET_WM_WINDOW_TYPE（排除 dock、desktop 等）
    // 检查 _NET_WM_STATE（排除 _NET_WM_STATE_HIDDEN）

    return true;
}
```

**窗口矩形**

```cpp
xcb_get_geometry_reply_t* geo = xcb_get_geometry_reply(
    conn, xcb_get_geometry(conn, win), nullptr);
// geo->x, geo->y, geo->width, geo->height 是相对于父窗口的坐标
// 需要 xcb_translate_coordinates 转换到屏幕绝对坐标
```

**根据鼠标位置匹配**

同 Windows 方案，从最上层开始遍历，检查鼠标是否在窗口矩形内。

### 1.3 Wayland 说明

Wayland 协议不允许客户端枚举其他窗口或获取全局鼠标位置（安全设计）。截图应用在 Wayland 下需使用：
- `xdg-desktop-portal` 的 Screenshot 接口（仅能获取截图，无法获取窗口列表）
- 或依赖 XWayland 兼容层

当前阶段优先支持 X11。Wayland 支持可作为后续迭代项。

---

## 2. 窗口内控件识别方案

### 2.1 Windows —— UI Automation API

**选择 UIA 而非 MSAA 的原因**：
- MSAA 是旧版 API，对 WPF/UWP/WinUI 应用支持差
- UIA 是 MSAA 的继任者，支持所有现代 UI 框架
- UIA 能识别更丰富的控件类型和属性

**核心流程**：

```cpp
#include <UIAutomation.h>

// 1. 创建 UIA 实例（进程级单例）
IUIAutomation* pAutomation = nullptr;
CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                 IID_IUIAutomation, (void**)&pAutomation);

// 2. 根据鼠标坐标获取控件元素
IUIAutomationElement* pElement = nullptr;
POINT pt = { mouseX, mouseY };
pAutomation->ElementFromPoint(pt, &pElement);

// 3. 获取控件矩形
RECT boundingRect;
pElement->get_CurrentBoundingRectangle(&boundingRect);

// 4. 获取控件类型
CONTROLTYPEID controlType;
pElement->get_CurrentControlType(&controlType);
// UIA_ButtonControlTypeId, UIA_EditControlTypeId, UIA_MenuItemControlTypeId ...

// 5. 获取控件名称
BSTR name;
pElement->get_CurrentName(&name);
```

**性能优化**：
- `ElementFromPoint` 调用开销约 5-30ms（跨进程 COM 调用）
- 在鼠标移动时使用节流（throttle），约 50-100ms 调用一次
- 缓存上一次结果，鼠标未移动到新控件时复用

**遍历子控件**：

对于需要高亮所有控件的场景，使用 TreeWalker 遍历：

```cpp
IUIAutomationTreeWalker* pWalker = nullptr;
pAutomation->get_ControlViewWalker(&pWalker);

// 遍历窗口的所有直接子控件
IUIAutomationElement* pChild = nullptr;
pWalker->GetFirstChildElement(pWindowElement, &pChild);
while (pChild) {
    // 获取矩形、类型等
    IUIAutomationElement* pNext = nullptr;
    pWalker->GetNextSiblingElement(pChild, &pNext);
    pChild->Release();
    pChild = pNext;
}
```

### 2.2 Linux —— AT-SPI2

AT-SPI2 是 Linux 下的无障碍接口，基于 D-Bus，能识别 GTK、Qt 等应用的控件树。

```cpp
#include <atspi/atspi.h>

// 初始化
atspi_init();

// 根据坐标获取控件
AtspiAccessible* element = atspi_get_desktop(0);
// 遍历应用 -> 窗口 -> 控件树
// 或使用 atspi_accessible_get_component_iface() + atspi_component_get_extents()

// 获取控件矩形
AtspiComponent* comp = atspi_accessible_get_component_iface(element);
AtspiRect* rect = atspi_component_get_extents(comp, ATSPI_COORD_TYPE_SCREEN, nullptr);

// 获取控件角色
AtspiRole role = atspi_accessible_get_role(element, nullptr);
// ATSPI_ROLE_PUSH_BUTTON, ATSPI_ROLE_TEXT, ATSPI_ROLE_MENU_ITEM ...
```

**注意事项**：
- AT-SPI2 默认未启用的桌面环境需要设置 `DBUS_SESSION_BUS_ADDRESS`
- 性能比 Windows UIA 略差（D-Bus 通信开销），同样需要节流
- 部分应用（如 Electron/Chrome）的无障碍支持需要用户手动开启

### 2.3 控件识别的降级策略

当 UIA/AT-SPI2 无法获取控件信息时（如某些游戏、自绘界面），降级为：
1. 仅识别窗口级别的矩形
2. 提供手动框选作为兜底

---

## 3. 平台抽象接口设计

### 3.1 数据结构

```cpp
// platform/platform_api.h
#pragma once

#include <QRect>
#include <QString>
#include <QPixmap>
#include <vector>
#include <memory>
#include <cstdint>

namespace simpleshotter {

// 平台窗口句柄的统一类型
using NativeWindowHandle = quintptr;  // HWND on Windows, xcb_window_t on Linux

// 窗口信息
struct WindowInfo {
    NativeWindowHandle handle = 0;
    QString title;
    QRect rect;            // 屏幕绝对坐标
    int zOrder = 0;        // 层叠顺序，0 = 最上层
    bool isTopLevel = true;
};

// 控件类型枚举
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

// 控件信息
struct ControlInfo {
    ControlType type = ControlType::Unknown;
    QRect rect;            // 屏幕绝对坐标
    QString name;          // 控件名称/文本
    QString className;     // 原生类名（调试用）
};

} // namespace simpleshotter
```

### 3.2 平台抽象接口

```cpp
// platform/platform_api.h（续）

namespace simpleshotter {

class PlatformApi {
public:
    virtual ~PlatformApi() = default;

    // ---- 截屏 ----

    // 截取整个虚拟桌面（所有屏幕拼接）
    virtual QPixmap captureScreen() = 0;

    // 截取指定矩形区域
    virtual QPixmap captureRegion(const QRect& region) = 0;

    // ---- 窗口检测 ----

    // 获取所有可见窗口，按 Z-Order 排序（index 0 = 最上层）
    virtual std::vector<WindowInfo> getVisibleWindows() = 0;

    // 根据屏幕坐标获取最上层窗口
    virtual WindowInfo windowAtPoint(const QPoint& screenPos) = 0;

    // ---- 控件识别 ----

    // 获取指定坐标处的控件信息
    virtual ControlInfo controlAtPoint(const QPoint& screenPos) = 0;

    // 获取指定窗口内所有可见控件
    virtual std::vector<ControlInfo> getWindowControls(NativeWindowHandle window) = 0;

    // ---- 全局热键 ----

    // 注册全局热键，触发时调用 callback
    // 返回热键 ID，失败返回 -1
    virtual int registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                               std::function<void()> callback) = 0;

    // 注销热键
    virtual void unregisterHotkey(int hotkeyId) = 0;

    // ---- 工厂方法 ----

    // 创建当前平台的实现实例
    static std::unique_ptr<PlatformApi> create();
};

} // namespace simpleshotter
```

### 3.3 工厂方法实现

```cpp
// platform/platform_api.cpp
#include "platform_api.h"

#ifdef Q_OS_WIN
#include "win/win_platform_api.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_platform_api.h"
#endif

namespace simpleshotter {

std::unique_ptr<PlatformApi> PlatformApi::create() {
#ifdef Q_OS_WIN
    return std::make_unique<WinPlatformApi>();
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxPlatformApi>();
#else
    static_assert(false, "Unsupported platform");
#endif
}

} // namespace simpleshotter
```

### 3.4 Windows 实现类声明

```cpp
// platform/win/win_platform_api.h
#pragma once
#include "../platform_api.h"
#include <Windows.h>
#include <UIAutomation.h>

namespace simpleshotter {

class WinPlatformApi : public PlatformApi {
public:
    WinPlatformApi();
    ~WinPlatformApi() override;

    QPixmap captureScreen() override;
    QPixmap captureRegion(const QRect& region) override;
    std::vector<WindowInfo> getVisibleWindows() override;
    WindowInfo windowAtPoint(const QPoint& screenPos) override;
    ControlInfo controlAtPoint(const QPoint& screenPos) override;
    std::vector<ControlInfo> getWindowControls(NativeWindowHandle window) override;
    int registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers,
                       std::function<void()> callback) override;
    void unregisterHotkey(int hotkeyId) override;

private:
    IUIAutomation* m_uiAutomation = nullptr;
    // hotkey 管理
    int m_nextHotkeyId = 1;
    std::unordered_map<int, std::function<void()>> m_hotkeyCallbacks;
};

} // namespace simpleshotter
```

---

## 4. 整体架构

### 4.1 模块依赖关系

```
                    ┌──────────────┐
                    │  main.cpp    │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │  MainWindow  │  系统托盘、热键管理
                    └──────┬───────┘
                           │ 截图触发
                    ┌──────▼───────────────┐
                    │   CaptureOverlay     │  全屏透明覆盖层
                    │   ├─ RegionSelector  │  矩形选择/拖拽
                    │   ├─ WindowDetector  │  窗口匹配高亮
                    │   └─ ControlDetector │  控件匹配高亮
                    └──────┬───────────────┘
                           │ 确认截图
                    ┌──────▼───────────────┐
                    │   PreviewWidget      │  截图预览
                    │   ├─ ToolbarWidget   │  操作工具栏
                    │   └─ AnnotationLayer │  标注层（P1）
                    └──────┬───────────────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ClipboardMgr   FileSaver   PinWindow（P1）
```

### 4.2 截图流程

```
用户按快捷键
    │
    ▼
MainWindow::onHotkeyTriggered()
    │
    ├─ PlatformApi::captureScreen()          // 截取全屏
    ├─ PlatformApi::getVisibleWindows()      // 获取窗口列表
    │
    ▼
CaptureOverlay::show(screenPixmap, windowList)
    │
    ├─ 鼠标移动 → 匹配窗口/控件 → 高亮矩形
    ├─ 鼠标拖拽 → 自由选区
    ├─ 右键 → 取消
    │
    ▼ 双击/回车确认
RegionSelector::confirmed(QRect)
    │
    ├─ 裁剪截图
    ▼
PreviewWidget::show(croppedPixmap)
    │
    ├─ ToolbarWidget: 复制 / 保存 / 钉住 / 标注
    ▼
ClipboardManager::copy(pixmap) 或 FileSaver::save(pixmap, path)
```

### 4.3 核心类职责

| 类 | 职责 |
|---|---|
| `MainWindow` | 系统托盘图标、右键菜单、热键注册、截图触发入口 |
| `CaptureOverlay` | 全屏透明窗口，显示截屏背景图，处理鼠标交互 |
| `RegionSelector` | 管理选区矩形的创建、拖拽、调整大小 |
| `WindowDetector` | 持有窗口列表，根据鼠标位置返回匹配窗口 |
| `ControlDetector` | 调用 UIA/AT-SPI2 获取控件矩形 |
| `ScreenCapture` | 截屏逻辑封装（委托给 PlatformApi） |
| `PreviewWidget` | 截图结果预览、工具栏容器 |
| `ToolbarWidget` | 复制、保存、关闭等操作按钮 |
| `ClipboardManager` | 剪贴板读写 |
| `FileSaver` | 文件保存对话框、格式选择 |

### 4.4 WindowDetector 与 ControlDetector 的交互

```cpp
// core/window_detector.h
#pragma once
#include "../platform/platform_api.h"

namespace simpleshotter {

class WindowDetector {
public:
    explicit WindowDetector(PlatformApi* api);

    // 截图开始时调用，缓存窗口列表快照
    void refresh();

    // 根据鼠标位置查找窗口，O(n) 遍历但 n 通常很小（< 50）
    WindowInfo findWindowAt(const QPoint& screenPos) const;

    // 获取所有窗口用于绘制
    const std::vector<WindowInfo>& windows() const;

private:
    PlatformApi* m_api;
    std::vector<WindowInfo> m_windows;
};

} // namespace simpleshotter
```

```cpp
// core/control_detector.h
#pragma once
#include "../platform/platform_api.h"

namespace simpleshotter {

class ControlDetector {
public:
    explicit ControlDetector(PlatformApi* api);

    // 根据鼠标位置获取控件（带节流）
    ControlInfo findControlAt(const QPoint& screenPos);

private:
    PlatformApi* m_api;
    ControlInfo m_lastResult;
    QPoint m_lastPos;
};

} // namespace simpleshotter
```

**自动识别策略**：
1. 鼠标进入 CaptureOverlay 后，首先在窗口列表中查找匹配窗口
2. 找到窗口后，异步调用 `controlAtPoint` 检查是否命中子控件
3. 如果命中控件，高亮控件矩形；否则高亮整个窗口矩形
4. 用户可以按 Tab 键在"窗口级"和"控件级"之间切换

---

## 5. 标注层架构（P1 预留）

### 5.1 设计思路

标注层采用 **Tool + Item** 模式：
- `AnnotationTool`：定义用户交互行为（按下、拖拽、释放）
- `AnnotationItem`：定义绘制内容和属性

这与 Qt 的 `QGraphicsScene` 理念类似，但我们不使用 `QGraphicsView`（因为 CaptureOverlay 已经是自绘窗口），而是自行管理绘制栈。

### 5.2 类设计

```cpp
// annotation/annotation_item.h
#pragma once
#include <QPainter>
#include <QRect>

namespace simpleshotter {

class AnnotationItem {
public:
    virtual ~AnnotationItem() = default;
    virtual void paint(QPainter& painter) = 0;
    virtual QRect boundingRect() const = 0;
    virtual AnnotationItem* clone() const = 0;  // 用于撤销/重做
};

// 具体 Item 子类
class RectangleItem : public AnnotationItem { ... };
class ArrowItem : public AnnotationItem { ... };
class TextItem : public AnnotationItem { ... };
class PenStrokeItem : public AnnotationItem { ... };   // 自由画笔
class MosaicItem : public AnnotationItem { ... };      // 马赛克/模糊

} // namespace simpleshotter
```

```cpp
// annotation/annotation_tool.h
#pragma once
#include <QPoint>
#include <QMouseEvent>

namespace simpleshotter {

class AnnotationLayer;  // forward

class AnnotationTool {
public:
    virtual ~AnnotationTool() = default;

    virtual void onMousePress(const QPoint& pos, Qt::MouseButton button) = 0;
    virtual void onMouseMove(const QPoint& pos) = 0;
    virtual void onMouseRelease(const QPoint& pos, Qt::MouseButton button) = 0;
    virtual void onKeyPress(int key) {}

    void setLayer(AnnotationLayer* layer) { m_layer = layer; }

protected:
    AnnotationLayer* m_layer = nullptr;
};

// 具体 Tool 子类
class RectangleTool : public AnnotationTool { ... };
class ArrowTool : public AnnotationTool { ... };
class TextTool : public AnnotationTool { ... };
class PenTool : public AnnotationTool { ... };
class MosaicTool : public AnnotationTool { ... };

} // namespace simpleshotter
```

```cpp
// annotation/annotation_layer.h
#pragma once
#include "annotation_item.h"
#include "annotation_tool.h"
#include <vector>
#include <memory>
#include <stack>

namespace simpleshotter {

class AnnotationLayer {
public:
    // 绘制所有标注
    void paint(QPainter& painter);

    // 添加标注项
    void addItem(std::unique_ptr<AnnotationItem> item);

    // 设置当前工具
    void setTool(std::unique_ptr<AnnotationTool> tool);

    // 撤销/重做
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // 事件转发
    void mousePressEvent(const QPoint& pos, Qt::MouseButton button);
    void mouseMoveEvent(const QPoint& pos);
    void mouseReleaseEvent(const QPoint& pos, Qt::MouseButton button);

private:
    std::vector<std::unique_ptr<AnnotationItem>> m_items;
    std::unique_ptr<AnnotationTool> m_currentTool;

    // 撤销/重做栈（Command Pattern）
    struct UndoState {
        std::vector<std::unique_ptr<AnnotationItem>> items;
    };
    std::vector<UndoState> m_undoStack;
    int m_undoIndex = -1;  // 当前在 undoStack 中的位置
};

} // namespace simpleshotter
```

### 5.3 撤销/重做策略

采用 **快照式** 而非命令式：每次操作完成后，保存整个 items 列表的克隆。虽然内存开销稍大，但实现简单可靠，且标注项数量通常很少（< 100）。

```
undoStack:  [S0] [S1] [S2] [S3]
                            ↑ undoIndex=3
执行 undo:
undoStack:  [S0] [S1] [S2] [S3]
                       ↑ undoIndex=2, 恢复 S2

执行新操作:
undoStack:  [S0] [S1] [S2] [S4]   // S3 被丢弃
                            ↑ undoIndex=3
```

---

## 6. 截图钉在桌面（P1 预留）

### 6.1 PinWindow 设计

```cpp
// ui/pin_window.h
#pragma once
#include <QWidget>

namespace simpleshotter {

class PinWindow : public QWidget {
    Q_OBJECT
public:
    explicit PinWindow(const QPixmap& pixmap, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QPixmap m_pixmap;
    QPoint m_dragStartPos;
    double m_scale = 1.0;
};

} // namespace simpleshotter
```

**关键特性**：
- 窗口属性：`Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool`
- `Qt::Tool` 防止在任务栏显示
- 支持拖拽移动（鼠标左键按住拖动）
- 支持滚轮缩放
- 右键菜单：复制、保存、关闭、透明度调节
- 双击关闭
- 可以同时存在多个 PinWindow 实例，由 MainWindow 持有列表管理生命周期

---

## 7. CaptureOverlay 详细设计

### 7.1 全屏覆盖层

CaptureOverlay 是截图交互的核心 UI 组件：

```cpp
// ui/capture_overlay.h
#pragma once
#include <QWidget>
#include "../core/window_detector.h"
#include "../core/control_detector.h"
#include "../core/region_selector.h"

namespace simpleshotter {

class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CaptureOverlay(PlatformApi* api, QWidget* parent = nullptr);

    // 开始截图
    void startCapture();

signals:
    void captureConfirmed(const QPixmap& pixmap, const QRect& region);
    void captureCancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class State {
        Idle,           // 等待操作
        AutoDetect,     // 自动检测窗口/控件
        Selecting,      // 手动框选中
        Selected,       // 已选定区域
        Moving,         // 移动选区
        Resizing        // 调整选区大小
    };

    PlatformApi* m_api;
    QPixmap m_screenPixmap;          // 全屏截图
    State m_state = State::Idle;

    WindowDetector m_windowDetector;
    ControlDetector m_controlDetector;
    RegionSelector m_regionSelector;

    QRect m_highlightRect;           // 当前高亮矩形
    QRect m_selectedRect;            // 已确认的选区
    bool m_detectControls = true;    // 是否检测控件级别
};

} // namespace simpleshotter
```

### 7.2 绘制策略

```
paintEvent 绘制顺序：
1. 全屏截图作为背景
2. 半透明黑色遮罩覆盖全屏
3. 选区/高亮矩形内"打洞"（用 CompositionMode 恢复原始亮度）
4. 选区/高亮矩形边框（蓝色 2px）
5. 选区的 8 个控制点（用于调整大小）
6. 选区尺寸信息（如 "1920 x 1080"）
7. 放大镜（鼠标附近区域的放大视图 + RGB 值）
```

### 7.3 多显示器支持

```cpp
void CaptureOverlay::startCapture() {
    // 获取虚拟桌面的完整矩形（覆盖所有显示器）
    QRect virtualGeometry;
    for (QScreen* screen : QApplication::screens()) {
        virtualGeometry = virtualGeometry.united(screen->geometry());
    }

    // 设置窗口大小为虚拟桌面大小
    setGeometry(virtualGeometry);

    // 截取全屏
    m_screenPixmap = m_api->captureScreen();

    // 刷新窗口列表
    m_windowDetector.refresh();

    // 显示覆盖层
    showFullScreen();
}
```

---

## 8. RegionSelector 详细设计

```cpp
// core/region_selector.h
#pragma once
#include <QRect>
#include <QPoint>

namespace simpleshotter {

class RegionSelector {
public:
    enum class Handle {
        None,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight,
        Body   // 拖动整个选区
    };

    // 开始新选区
    void beginSelection(const QPoint& pos);

    // 更新选区（拖拽中）
    void updateSelection(const QPoint& pos);

    // 结束选区
    void endSelection();

    // 设置选区（从窗口/控件检测结果）
    void setRect(const QRect& rect);

    // 获取当前选区
    QRect rect() const;
    QRect normalizedRect() const;  // 确保 width/height > 0

    // 判断鼠标在哪个控制点上
    Handle hitTest(const QPoint& pos) const;

    // 开始移动/调整大小
    void beginResize(Handle handle, const QPoint& pos);
    void updateResize(const QPoint& pos);

    // 获取各控制点的矩形（用于绘制）
    std::array<QRect, 8> handleRects() const;

private:
    QRect m_rect;
    QPoint m_startPos;
    Handle m_activeHandle = Handle::None;
    QRect m_originalRect;  // resize 开始前的矩形
    QPoint m_resizeStartPos;
};

} // namespace simpleshotter
```

---

## 9. 信号槽通信流

```
MainWindow
  │
  │ onHotkeyTriggered()
  ▼
CaptureOverlay::startCapture()
  │
  │ captureConfirmed(pixmap, rect)
  ▼
PreviewWidget::show(pixmap)
  │
  ├─ copyClicked() → ClipboardManager::copy(pixmap)
  ├─ saveClicked() → FileSaver::save(pixmap)
  └─ pinClicked()  → MainWindow::createPinWindow(pixmap)
```

所有模块间通信使用 Qt 信号槽，避免直接依赖。

---

## 10. 技术决策总结

| 决策 | 方案 | 理由 |
|---|---|---|
| 窗口枚举 | Win32 EnumWindows / XCB query_tree | 原生 API，性能最佳 |
| 窗口矩形 | DwmGetWindowAttribute（Win） | 正确处理 Win10/11 隐形边框 |
| 控件识别 | UIA (Win) / AT-SPI2 (Linux) | 现代 API，覆盖面广 |
| 平台抽象 | 纯虚基类 + 工厂方法 | 简单清晰，编译期隔离 |
| 全屏覆盖 | QWidget + 自绘 paintEvent | 符合项目 UI 规则，控制力强 |
| 标注层 | Tool + Item + 快照式 Undo | 扩展性好，实现简单 |
| 钉图窗口 | 独立 QWidget + WindowStaysOnTopHint | 轻量，支持多实例 |
| 多显示器 | QScreen 虚拟桌面 | Qt 原生支持 |
| 构建隔离 | CMake 条件编译 + 平台子目录 | 清晰的代码组织 |
