#include "../include/database.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>

static std::tm safe_localtime(const std::time_t* t) {
    std::tm buf{};
#if defined(_WIN32) || defined(__MINGW32__)
    localtime_s(&buf, t);
#else
    localtime_r(t, &buf);
#endif
    return buf;
}

Database::Database(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error(std::string("Cannot open database: ") + sqlite3_errmsg(db_));
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    std::cout << "[DB] Connected to: " << db_path << "\n";
}

Database::~Database() { if (db_) sqlite3_close(db_); }

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "Unknown";
        sqlite3_free(err);
        throw std::runtime_error("[DB] SQL error: " + msg);
    }
}

std::string Database::generateMembershipNumber() {
    const char* sql = "SELECT COUNT(*) FROM members;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return "LIB" + std::to_string(1001 + count);
}

void Database::initializeTables() {
    // Books — added genre column
    exec("CREATE TABLE IF NOT EXISTS books ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "title TEXT NOT NULL,"
         "author TEXT NOT NULL,"
         "genre TEXT NOT NULL DEFAULT 'General',"
         "quantity INTEGER NOT NULL DEFAULT 1,"
         "available INTEGER NOT NULL DEFAULT 1);");

    // Members
    exec("CREATE TABLE IF NOT EXISTS members ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "membership_number TEXT NOT NULL UNIQUE,"
         "name TEXT NOT NULL,"
         "email TEXT NOT NULL,"
         "phone TEXT NOT NULL,"
         "total_fine REAL NOT NULL DEFAULT 0.0,"
         "joined_date TEXT NOT NULL);");

    // Transactions
    exec("CREATE TABLE IF NOT EXISTS transactions ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "member_id INTEGER NOT NULL,"
         "book_id INTEGER NOT NULL,"
         "issue_date TEXT NOT NULL,"
         "due_date TEXT NOT NULL,"
         "return_date TEXT,"
         "status TEXT NOT NULL DEFAULT 'issued',"
         "fine REAL NOT NULL DEFAULT 0.0,"
         "FOREIGN KEY (member_id) REFERENCES members(id),"
         "FOREIGN KEY (book_id) REFERENCES books(id));");

    // Staff table — separate from members, for portal login
    exec("CREATE TABLE IF NOT EXISTS staff ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "username TEXT NOT NULL UNIQUE,"
         "password TEXT NOT NULL,"
         "name TEXT NOT NULL,"
         "email TEXT NOT NULL,"
         "role TEXT NOT NULL DEFAULT 'librarian',"
         "created_date TEXT NOT NULL);");

    // Settings
    exec("CREATE TABLE IF NOT EXISTS settings ("
         "key TEXT PRIMARY KEY,"
         "value TEXT NOT NULL);");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('fine_rate_per_day','2.0');");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('allowed_days','14');");

    // Seed default staff (admin and librarian) — only if staff table is empty
    exec("INSERT OR IGNORE INTO staff (username,password,name,email,role,created_date) "
         "VALUES ('admin','admin123','System Admin','admin@aelibrary.com','admin',date('now'));");

    std::cout << "[DB] Tables initialized.\n";
}

// ── Books ──────────────────────────────────────────────────────────────────

int Database::addBook(const std::string& title, const std::string& author,
                      const std::string& genre, int quantity) {
    const char* sql = "INSERT INTO books (title,author,genre,quantity,available) VALUES (?,?,?,?,?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,title.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,author.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,genre.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,4,quantity);
    sqlite3_bind_int(stmt,5,quantity);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}

bool Database::removeBook(int book_id) {
    const char* check = "SELECT (quantity-available) FROM books WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, check, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,book_id);
    int issued = 0;
    if (sqlite3_step(stmt)==SQLITE_ROW) issued=sqlite3_column_int(stmt,0);
    sqlite3_finalize(stmt);
    if (issued>0) return false;
    const char* sql = "DELETE FROM books WHERE id=?;";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,book_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt);
    return c>0;
}

