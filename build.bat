@echo off
chcp 65001 >nul
rem ============================================================
rem  CounterForge 一键编译脚本
rem  如本机 Qt / MinGW 路径不同，只改下面三行即可
rem ============================================================
set QTDIR=D:\QT\6.10.2\mingw_64
set MINGW=D:\QT\Tools\mingw1310_64
set NINJA=D:\QT\Tools\Ninja

set PATH=%MINGW%\bin;%NINJA%;%QTDIR%\bin;%PATH%

cmake -S "%~dp0" -B "%~dp0build" -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=%QTDIR% ^
      -DCMAKE_CXX_COMPILER=%MINGW%/bin/g++.exe || goto :err

cmake --build "%~dp0build" || goto :err

echo.
echo [OK] 编译完成：build\CounterForge.exe
echo      如需独立发布目录，执行：
echo      windeployqt --release dist\CounterForge.exe
exit /b 0

:err
echo.
echo [FAILED] 编译失败，请检查上面的错误信息。
exit /b 1
