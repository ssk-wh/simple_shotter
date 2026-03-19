#!/usr/bin/env python3
"""
SimpleShotter dist 目录收集脚本

从 CMakeLists.txt 提取 Qt 模块依赖，收集 DLL、Qt 插件和资源到 dist 目录。
自动验证所有文件架构一致性（防止 32/64 位混用）。

用法:
    python scripts/collect_dist.py [--qt-dir D:/Qt/5.15.2/msvc2019_64] [--clean]
    python scripts/collect_dist.py --verify-only
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

# ── 项目配置 ──────────────────────────────────────────────
EXE_NAME = "SimpleShotter.exe"
BUILD_DIR = "build/src/Release"
DIST_DIR = "installer/dist"
EXTRA_FILES = ["resources/app_icon.ico"]
QT_PLUGINS = ["platforms", "imageformats", "iconengines", "styles"]


def check_pe_arch(pe_path: str) -> str:
    """检查 PE 文件架构"""
    try:
        with open(pe_path, "rb") as f:
            if f.read(2) != b"MZ":
                return "unknown"
            f.seek(0x3C)
            pe_offset = struct.unpack("<I", f.read(4))[0]
            f.seek(pe_offset + 4)
            machine = struct.unpack("<H", f.read(2))[0]
            if machine == 0x8664:
                return "x64"
            elif machine == 0x14C:
                return "x86"
            return "unknown"
    except Exception:
        return "unknown"


def extract_qt_modules(project_dir: Path) -> list[str]:
    """从 CMakeLists.txt 提取 Qt5::Xxx 模块名"""
    cmake_path = project_dir / "src" / "CMakeLists.txt"
    if not cmake_path.exists():
        return []

    content = cmake_path.read_text(encoding="utf-8")
    modules = re.findall(r"Qt5::(\w+)", content)
    # 去重保序
    seen = set()
    result = []
    for m in modules:
        if m not in seen:
            seen.add(m)
            result.append(m)
    return result


def get_vc_runtime_dlls() -> list[str]:
    """获取需要的 VC 运行时 DLL（从 System32 收集）"""
    sys32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    needed = [
        "vcruntime140.dll",
        "vcruntime140_1.dll",
        "msvcp140.dll",
        "msvcp140_1.dll",
        "concrt140.dll",
    ]
    found = []
    for dll in needed:
        full = sys32 / dll
        if full.exists():
            found.append(str(full))
    return found


def try_ldd(exe_path: str, qt_bin: str) -> list[str]:
    """尝试用 ldd 分析依赖（MSYS2 环境下可用）"""
    try:
        env = os.environ.copy()
        env["PATH"] = qt_bin + ";" + env.get("PATH", "")
        result = subprocess.run(
            ["ldd", exe_path], capture_output=True, text=True, env=env, timeout=10
        )
        dlls = []
        for line in result.stdout.splitlines():
            line = line.strip()
            if "=>" in line and "not found" not in line:
                parts = line.split("=>")
                if len(parts) == 2:
                    path = parts[1].strip().split("(")[0].strip()
                    if path and os.path.isfile(path):
                        dlls.append(path)
        return dlls
    except Exception:
        return []


def main():
    parser = argparse.ArgumentParser(description="收集 SimpleShotter dist 目录依赖")
    parser.add_argument("--qt-dir", default=r"D:\Qt\5.15.2\msvc2019_64",
                        help="Qt 安装目录")
    parser.add_argument("--clean", action="store_true",
                        help="清空 dist 目录后重新收集")
    parser.add_argument("--verify-only", action="store_true",
                        help="仅验证 dist 目录完整性")
    args = parser.parse_args()

    project_dir = Path(__file__).resolve().parent.parent
    qt_dir = Path(args.qt_dir)
    build_dir = project_dir / BUILD_DIR
    dist_dir = project_dir / DIST_DIR
    exe_path = build_dir / EXE_NAME

    # 验证路径
    if not exe_path.exists():
        print(f"[错误] exe 不存在: {exe_path}")
        print(f"  请先编译: cmake --build build --config Release")
        sys.exit(1)

    qt_bin = qt_dir / "bin"
    if not (qt_bin / "Qt5Core.dll").exists():
        print(f"[错误] Qt 路径无效: {qt_dir}")
        sys.exit(1)

    exe_arch = check_pe_arch(str(exe_path))
    qt_arch = check_pe_arch(str(qt_bin / "Qt5Core.dll"))
    if exe_arch != qt_arch:
        print(f"[错误] 架构不匹配! exe={exe_arch}, Qt={qt_arch}")
        sys.exit(1)

    if args.verify_only:
        ok = verify_dist(dist_dir, exe_arch)
        sys.exit(0 if ok else 1)

    # ── 收集流程 ──────────────────────────────────────────
    print("=" * 50)
    print(" SimpleShotter 依赖收集")
    print(f"  架构: {exe_arch}")
    print(f"  Qt:   {qt_dir}")
    print("=" * 50)

    # 清理
    if args.clean and dist_dir.exists():
        shutil.rmtree(dist_dir)
    dist_dir.mkdir(parents=True, exist_ok=True)

    # 1. 复制 exe
    print(f"\n[1/5] 复制 exe")
    shutil.copy2(exe_path, dist_dir / EXE_NAME)
    print(f"  {EXE_NAME}")

    # 2. 从 CMakeLists.txt 提取 Qt 模块并复制 DLL
    print(f"\n[2/5] 收集 Qt 模块 DLL")
    qt_modules = extract_qt_modules(project_dir)
    print(f"  检测到模块: {', '.join(qt_modules)}")
    for mod in qt_modules:
        dll_name = f"Qt5{mod}.dll"
        src = qt_bin / dll_name
        if src.exists():
            shutil.copy2(src, dist_dir / dll_name)
            print(f"  -> {dll_name}")
        else:
            print(f"  [警告] {dll_name} 不存在")

    # 3. 收集 Qt 插件
    print(f"\n[3/5] 收集 Qt 插件")
    for plugin_name in QT_PLUGINS:
        src_dir = qt_dir / "plugins" / plugin_name
        if not src_dir.exists():
            print(f"  [跳过] {plugin_name}/ (不存在)")
            continue
        dst_dir = dist_dir / plugin_name
        if dst_dir.exists():
            shutil.rmtree(dst_dir)
        dst_dir.mkdir()
        count = 0
        for dll in sorted(src_dir.glob("*.dll")):
            # 跳过 debug 版 (xxxd.dll)
            if dll.stem.endswith("d") and (src_dir / f"{dll.stem[:-1]}.dll").exists():
                continue
            shutil.copy2(dll, dst_dir / dll.name)
            count += 1
        print(f"  -> {plugin_name}/ ({count} 个)")

    # 4. 复制额外文件
    print(f"\n[4/5] 复制额外文件")
    for extra in EXTRA_FILES:
        src = project_dir / extra
        if src.exists():
            shutil.copy2(src, dist_dir / src.name)
            print(f"  -> {src.name}")
        else:
            print(f"  [警告] 不存在: {extra}")

    # 5. 验证
    print(f"\n[5/5] 验证")
    ok = verify_dist(dist_dir, exe_arch)

    # 统计
    total_size = sum(f.stat().st_size for f in dist_dir.rglob("*") if f.is_file())
    file_count = sum(1 for _ in dist_dir.rglob("*") if _.is_file())
    print(f"\n{'=' * 50}")
    if ok:
        print(f" 完成! {file_count} 个文件, {total_size / 1024 / 1024:.1f} MB")
    else:
        print(f" 完成但有问题，请检查上述警告")
    print(f" {dist_dir}")
    print("=" * 50)


def verify_dist(dist_dir: Path, expected_arch: str = None) -> bool:
    """验证 dist 目录中所有 DLL 架构一致"""
    if not dist_dir.exists():
        print("  [错误] dist 目录不存在")
        return False

    exe_file = dist_dir / EXE_NAME
    if not exe_file.exists():
        print(f"  [错误] {EXE_NAME} 不在 dist 中")
        return False

    if expected_arch is None:
        expected_arch = check_pe_arch(str(exe_file))

    all_ok = True

    # 检查所有 DLL 架构
    for dll in sorted(dist_dir.rglob("*.dll")):
        arch = check_pe_arch(str(dll))
        if arch != expected_arch:
            rel = dll.relative_to(dist_dir)
            print(f"  [架构错误] {rel}: {arch} (应为 {expected_arch})")
            all_ok = False

    # 检查必要的 Qt 插件目录
    for plugin in QT_PLUGINS:
        pdir = dist_dir / plugin
        if not pdir.exists() or not list(pdir.glob("*.dll")):
            print(f"  [缺失] {plugin}/ 插件目录")
            all_ok = False

    # 检查必要的 Qt 模块 DLL
    required_qt = ["Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll"]
    for dll in required_qt:
        if not (dist_dir / dll).exists():
            print(f"  [缺失] {dll}")
            all_ok = False

    if all_ok:
        print(f"  [OK] 全部通过 ({expected_arch})")

    return all_ok


if __name__ == "__main__":
    main()
