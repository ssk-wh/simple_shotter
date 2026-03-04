# EasyShotter - 跨平台截图应用

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
- 命名空间: `easyshotter`
- 头文件使用 `#pragma once`

## UI 规则
- **禁止**使用 .ui 文件，所有界面用 C++ 类实现
- **禁止**使用 stylesheet，通过重写 `paintEvent` 实现自定义绘制
- 界面要美观、交互友好

## 项目结构
```
easy_shotter/
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
│   │   ├── preview_widget.h/cpp      # 预览窗口
│   │   └── main_window.h/cpp        # 主窗口(托盘)
│   ├── utils/                  # 工具类
│   │   ├── clipboard_manager.h/cpp   # 剪贴板
│   │   ├── file_saver.h/cpp         # 文件保存
│   │   └── hotkey_manager.h/cpp     # 快捷键
│   └── platform/               # 平台相关
│       ├── platform_api.h           # 平台抽象接口
│       ├── win/                     # Windows 实现
│       └── linux/                   # Linux 实现
├── tests/                      # 单元测试
│   ├── CMakeLists.txt
│   └── ...
└── resources/                  # 资源文件
```

## 架构设计要点
- 平台相关代码通过抽象接口隔离
- 核心逻辑与 UI 解耦，便于测试
- 预留绘画/标注层扩展接口（后期支持画笔、马赛克等）
- 使用信号槽机制进行模块间通信

## 功能清单
### P0 - 当前迭代
- [x] 全屏截图
- [x] 区域选择截图
- [x] 自动识别窗口区域
- [x] 自动识别窗口内控件
- [x] 复制到剪贴板
- [x] 保存到文件（桌面/自定义目录）
- [x] 系统托盘
- [x] 全局快捷键

### P1 - 后续迭代（已预留架构）
- [ ] 画笔标注
- [ ] 马赛克/模糊
- [ ] 箭头/文字标注
- [ ] 撤销/重做
- [ ] 截图钉在桌面
