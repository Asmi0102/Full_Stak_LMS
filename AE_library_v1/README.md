# 📚 AE Library Management System
### Full-Stack Application · C++17 Backend · HTML/JS Frontend
#### Primary Build Target: MSYS2 UCRT64 (Windows)

---

## 📁 Project Structure

```
AE_Library_System/
│
├── README.md
├── START_SERVER.bat              ← Double-click to start backend (Windows)
├── START_FRONTEND.bat            ← Double-click to start frontend server
│
├── backend/                      ← C++17 REST API Server
│   ├── main.cpp                  ← HTTP server, router, all endpoints
│   ├── Makefile                  ← Cross-platform build
│   ├── build_msys2.sh            ← ★ MSYS2 UCRT64 build script
│   ├── install_deps_msys2.sh     ← ★ Install all MSYS2 packages
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── database.h            ← Database class + StaffMember struct
│   ├── models/
│   │   ├── Book.h                ← Book model (with genre field)
│   │   ├── Member.h              ← Member model
│   │   └── Transaction.h         ← Transaction model
│   ├── services/
│   │   ├── LibraryService.h      ← Business logic declarations
│   │   └── LibraryService.cpp    ← Business logic implementation
│   └── src/
│       └── database.cpp          ← SQLite CRUD with prepared statements
│
└── frontend/                     ← Single flat folder, one server
    ├── index.html                ← Landing page (role selection)
    ├── staff_login.html          ← Staff login (validates via backend)
    ├── admin_login.html          ← Admin login (admin / admin123)
    ├── staff_dashboard.html      ← Staff: live stats + recent activity
    ├── staff_books.html          ← Staff: catalog with genre field
    ├── staff_members.html        ← Staff: members + search + fines
    ├── staff_issue_return.html   ← Staff: searchable dropdowns + custom due date
    ├── staff_transactions.html   ← Staff: filters, date range, clear fine, CSV
    ├── admin_dashboard.html      ← Admin: system overview + activity table
    ├── admin_books.html          ← Admin: full catalog management
    ├── admin_members.html        ← Admin: member directory + fine control
    ├── admin_transactions.html   ← Admin: transactions + return + clear fine
    ├── admin_staff.html          ← Admin: add/remove staff, generate passwords
    ├── admin_settings.html       ← Admin: profile, fine rules, exports
    ├── api.js                    ← Shared API client (staff pages)
    ├── admin-api.js              ← Shared API client (admin pages)
    └── nav.js                    ← Staff nav helper
```

---

## 🔐 Default Login Credentials

| Role | Username | Password | Portal URL |
|---|---|---|---|
| **Admin** | `admin` | `admin123` | http://localhost:3000/admin_login.html |
| **Staff** | `staff` | `staff123` | http://localhost:3000/staff_login.html |

> Staff accounts are validated against the backend database.
> Admin can create additional staff accounts via **Admin → Staff Management**.

---

## 🚀 How to Run (MSYS2 UCRT64)

### One-time setup
```bash
# Open MSYS2 UCRT64 terminal (orange icon in Start Menu)
cd /d/projects/AE_Library_System/backend
bash install_deps_msys2.sh     # installs gcc, boost, sqlite3, nlohmann-json
```

### Every time you run the app

**Terminal 1 — Backend (MSYS2 UCRT64)**
```bash
cd /d/projects/AE_Library_System/backend
bash build_msys2.sh            # compile (only needed after code changes)
./library_server.exe           # start server on port 8080
```

**Terminal 2 — Frontend (any terminal)**
```bash
cd /d/projects/AE_Library_System/frontend
python -m http.server 3000
```

**Open browser → http://localhost:3000/index.html**

### Windows Firewall (if port 8080 is blocked)
Run once in Admin PowerShell:
```powershell
netsh advfirewall firewall add rule name="AE Library" protocol=TCP dir=in localport=8080 action=allow
```
Or run `library_server.exe` as Administrator.

---

## 🌐 Complete API Reference

Base URL: `http://localhost:8080`

### Books
| Method | Endpoint | Body | Description |
|---|---|---|---|
| GET | `/books/all` | — | All books (includes genre) |
| GET | `/books/available` | — | Books with available > 0 |
| GET | `/books/search?q=` | — | Search by title/author/genre |
| POST | `/books/add` | `{title, author, genre, quantity}` | Add book |
| DELETE | `/books/remove/{id}` | — | Remove book |

### Members
| Method | Endpoint | Body | Description |
|---|---|---|---|
| GET | `/members/all` | — | All members |
| POST | `/members/add` | `{name, email, phone}` | Register member (auto-ID) |
| DELETE | `/members/remove/{id}` | — | Remove member |

### Transactions
| Method | Endpoint | Body | Description |
|---|---|---|---|
| POST | `/issue` | `{member_id, book_id, due_date?}` | Issue book (due_date optional) |
| POST | `/return` | `{transaction_id}` | Return book + calculate fine |
| GET | `/transactions` | — | All transactions |
| GET | `/transactions/active` | — | Only issued (not returned) |

### Fines & Settings
| Method | Endpoint | Body | Description |
|---|---|---|---|
| POST | `/fine/clear/{member_id}` | — | Clear all fines for member |
| GET | `/settings` | — | Get fine rate + allowed days |
| POST | `/settings/fine-rate` | `{fine_rate_per_day}` | Set fine rate |
| POST | `/settings/allowed-days` | `{allowed_days}` | Set default borrow period |