std::vector<Book> Database::searchBooks(const std::string& query) {
    std::vector<Book> books;
    const char* sql = "SELECT id,title,author,genre,quantity,available FROM books WHERE title LIKE ? OR author LIKE ? OR genre LIKE ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    std::string p = "%"+query+"%";
    sqlite3_bind_text(stmt,1,p.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,p.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,p.c_str(),-1,SQLITE_TRANSIENT);
    while (sqlite3_step(stmt)==SQLITE_ROW)
        books.emplace_back(sqlite3_column_int(stmt,0),(const char*)sqlite3_column_text(stmt,1),
            (const char*)sqlite3_column_text(stmt,2),(const char*)sqlite3_column_text(stmt,3),
            sqlite3_column_int(stmt,4),sqlite3_column_int(stmt,5));
    sqlite3_finalize(stmt); return books;
}

std::vector<Book> Database::getAllBooks() {
    std::vector<Book> books;
    const char* sql = "SELECT id,title,author,genre,quantity,available FROM books;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW)
        books.emplace_back(sqlite3_column_int(stmt,0),(const char*)sqlite3_column_text(stmt,1),
            (const char*)sqlite3_column_text(stmt,2),(const char*)sqlite3_column_text(stmt,3),
            sqlite3_column_int(stmt,4),sqlite3_column_int(stmt,5));
    sqlite3_finalize(stmt); return books;
}

std::vector<Book> Database::getAvailableBooks() {
    std::vector<Book> books;
    const char* sql = "SELECT id,title,author,genre,quantity,available FROM books WHERE available>0;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW)
        books.emplace_back(sqlite3_column_int(stmt,0),(const char*)sqlite3_column_text(stmt,1),
            (const char*)sqlite3_column_text(stmt,2),(const char*)sqlite3_column_text(stmt,3),
            sqlite3_column_int(stmt,4),sqlite3_column_int(stmt,5));
    sqlite3_finalize(stmt); return books;
}

Book Database::getBookById(int book_id) {
    const char* sql = "SELECT id,title,author,genre,quantity,available FROM books WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,book_id);
    Book book;
    if (sqlite3_step(stmt)==SQLITE_ROW) {
        book.id=sqlite3_column_int(stmt,0); book.title=(const char*)sqlite3_column_text(stmt,1);
        book.author=(const char*)sqlite3_column_text(stmt,2); book.genre=(const char*)sqlite3_column_text(stmt,3);
        book.quantity=sqlite3_column_int(stmt,4); book.available=sqlite3_column_int(stmt,5);
    }
    sqlite3_finalize(stmt); return book;
}

