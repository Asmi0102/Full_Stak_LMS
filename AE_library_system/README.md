# 📚 AE Library Management System
### Full-Stack · C++17 Backend · HTML/JS Frontend
#### Build Target: MSYS2 UCRT64 (Windows)

---

## 🚀 How to Run

**Terminal 1 — Backend (MSYS2 UCRT64)**
```bash
cd /d/projects/AE_Library_System/backend
bash build_msys2.sh        # compile (once, or after code changes)
./library_server.exe       # start on port 8080
```

**Terminal 2 — Frontend (any terminal)**
```bash
cd /d/projects/AE_Library_System/frontend
python -m http.server 3000
```

**Browser → http://localhost:3000/index.html**

| Role | Login URL | Username | Password |
|---|---|---|---|
| Admin | http://localhost:3000/admin_login.html | `admin` | `admin123` |
| Staff | http://localhost:3000/staff_login.html | `staff` | `staff123` |

> If port 8080 is blocked: run as Administrator, or run in Admin PowerShell:
> `netsh advfirewall firewall add rule name="AELib" protocol=TCP dir=in localport=8080 action=allow`

---

## 📁 Project Structure

```
AE_Library_System/
├── backend/
│   ├── main.cpp                    HTTP server + all API endpoints
│   ├── build_msys2.sh              One-click MSYS2 build script
│   ├── install_deps_msys2.sh       Install all MSYS2 packages
│   ├── Makefile
│   ├── include/database.h          Database class declaration
│   ├── models/
│   │   ├── Book.h                  Book struct (title, author, genre, qty, available)
│   │   ├── Member.h                Member struct
│   │   └── Transaction.h          Transaction struct (+ staff_id, staff_name)
│   ├── services/
│   │   ├── LibraryService.h/cpp    Business logic layer
│   └── src/database.cpp            All SQLite CRUD operations
│
└── frontend/
    ├── index.html                  Landing page (role selection)
    ├── admin_login.html            Admin login → admin_dashboard.html
    ├── admin_dashboard.html        Live stats + recent activity (with Staff column)
    ├── admin_books.html            Catalog management + genre
    ├── admin_members.html          Member directory + fine control
    ├── admin_transactions.html     Transactions + Staff Name column + return + clear fine
    ├── admin_staff.html            Add/remove staff + generate passwords
    ├── admin_settings.html         ★ Persistent profile + fine rules (backend-saved)
    ├── staff_login.html            Backend-validated login + first_login detection
    ├── staff_change_password.html  ★ NEW: forced password change on first login
    ├── staff_dashboard.html        Live stats + quick links + Settings in sidebar
    ├── staff_books.html            Books + genre + Settings in sidebar
    ├── staff_members.html          Members + search + Settings in sidebar
    ├── staff_issue_return.html     Issue/Return (info text removed)
    ├── staff_transactions.html     Ledger + filters + clear fine
    ├── staff_settings.html         ★ NEW: staff can update email + password only
    ├── api.js                      Staff API client (staff_id auto-injected)
    └── admin-api.js                Admin API client (name loaded from backend)
```

---

## ✅ Changes Made in This Version

### 1. Admin Profile — Persistent Storage
**Files:** `backend/src/database.cpp`, `backend/main.cpp`, `backend/services/LibraryService.cpp/h`, `frontend/admin_settings.html`, `frontend/admin-api.js`

**How it works:**
- Admin name, email and password are stored in the `settings` table under keys `admin_name`, `admin_email`, `admin_password`
- New endpoint `GET /admin/profile` returns the stored name and email
- New endpoint `POST /admin/update-profile` accepts `{name, email, new_password, current_password}` and saves to DB
- Current password must be verified before a new password is accepted
- All admin pages call `/admin/profile` on load to show the current persistent name
- On login, sessionStorage gets `'Admin'` as placeholder; the actual name is fetched from backend on each page

**Default admin name** is now `"Admin"` (not "Dr. Julian Vane").

**Schema:** No new table. New settings keys:
```sql
INSERT OR IGNORE INTO settings VALUES ('admin_name', 'Admin');
INSERT OR IGNORE INTO settings VALUES ('admin_email', 'admin@aelibrary.com');
INSERT OR IGNORE INTO settings VALUES ('admin_password', 'admin123');
```

---

### 2. Fine & Lending Rules — Persistent + System-Wide
**Files:** `backend/src/database.cpp`, `backend/services/LibraryService.cpp`, `frontend/admin_settings.html`