### Staff Management
| Method | Endpoint | Body | Description |
|---|---|---|---|
| GET | `/staff/all` | — | All staff accounts |
| POST | `/staff/add` | `{username, password, name, email, role}` | Create staff account |
| DELETE | `/staff/remove/{id}` | — | Remove staff (cannot remove ID 1) |
| POST | `/staff/login` | `{username, password}` | Validate staff credentials |

---

## 🗄️ Database Schema

Auto-created as `library.db` on first run.

```sql
CREATE TABLE books (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    title     TEXT NOT NULL,
    author    TEXT NOT NULL,
    genre     TEXT NOT NULL DEFAULT 'General',   -- NEW: required field
    quantity  INTEGER NOT NULL DEFAULT 1,
    available INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE members (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    membership_number TEXT NOT NULL UNIQUE,       -- Auto-assigned: LIB1001, LIB1002...
    name              TEXT NOT NULL,
    email             TEXT NOT NULL,
    phone             TEXT NOT NULL,
    total_fine        REAL NOT NULL DEFAULT 0.0,
    joined_date       TEXT NOT NULL
);

CREATE TABLE transactions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    member_id   INTEGER NOT NULL REFERENCES members(id),
    book_id     INTEGER NOT NULL REFERENCES books(id),
    issue_date  TEXT NOT NULL,
    due_date    TEXT NOT NULL,                    -- Custom or auto (issue + allowed_days)
    return_date TEXT,
    status      TEXT NOT NULL DEFAULT 'issued',   -- 'issued' | 'returned'
    fine        REAL NOT NULL DEFAULT 0.0
);

CREATE TABLE staff (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    username     TEXT NOT NULL UNIQUE,
    password     TEXT NOT NULL,
    name         TEXT NOT NULL,
    email        TEXT NOT NULL,
    role         TEXT NOT NULL DEFAULT 'librarian',  -- 'admin' | 'librarian'
    created_date TEXT NOT NULL
);

CREATE TABLE settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
    -- fine_rate_per_day: default '2.0'
    -- allowed_days:      default '14'
);
```

**Default seeded on first run:**
- `staff` table: admin account (`admin` / `admin123`)

---

## 🧠 OOP Concepts Used

| Concept | Where in Code |
|---|---|
| **Classes** | `Database`, `LibraryService`, `RequestHandler` in backend |
| **Structs (Data Models)** | `Book`, `Member`, `Transaction`, `StaffMember` in `models/` |
| **Encapsulation** | `Database` hides raw `sqlite3*` handle; `LibraryService` hides SQL |
| **Abstraction** | HTTP handler calls `svc.issueBook()` — no SQL knowledge needed |
| **RAII** | `Database` constructor opens SQLite; destructor always closes it |
| **Constructor Overloading** | All model structs have default + parameterized constructors |
| **Composition (Has-A)** | `LibraryService` has-a `Database&`; `RequestHandler` has-a `LibraryService&` |
| **Exception Handling** | DB layer throws on SQL errors; caught at HTTP session boundary |
| **Move Semantics** | `std::move()` used in struct constructors to avoid string copies |
| **Separation of Concerns** | HTTP → Service → Database → Models: each layer has one job |

---

## ✅ Feature Summary

### Staff Portal
| Feature | Status |
|---|---|
| Book Management (Title, Author, **Genre**, Qty) | ✓ |
| Member Management with search (name + ID) | ✓ |
| Issue Book (searchable dropdowns, **custom due date**) | ✓ |
| Return Book with fine calculation | ✓ |
| Transaction Ledger (search, date range, status filter, CSV) | ✓ |
| Clear Fine button on transactions | ✓ |
| Backend-validated login | ✓ |
| Logout → Landing page | ✓ |

### Admin Portal
| Feature | Status |
|---|---|
| Live dashboard stats (no mock data) | ✓ |
| Recent activity table (member, book, type, status) | ✓ |
| Book catalog with genre | ✓ |
| Member directory with real counts + search | ✓ |
| Transactions: return button + clear fine + filters + CSV | ✓ |
| Staff Management: add/remove + auto-generate credentials | ✓ |
| Settings: admin profile, fine rules, data export | ✓ |
| Logout → Landing page | ✓ |
| Login page: fixed navbar overlap, reduced footer | ✓ |

---

## 🔧 Troubleshooting

| Problem | Fix |
|---|---|
| `cannot find -lboost_system` | Use `bash build_msys2.sh` (uses header-only Boost, no lib needed) |
| `nlohmann/json.hpp not found` | `pacman -S mingw-w64-ucrt-x86_64-nlohmann-json` |
| Port 8080 blocked (error 10013) | Run as Administrator or run the firewall command above |
| Staff login "Cannot reach server" | Start `library_server.exe` first |
| Stats show `—` dashes | Backend not running — check Terminal 1 |
| "Admin accounts use Admin Portal" on staff login | Use http://localhost:3000/admin_login.html for admin |
| Data not appearing after add | Refresh is automatic — if still missing, check console for errors |
| `python: not found` | Use `python3 -m http.server 3000` |
| Old `library.db` has no `genre`/`staff` column | Delete `library.db` and restart — tables recreate automatically |

---

*AE Library Management System · C++ OOP Mini Project*
*Backend: C++17 · Boost.Beast · SQLite3 · nlohmann/json*
*Frontend: HTML · Tailwind CSS · Vanilla JS · Material Symbols*
