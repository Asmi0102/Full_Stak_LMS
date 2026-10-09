# ============================================================
#  AE Library Management System — Makefile
#  Compatible: MSYS2 UCRT64 (Windows) · Linux · macOS
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

INCLUDES := -I./include -I./models -I./services

SRCS     := main.cpp src/database.cpp services/LibraryService.cpp

TARGET   := library_server

# ── Detect OS and set libs accordingly ──────────────────────
ifeq ($(OS),Windows_NT)
    # MSYS2 UCRT64 — need winsock + mingw thread support
    LIBS    := -lsqlite3 -lboost_system -lws2_32 -lmswsock -lbcrypt -mthreads
    TARGET  := library_server.exe
    RM      := del /Q
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Darwin)
        # macOS — Homebrew paths
        LIBS    := -lsqlite3 -lboost_system -lpthread
        INCLUDES += -I/opt/homebrew/include -I/usr/local/include
        LDFLAGS := -L/opt/homebrew/lib -L/usr/local/lib
    else
        # Linux
        LIBS    := -lsqlite3 -lboost_system -lpthread
    endif
    RM      := rm -f
endif

# ── Build rules ──────────────────────────────────────────────
all: $(TARGET)
	@echo ""
	@echo "  Build successful!"
	@echo "  Run with: ./$(TARGET)"
	@echo ""

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LDFLAGS) $(LIBS) -o $@

clean:
	$(RM) library_server library_server.exe library.db

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
