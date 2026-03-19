@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: ============================================================
:: SimpleShotter 一键打包脚本
:: 用法: scripts\pack.bat [Qt路径] [NSIS路径]
:: 示例: scripts\pack.bat D:\Qt\5.15.2\msvc2019_64
:: ============================================================

set "PROJECT_DIR=%~dp0.."
set "QT_DIR=%~1"
set "NSIS_DIR=%~2"

if "%QT_DIR%"=="" set "QT_DIR=D:\Qt\5.15.2\msvc2019_64"
if "%NSIS_DIR%"=="" set "NSIS_DIR=C:\Program Files (x86)\NSIS"

set "BUILD_DIR=%PROJECT_DIR%\build"
set "NSI_SCRIPT=%PROJECT_DIR%\installer\SimpleShotter.nsi"
set "OUTPUT=%PROJECT_DIR%\installer\SimpleShotter-0.2.0-Setup.exe"

:: 检查依赖
if not exist "%QT_DIR%\bin\Qt5Core.dll" (
    echo [错误] Qt 路径无效: %QT_DIR%
    exit /b 1
)
if not exist "%NSIS_DIR%\makensis.exe" (
    echo [错误] NSIS 未安装: %NSIS_DIR%
    exit /b 1
)

echo ============================================================
echo  SimpleShotter 打包流程
echo  Qt:   %QT_DIR%
echo  NSIS: %NSIS_DIR%
echo ============================================================

:: Step 1: 编译
echo.
echo [1/3] 编译 Release...
cmake -B "%BUILD_DIR%" -DCMAKE_PREFIX_PATH="%QT_DIR%" "%PROJECT_DIR%"
if errorlevel 1 (echo [错误] CMake 配置失败 & exit /b 1)

cmake --build "%BUILD_DIR%" --config Release --target SimpleShotter
if errorlevel 1 (echo [错误] 编译失败 & exit /b 1)
echo [OK] 编译成功

:: Step 2: 收集依赖 (调用 Python 脚本)
echo.
echo [2/3] 收集依赖...
python "%PROJECT_DIR%\scripts\collect_dist.py" --qt-dir "%QT_DIR%" --clean
if errorlevel 1 (echo [错误] 依赖收集失败 & exit /b 1)

:: Step 3: NSIS 打包
echo.
echo [3/3] 生成安装包...
pushd "%PROJECT_DIR%\installer"
"%NSIS_DIR%\makensis.exe" "%NSI_SCRIPT%"
if errorlevel 1 (echo [错误] NSIS 打包失败 & popd & exit /b 1)
popd

echo.
echo ============================================================
for %%f in ("%OUTPUT%") do echo  安装包: %%~dpnxf  (%%~zf bytes)
echo ============================================================
