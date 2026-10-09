@echo off
title AE Library — Backend Server
echo.
echo   AE Library Management System
echo   ================================
echo   Starting backend on port 8080...
echo.
cd /d "%~dp0backend"
if exist library_server.exe (
    library_server.exe
) else (
    echo   [ERROR] library_server.exe not found.
    echo   Please build first using MSYS2 UCRT64:
    echo     cd backend
    echo     bash install_deps_msys2.sh
    echo     bash build_msys2.sh
    echo.
    pause
)
