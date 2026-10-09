#!/usr/bin/env bash
# ============================================================
#  install_deps_msys2.sh
#  Run once in MSYS2 UCRT64 to install all required packages.
# ============================================================

echo ""
echo "  Installing AE Library dependencies for MSYS2 UCRT64..."
echo ""

pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-boost \
    mingw-w64-ucrt-x86_64-sqlite3 \
    mingw-w64-ucrt-x86_64-nlohmann-json \
    mingw-w64-ucrt-x86_64-pkg-config \
    make \
    curl

echo ""
echo "  All dependencies installed!"
echo "  Now run:  bash build_msys2.sh"
echo ""