**How it works:**
- Fine rate and allowed days were already stored in the `settings` table — they were always persistent
- The Settings page now uses `INSERT OR REPLACE` (via `setSetting()`) to guarantee the value is saved even if the key pre-exists
- All new `issueBook()` calls read `db_.getAllowedDays()` live at issue time — so changing the setting takes effect immediately for all future transactions
- Fine is computed at return time using `db_.getFineRate()` live — so a rate change affects all future returns

**No schema change needed** — already in `settings` table.

---

### 3. Staff Name in Transactions
**Files:** `backend/models/Transaction.h`, `backend/src/database.cpp`, `backend/main.cpp`, `frontend/admin_dashboard.html`, `frontend/admin_transactions.html`

**How it works:**
- `Transaction` struct now has `int staff_id` and `std::string staff_name` fields
- `transactions` table has a new `staff_id INTEGER NOT NULL DEFAULT 0` column (auto-migrated on startup with `ALTER TABLE ... ADD COLUMN` — safe if column already exists)
- `addTransaction()` accepts `int staff_id` parameter
- `returnBook()` accepts `int staff_id` parameter (updates staff_id if not already set)
- All transaction queries do a `LEFT JOIN staff sf ON t.staff_id=sf.id` to get `sf.name`
- The JS clients (`api.js`) auto-inject `staff_id` from `sessionStorage.getItem('ae_staff_id')` on every issue/return call
- Admin dashboard "Recent Activity" and Admin Transactions both show a **Staff** column

**Schema change:**
```sql
-- Auto-applied at startup (ALTER TABLE is safe to run on existing DB)
ALTER TABLE transactions ADD COLUMN staff_id INTEGER NOT NULL DEFAULT 0;
```

---

### 4a. First Login Forced Password Change
**Files:** `backend/src/database.cpp`, `backend/include/database.h`, `backend/main.cpp`, `frontend/staff_login.html`, `frontend/staff_change_password.html` (NEW)

**How it works:**
- `staff` table has a new `first_login INTEGER NOT NULL DEFAULT 0` column
- Every new staff created by admin gets `first_login = 1`
- The seeded admin gets `first_login = 0` (never forced)
- On `/staff/login`, the response includes `"first_login": true/false`
- If `first_login` is true, `staff_login.html` redirects to `staff_change_password.html` instead of the dashboard
- `staff_change_password.html` blocks all navigation, requires a new password (≥ 6 chars), calls `POST /staff/update-profile` then `POST /staff/clear-first-login/{id}`
- Once cleared, `first_login = 0` in DB — staff lands on dashboard normally on all future logins

**Schema change:**
```sql
-- Auto-applied at startup
ALTER TABLE staff ADD COLUMN first_login INTEGER NOT NULL DEFAULT 0;
-- New staff (created by admin): first_login = 1
-- Admin seed: first_login = 0
```

**New endpoints:**
- `POST /staff/update-profile` — `{staff_id, email, new_password}`
- `POST /staff/clear-first-login/{id}`

---

### 4b. Staff Settings Page
**Files:** `frontend/staff_settings.html` (NEW), all staff sidebar pages (MODIFIED)

**How it works:**
- New `staff_settings.html` page in the same folder, accessible from every staff sidebar
- Staff can update **email only** and **password only** — no name or role changes
- Calls `POST /staff/update-profile` with `staff_id` from sessionStorage
- A "Settings" link was added **above** the Sign Out button in all 5 staff page sidebars

---

### 4c. Issue/Return Info Text Removed
**File:** `frontend/staff_issue_return.html`

Removed the blue info box that said:
> "Issue date is automatically set to today. If due date is not specified, it defaults to 7 days from today."

---

## 🗄️ Complete Database Schema

```sql
CREATE TABLE books (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL, author TEXT NOT NULL,
    genre TEXT NOT NULL DEFAULT 'General',
    quantity INTEGER NOT NULL DEFAULT 1,
    available INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE members (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    membership_number TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL, email TEXT NOT NULL, phone TEXT NOT NULL,
    total_fine REAL NOT NULL DEFAULT 0.0,
    joined_date TEXT NOT NULL
);

CREATE TABLE transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    member_id INTEGER NOT NULL REFERENCES members(id),
    book_id INTEGER NOT NULL REFERENCES books(id),
    issue_date TEXT NOT NULL, due_date TEXT NOT NULL,
    return_date TEXT,
    status TEXT NOT NULL DEFAULT 'issued',
    fine REAL NOT NULL DEFAULT 0.0,
    staff_id INTEGER NOT NULL DEFAULT 0   -- ← NEW: who handled this transaction
);

CREATE TABLE staff (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    name TEXT NOT NULL, email TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'librarian',
    created_date TEXT NOT NULL,
    first_login INTEGER NOT NULL DEFAULT 0  -- ← NEW: 1 = must change password
);

CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
    -- fine_rate_per_day  → e.g. "2.0"
    -- allowed_days       → e.g. "14"
    -- admin_name         → e.g. "Admin"          ← NEW
    -- admin_email        → e.g. "admin@ae.com"   ← NEW
    -- admin_password     → e.g. "admin123"        ← NEW
);
```