bool Database::updateBookAvailability(int book_id, int delta) {
    const char* sql = "UPDATE books SET available=available+? WHERE id=? AND (available+?)>=0 AND (available+?)<=quantity;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,delta); sqlite3_bind_int(stmt,2,book_id);
    sqlite3_bind_int(stmt,3,delta); sqlite3_bind_int(stmt,4,delta);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

// ── Members ────────────────────────────────────────────────────────────────

int Database::addMember(const std::string& name, const std::string& email, const std::string& phone) {
    auto t = std::time(nullptr); auto tm = safe_localtime(&t);
    std::ostringstream oss; oss << std::put_time(&tm, "%Y-%m-%d");
    std::string today = oss.str();
    std::string mem_num = generateMembershipNumber();
    const char* sql = "INSERT INTO members (membership_number,name,email,phone,total_fine,joined_date) VALUES (?,?,?,?,0.0,?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,mem_num.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,4,phone.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,5,today.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}

bool Database::removeMember(int member_id) {
    const char* check = "SELECT COUNT(*) FROM transactions WHERE member_id=? AND status='issued';";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, check, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,member_id);
    int active=0;
    if (sqlite3_step(stmt)==SQLITE_ROW) active=sqlite3_column_int(stmt,0);
    sqlite3_finalize(stmt);
    if (active>0) return false;
    const char* sql = "DELETE FROM members WHERE id=?;";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,member_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

std::vector<Member> Database::getAllMembers() {
    std::vector<Member> members;
    const char* sql = "SELECT id,membership_number,name,email,phone,total_fine,joined_date FROM members;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW)
        members.emplace_back(sqlite3_column_int(stmt,0),(const char*)sqlite3_column_text(stmt,1),
            (const char*)sqlite3_column_text(stmt,2),(const char*)sqlite3_column_text(stmt,3),
            (const char*)sqlite3_column_text(stmt,4),sqlite3_column_double(stmt,5),
            (const char*)sqlite3_column_text(stmt,6));
    sqlite3_finalize(stmt); return members;
}

Member Database::getMemberById(int member_id) {
    const char* sql = "SELECT id,membership_number,name,email,phone,total_fine,joined_date FROM members WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,member_id);
    Member member;
    if (sqlite3_step(stmt)==SQLITE_ROW) {
        member.id=sqlite3_column_int(stmt,0); member.membership_number=(const char*)sqlite3_column_text(stmt,1);
        member.name=(const char*)sqlite3_column_text(stmt,2); member.email=(const char*)sqlite3_column_text(stmt,3);
        member.phone=(const char*)sqlite3_column_text(stmt,4); member.total_fine=sqlite3_column_double(stmt,5);
        member.joined_date=(const char*)sqlite3_column_text(stmt,6);
    }
    sqlite3_finalize(stmt); return member;
}

bool Database::updateMemberFine(int member_id, double fine_amount) {
    const char* sql = "UPDATE members SET total_fine=total_fine+? WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_double(stmt,1,fine_amount); sqlite3_bind_int(stmt,2,member_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

bool Database::clearMemberFine(int member_id) {
    const char* sql = "UPDATE members SET total_fine=0.0 WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,member_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

// ── Transactions ────────────────────────────────────────────────────────────

int Database::addTransaction(int member_id, int book_id,
                             const std::string& issue_date, const std::string& due_date) {
    const char* sql = "INSERT INTO transactions (member_id,book_id,issue_date,due_date,status,fine) VALUES (?,?,?,?,'issued',0.0);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,member_id); sqlite3_bind_int(stmt,2,book_id);
    sqlite3_bind_text(stmt,3,issue_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,4,due_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}

bool Database::returnBook(int transaction_id, const std::string& return_date, double fine) {
    const char* sql = "UPDATE transactions SET return_date=?,status='returned',fine=? WHERE id=? AND status='issued';";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,return_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt,2,fine); sqlite3_bind_int(stmt,3,transaction_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

static Transaction rowToTransaction(sqlite3_stmt* stmt) {
    Transaction t;
    t.id=sqlite3_column_int(stmt,0); t.member_id=sqlite3_column_int(stmt,1); t.book_id=sqlite3_column_int(stmt,2);
    t.member_name=(const char*)sqlite3_column_text(stmt,3); t.book_title=(const char*)sqlite3_column_text(stmt,4);
    t.issue_date=(const char*)sqlite3_column_text(stmt,5); t.due_date=(const char*)sqlite3_column_text(stmt,6);
    t.return_date=(const char*)sqlite3_column_text(stmt,7); t.status=(const char*)sqlite3_column_text(stmt,8);
    t.fine=sqlite3_column_double(stmt,9);
    return t;
}

std::vector<Transaction> Database::getAllTransactions() {
    std::vector<Transaction> txns;
    const char* sql = "SELECT t.id,t.member_id,t.book_id,m.name,b.title,t.issue_date,t.due_date,IFNULL(t.return_date,''),t.status,t.fine FROM transactions t JOIN members m ON t.member_id=m.id JOIN books b ON t.book_id=b.id ORDER BY t.id DESC;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW) txns.push_back(rowToTransaction(stmt));
    sqlite3_finalize(stmt); return txns;
}

std::vector<Transaction> Database::getActiveTransactions() {
    std::vector<Transaction> txns;
    const char* sql = "SELECT t.id,t.member_id,t.book_id,m.name,b.title,t.issue_date,t.due_date,'',t.status,t.fine FROM transactions t JOIN members m ON t.member_id=m.id JOIN books b ON t.book_id=b.id WHERE t.status='issued' ORDER BY t.due_date ASC;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW) txns.push_back(rowToTransaction(stmt));
    sqlite3_finalize(stmt); return txns;
}

Transaction Database::getTransactionById(int transaction_id) {
    const char* sql = "SELECT t.id,t.member_id,t.book_id,m.name,b.title,t.issue_date,t.due_date,IFNULL(t.return_date,''),t.status,t.fine FROM transactions t JOIN members m ON t.member_id=m.id JOIN books b ON t.book_id=b.id WHERE t.id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,transaction_id);
    Transaction t;
    if (sqlite3_step(stmt)==SQLITE_ROW) t=rowToTransaction(stmt);
    sqlite3_finalize(stmt); return t;
}

// ── Staff ──────────────────────────────────────────────────────────────────

int Database::addStaff(const std::string& username, const std::string& password,
                       const std::string& name, const std::string& email,
                       const std::string& role) {
    auto t = std::time(nullptr); auto tm = safe_localtime(&t);
    std::ostringstream oss; oss << std::put_time(&tm, "%Y-%m-%d");
    std::string today = oss.str();
    const char* sql = "INSERT INTO staff (username,password,name,email,role,created_date) VALUES (?,?,?,?,?,?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,password.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,4,email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,5,role.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,6,today.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}

bool Database::removeStaff(int staff_id) {
    // Prevent removing admin (id=1)
    if (staff_id==1) return false;
    const char* sql = "DELETE FROM staff WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,staff_id);
    sqlite3_step(stmt); int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

std::vector<StaffMember> Database::getAllStaff() {
    std::vector<StaffMember> staff;
    const char* sql = "SELECT id,username,name,email,role,created_date FROM staff ORDER BY id;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt)==SQLITE_ROW) {
        StaffMember s;
        s.id=sqlite3_column_int(stmt,0);
        s.username=(const char*)sqlite3_column_text(stmt,1);
        s.name=(const char*)sqlite3_column_text(stmt,2);
        s.email=(const char*)sqlite3_column_text(stmt,3);
        s.role=(const char*)sqlite3_column_text(stmt,4);
        s.created_date=(const char*)sqlite3_column_text(stmt,5);
        staff.push_back(s);
    }
    sqlite3_finalize(stmt); return staff;
}

bool Database::validateStaff(const std::string& username, const std::string& password,
                              StaffMember& out) {
    const char* sql = "SELECT id,username,name,email,role,created_date FROM staff WHERE username=? AND password=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,password.c_str(),-1,SQLITE_TRANSIENT);
    bool found=false;
    if (sqlite3_step(stmt)==SQLITE_ROW) {
        out.id=sqlite3_column_int(stmt,0);
        out.username=(const char*)sqlite3_column_text(stmt,1);
        out.name=(const char*)sqlite3_column_text(stmt,2);
        out.email=(const char*)sqlite3_column_text(stmt,3);
        out.role=(const char*)sqlite3_column_text(stmt,4);
        out.created_date=(const char*)sqlite3_column_text(stmt,5);
        found=true;
    }
    sqlite3_finalize(stmt); return found;
}

// ── Settings ──────────────────────────────────────────────────────────────

double Database::getFineRate() {
    const char* sql = "SELECT value FROM settings WHERE key='fine_rate_per_day';";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    double rate=2.0;
    if (sqlite3_step(stmt)==SQLITE_ROW) rate=std::stod((const char*)sqlite3_column_text(stmt,0));
    sqlite3_finalize(stmt); return rate;
}

int Database::getAllowedDays() {
    const char* sql = "SELECT value FROM settings WHERE key='allowed_days';";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    int days=14;
    if (sqlite3_step(stmt)==SQLITE_ROW) days=std::stoi((const char*)sqlite3_column_text(stmt,0));
    sqlite3_finalize(stmt); return days;
}

bool Database::setFineRate(double rate) {
    const char* sql = "UPDATE settings SET value=? WHERE key='fine_rate_per_day';";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_double(stmt,1,rate); sqlite3_step(stmt);
    int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}

bool Database::setAllowedDays(int days) {
    const char* sql = "UPDATE settings SET value=? WHERE key='allowed_days';";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,1,days); sqlite3_step(stmt);
    int c=sqlite3_changes(db_); sqlite3_finalize(stmt); return c>0;
}
