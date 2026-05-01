#!/usr/bin/env bash
# ============================================================
#  build_msys2.sh — One-click build for MSYS2 UCRT64
# ============================================================

set -e

echo ""
echo "  AE Library -- MSYS2 UCRT64 Build Script"
echo "  ========================================"
echo ""

# Compiler check
if ! command -v g++ &>/dev/null; then
    echo "  [ERROR] g++ not found."
    echo "  Run: pacman -S mingw-w64-ucrt-x86_64-gcc"
    exit 1
fi
echo "  [OK] Compiler:  $(g++ --version | head -1)"

# nlohmann/json
JSON_INC=""
if [ -f "/ucrt64/include/nlohmann/json.hpp" ]; then
    JSON_INC="-I/ucrt64/include"
    echo "  [OK] nlohmann/json: /ucrt64/include"
elif [ -f "/mingw64/include/nlohmann/json.hpp" ]; then
    JSON_INC="-I/mingw64/include"
    echo "  [OK] nlohmann/json: /mingw64/include"
else
    echo "  [WARN] Downloading nlohmann/json fallback..."
    mkdir -p ./vendor/nlohmann
    curl -sL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" \
         -o ./vendor/nlohmann/json.hpp 2>/dev/null || {
        echo "  [ERROR] Run: pacman -S mingw-w64-ucrt-x86_64-nlohmann-json"
        exit 1
    }
    JSON_INC="-I./vendor"
    echo "  [OK] nlohmann/json: ./vendor (downloaded)"
fi

# SQLite
SQLITE_CFLAGS=$(pkg-config --cflags sqlite3 2>/dev/null || echo "-I/ucrt64/include")
SQLITE_LIBS=$(pkg-config --libs   sqlite3 2>/dev/null || echo "-lsqlite3")
echo "  [OK] SQLite3"
echo "  [OK] Boost (header-only -- no -lboost_system needed)"
echo ""
echo "  Compiling..."
echo ""

g++ -std=c++17 -O2 -Wall \
    main.cpp \
    src/database.cpp \
    services/LibraryService.cpp \
    -I./include -I./models -I./services \
    -I/ucrt64/include \
    $JSON_INC \
    $SQLITE_CFLAGS \
    -L/ucrt64/lib \
    $SQLITE_LIBS \
    -lws2_32 \
    -lmswsock \
    -lbcrypt \
    -mthreads \
    -o library_server.exe

echo ""
echo "  ========================================="
echo "  Build successful!"
echo "  Run:  ./library_server.exe"
echo ""
echo "  If you get a port/firewall error, run this"
echo "  in an ADMIN PowerShell to allow port 8080:"
echo "    netsh advfirewall firewall add rule name=\"AE Library\" protocol=TCP dir=in localport=8080 action=allow"
echo "  OR just run library_server.exe as Administrator."
echo "  ========================================="
echo ""