---

## 🌐 API Endpoints

### Books
| Method | Endpoint | Description |
|---|---|---|
| GET | `/books/all` | All books |
| GET | `/books/available` | Books with copies available |
| GET | `/books/search?q=` | Search by title/author/genre |
| POST | `/books/add` | `{title, author, genre, quantity}` |
| DELETE | `/books/remove/{id}` | Remove book |

### Members
| Method | Endpoint | Description |
|---|---|---|
| GET | `/members/all` | All members |
| POST | `/members/add` | `{name, email, phone}` |
| DELETE | `/members/remove/{id}` | Remove member |

### Transactions
| Method | Endpoint | Description |
|---|---|---|
| POST | `/issue` | `{member_id, book_id, due_date?, staff_id}` |
| POST | `/return` | `{transaction_id, staff_id}` |
| GET | `/transactions` | All (includes staff_name) |
| GET | `/transactions/active` | Issued only |

### Fines & Settings
| Method | Endpoint | Description |
|---|---|---|
| POST | `/fine/clear/{member_id}` | Clear member fines |
| GET | `/settings` | Get fine_rate + allowed_days |
| POST | `/settings/fine-rate` | `{fine_rate_per_day}` |
| POST | `/settings/allowed-days` | `{allowed_days}` |

### Staff
| Method | Endpoint | Description |
|---|---|---|
| GET | `/staff/all` | All staff |
| POST | `/staff/add` | `{username, password, name, email, role}` |
| DELETE | `/staff/remove/{id}` | Remove (not ID 1) |
| POST | `/staff/login` | `{username, password}` → returns first_login flag |
| POST | `/staff/update-profile` | `{staff_id, email, new_password}` |
| POST | `/staff/clear-first-login/{id}` | Mark first login done |

### Admin Profile
| Method | Endpoint | Description |
|---|---|---|
| GET | `/admin/profile` | Get persistent admin name + email |
| POST | `/admin/update-profile` | `{name, email, new_password, current_password}` |

---

## 🧠 OOP Concepts Used

| Concept | Where |
|---|---|
| **Classes** | `Database`, `LibraryService`, `RequestHandler` |
| **Structs** | `Book`, `Member`, `Transaction` (with staff_id/staff_name), `StaffMember` (with first_login) |
| **Encapsulation** | `Database` hides `sqlite3*`; `LibraryService` hides SQL and date math |
| **Abstraction** | HTTP handler calls `svc.updateAdminProfile()` — no DB knowledge |
| **RAII** | `Database` opens SQLite in constructor, closes in destructor |
| **Constructor Overloading** | All model structs have default + parameterized constructors |
| **Composition** | `LibraryService` has-a `Database&`; `RequestHandler` has-a `LibraryService&` |
| **Exception Handling** | DB throws on SQL error; caught at HTTP session boundary |
| **Move Semantics** | `std::move()` in struct constructors avoids string copies |
| **Separation of Concerns** | HTTP → Service → Database → Models |

---

## 🔧 Troubleshooting

| Problem | Fix |
|---|---|
| `ALTER TABLE` error on startup | Safe to ignore — it means column already exists |
| Staff login redirects to change password | Expected on first login — set a new password |
| Admin name still shows "Admin" after save | Refresh the page; name loads from backend |
| Staff name shows "Unknown" in transactions | Old transactions before this update have staff_id=0 |
| Port 8080 blocked (error 10013) | Run as Administrator or add firewall rule |
| Old `library.db` missing columns | Delete `library.db` and restart — tables recreate |

---

*AE Library Management System · C++ OOP Mini Project*
*Backend: C++17 · Boost.Beast · SQLite3 · nlohmann/json*
*Frontend: HTML · Tailwind CSS · Vanilla JS · Material Symbols*
