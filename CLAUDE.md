# SimpleShotter - 跨平台截图应用

## 项目概述
类似 Snipaste 的跨平台截图工具，支持 Windows/Linux，自动识别窗口区域和控件。

## 技术栈
- **语言**: C++17
- **框架**: Qt 5.15.2
- **构建**: CMake 3.16+
- **平台**: Windows / Linux

## 编码规范
- 类名: PascalCase (如 `ScreenCapture`)
- 方法名: camelCase (如 `captureScreen`)
- 成员变量: `m_` 前缀 (如 `m_screenPixmap`)
- 常量: UPPER_SNAKE_CASE
- 文件名: snake_case (如 `screen_capture.h`)
- 命名空间: `simpleshotter`
- 头文件使用 `#pragma once`

## UI 规则
- **禁止**使用 .ui 文件，所有界面用 C++ 类实现
- **禁止**使用 stylesheet，通过重写 `paintEvent` 实现自定义绘制
- 界面要美观、交互友好

## 项目结构
```
simple_shotter/
├── CMakeLists.txt              # 顶层 CMake
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── core/                   # 核心逻辑
│   │   ├── screen_capture.h/cpp      # 截屏采集
│   │   ├── window_detector.h/cpp     # 窗口检测
│   │   ├── control_detector.h/cpp    # 控件检测
│   │   └── region_selector.h/cpp     # 区域选择
│   ├── ui/                     # 界面组件
│   │   ├── capture_overlay.h/cpp     # 截图覆盖层
│   │   ├── toolbar_widget.h/cpp      # 工具栏
│   │   ├── preview_widget.h/cpp      # 预览窗口（放大镜+颜色拾取）
│   │   ├── main_window.h/cpp        # 主窗口(托盘)
│   │   ├── annotation_item.h/cpp    # 标注图元（矩形/椭圆/箭头/文字/马赛克）
│   │   ├── save_menu_widget.h/cpp   # 保存菜单
│   │   └── style_panel_widget.h/cpp # 标注样式面板
│   ├── utils/                  # 工具类
│   │   ├── clipboard_manager.h/cpp   # 剪贴板
│   │   ├── file_saver.h/cpp         # 文件保存
│   │   └── hotkey_manager.h/cpp     # 快捷键
│   └── platform/               # 平台相关
│       ├── platform_api.h/cpp       # 平台抽象接口 + 工厂
│       ├── win/                     # Windows 实现 (UIAutomation)
│       └── linux/                   # Linux 实现 (XCB + AT-SPI)
├── debian/                     # Debian 打包配置
├── installer/                  # Windows NSIS 安装包脚本
├── tests/                      # 单元测试
└── resources/                  # 资源文件（图标等）
```

## 架构设计要点
- 平台相关代码通过 `PlatformApi` 抽象接口隔离，工厂方法创建实例
- Windows: Win32 API + COM UIAutomation 实现控件识别
- Linux: XCB 窗口枚举 + AT-SPI2 控件检测 + X11 XGrabKey 全局快捷键
- 核心逻辑与 UI 解耦，便于测试
- 使用信号槽机制进行模块间通信

## 功能清单
### 已完成
- [x] 全屏截图 / 区域选择截图
- [x] 自动识别窗口区域和控件
- [x] 复制到剪贴板 / 保存到文件（桌面/自定义目录）
- [x] 系统托盘 / 全局快捷键
- [x] 标注工具：矩形、椭圆、箭头、文字、马赛克
- [x] 撤销/重做
- [x] 单实例运行
- [x] DEB 打包 / NSIS 安装包
- [x] GitHub Actions CI/CD（Windows + Linux）

### 待实现
- [ ] 画笔自由绘制
- [ ] 截图钉在桌面
- [ ] 高 DPI / 缩放感知
- [ ] Wayland 支持
