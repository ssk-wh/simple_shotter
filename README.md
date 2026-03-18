# SimpleShotter

跨平台截图工具，类似 Snipaste，支持自动识别窗口区域和控件。

## 功能

- 自动识别窗口区域（鼠标悬停高亮）
- 自动识别窗口内控件（按钮、输入框等）
- 手动框选任意区域，支持拖拽调整
- 截图标注：矩形、椭圆、箭头、文字、马赛克
- 撤销/重做标注操作
- 复制截图到剪贴板
- 保存截图到文件（桌面/自定义目录）
- 系统托盘常驻，单击即截图
- 全局快捷键 `Ctrl+Shift+A`
- 多显示器支持
- 放大镜预览 + 像素颜色拾取
- 单实例运行（重复启动自动触发截图）

## 技术栈

- C++17 / Qt 5.15.2 / CMake 3.16+
- Windows: Win32 API + UI Automation
- Linux: X11/XCB + AT-SPI2

## 构建

### Windows (MSYS2/MinGW)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (Debian/Ubuntu/UOS)

```bash
sudo apt install cmake qtbase5-dev libqt5x11extras5-dev \
  libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev libx11-dev \
  libatspi2.0-dev libglib2.0-dev pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 打包

```bash
# Linux DEB
dpkg-buildpackage -us -uc -b

# Windows NSIS 安装包
cd installer && makensis SimpleShotter.nsi
```

## CI/CD

GitHub Actions 自动构建，推送到 master 时生成：

- **Windows**: Setup 安装包 + Portable 便携版
- **Linux**: DEB 安装包

推送 `v*` tag 时自动发布到 GitHub Release。

## TODO

### P1 - 后续迭代

- [ ] 画笔自由绘制
- [ ] 高 DPI / 缩放感知
- [ ] 全局快捷键可自定义
- [ ] 截图钉在桌面
- [ ] Wayland 支持

### 已知问题

- 部分 Electron/Chrome 应用控件识别需用户手动开启无障碍
- 控件递归遍历深度限制为 8 层，极深嵌套控件可能遗漏
