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
    exec("CREATE TABLE IF NOT EXISTS books ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "title TEXT NOT NULL, author TEXT NOT NULL,"
         "genre TEXT NOT NULL DEFAULT 'General',"
         "quantity INTEGER NOT NULL DEFAULT 1,"
         "available INTEGER NOT NULL DEFAULT 1);");

    exec("CREATE TABLE IF NOT EXISTS members ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "membership_number TEXT NOT NULL UNIQUE,"
         "name TEXT NOT NULL, email TEXT NOT NULL, phone TEXT NOT NULL,"
         "total_fine REAL NOT NULL DEFAULT 0.0,"
         "joined_date TEXT NOT NULL);");

    // transactions: staff_id column tracks who handled it
    exec("CREATE TABLE IF NOT EXISTS transactions ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "member_id INTEGER NOT NULL,"
         "book_id INTEGER NOT NULL,"
         "issue_date TEXT NOT NULL,"
         "due_date TEXT NOT NULL,"
         "return_date TEXT,"
         "status TEXT NOT NULL DEFAULT 'issued',"
         "fine REAL NOT NULL DEFAULT 0.0,"
         "staff_id INTEGER NOT NULL DEFAULT 0,"
         "FOREIGN KEY (member_id) REFERENCES members(id),"
         "FOREIGN KEY (book_id) REFERENCES books(id));");

    // Migrate existing transactions table if staff_id column missing
    // sqlite3_exec used directly so errors (duplicate column) are silently ignored
    sqlite3_exec(db_,
        "ALTER TABLE transactions ADD COLUMN staff_id INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);

    // staff: first_login flag
    exec("CREATE TABLE IF NOT EXISTS staff ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "username TEXT NOT NULL UNIQUE,"
         "password TEXT NOT NULL,"
         "name TEXT NOT NULL,"
         "email TEXT NOT NULL,"
         "role TEXT NOT NULL DEFAULT 'librarian',"
         "created_date TEXT NOT NULL,"
         "first_login INTEGER NOT NULL DEFAULT 0);");

    // Migrate existing staff table if first_login missing
    sqlite3_exec(db_,
        "ALTER TABLE staff ADD COLUMN first_login INTEGER NOT NULL DEFAULT 0;",
        nullptr, nullptr, nullptr);

    // Settings
    exec("CREATE TABLE IF NOT EXISTS settings ("
         "key TEXT PRIMARY KEY, value TEXT NOT NULL);");

    // Default settings
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('fine_rate_per_day','2.0');");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('allowed_days','14');");
    // Admin profile stored in settings table (persistent)
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('admin_name','Admin');");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('admin_email','admin@aelibrary.com');");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('admin_password','admin123');");
    exec("INSERT OR IGNORE INTO settings (key,value) VALUES ('admin_username','admin');");

    // Seed admin — first_login=0 (admin never forced to change password)
    exec("INSERT OR IGNORE INTO staff "
         "(username,password,name,email,role,created_date,first_login) "
         "VALUES ('admin','admin123','Admin','admin@aelibrary.com','admin',date('now'),0);");

    std::cout << "[DB] Tables initialized.\n";
}

// ── Books ──────────────────────────────────────────────────────────────────
int Database::addBook(const std::string& t, const std::string& a, const std::string& g, int q) {
    const char* sql="INSERT INTO books (title,author,genre,quantity,available) VALUES (?,?,?,?,?);";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_,sql,-1,&stmt,nullptr);
    sqlite3_bind_text(stmt,1,t.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,a.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,g.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,4,q); sqlite3_bind_int(stmt,5,q);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}
bool Database::removeBook(int id) {
    const char* chk="SELECT (quantity-available) FROM books WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,chk,-1,&s,nullptr);
    sqlite3_bind_int(s,1,id); int iss=0;
    if(sqlite3_step(s)==SQLITE_ROW) iss=sqlite3_column_int(s,0); sqlite3_finalize(s);
    if(iss>0) return false;
    const char* sql="DELETE FROM books WHERE id=?;";
    sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}
std::vector<Book> Database::searchBooks(const std::string& q) {
    std::vector<Book> v;
    const char* sql="SELECT id,title,author,genre,quantity,available FROM books WHERE title LIKE ? OR author LIKE ? OR genre LIKE ?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    std::string p="%"+q+"%";
    sqlite3_bind_text(s,1,p.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,p.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,p.c_str(),-1,SQLITE_TRANSIENT);
    while(sqlite3_step(s)==SQLITE_ROW) v.emplace_back(sqlite3_column_int(s,0),(const char*)sqlite3_column_text(s,1),(const char*)sqlite3_column_text(s,2),(const char*)sqlite3_column_text(s,3),sqlite3_column_int(s,4),sqlite3_column_int(s,5));
    sqlite3_finalize(s); return v;
}
std::vector<Book> Database::getAllBooks() {
    std::vector<Book> v;
    const char* sql="SELECT id,title,author,genre,quantity,available FROM books;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) v.emplace_back(sqlite3_column_int(s,0),(const char*)sqlite3_column_text(s,1),(const char*)sqlite3_column_text(s,2),(const char*)sqlite3_column_text(s,3),sqlite3_column_int(s,4),sqlite3_column_int(s,5));
    sqlite3_finalize(s); return v;
}
std::vector<Book> Database::getAvailableBooks() {
    std::vector<Book> v;
    const char* sql="SELECT id,title,author,genre,quantity,available FROM books WHERE available>0;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) v.emplace_back(sqlite3_column_int(s,0),(const char*)sqlite3_column_text(s,1),(const char*)sqlite3_column_text(s,2),(const char*)sqlite3_column_text(s,3),sqlite3_column_int(s,4),sqlite3_column_int(s,5));
    sqlite3_finalize(s); return v;
}
Book Database::getBookById(int id) {
    const char* sql="SELECT id,title,author,genre,quantity,available FROM books WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    Book b;
    if(sqlite3_step(s)==SQLITE_ROW){b.id=sqlite3_column_int(s,0);b.title=(const char*)sqlite3_column_text(s,1);b.author=(const char*)sqlite3_column_text(s,2);b.genre=(const char*)sqlite3_column_text(s,3);b.quantity=sqlite3_column_int(s,4);b.available=sqlite3_column_int(s,5);}
    sqlite3_finalize(s); return b;
}
bool Database::updateBookAvailability(int id, int delta) {
    const char* sql="UPDATE books SET available=available+? WHERE id=? AND (available+?)>=0 AND (available+?)<=quantity;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_int(s,1,delta);sqlite3_bind_int(s,2,id);sqlite3_bind_int(s,3,delta);sqlite3_bind_int(s,4,delta);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}

// ── Members ────────────────────────────────────────────────────────────────
int Database::addMember(const std::string& name, const std::string& email, const std::string& phone) {
    auto t=std::time(nullptr); auto tm=safe_localtime(&t);
    std::ostringstream oss; oss<<std::put_time(&tm,"%Y-%m-%d");
    std::string mem_num=generateMembershipNumber();
    const char* sql="INSERT INTO members (membership_number,name,email,phone,total_fine,joined_date) VALUES (?,?,?,?,0.0,?);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,mem_num.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,phone.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,5,oss.str().c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db_);
}
bool Database::removeMember(int id) {
    const char* chk="SELECT COUNT(*) FROM transactions WHERE member_id=? AND status='issued';";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,chk,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    int active=0; if(sqlite3_step(s)==SQLITE_ROW) active=sqlite3_column_int(s,0); sqlite3_finalize(s);
    if(active>0) return false;
    const char* sql="DELETE FROM members WHERE id=?;";
    sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}
std::vector<Member> Database::getAllMembers() {
    std::vector<Member> v;
    const char* sql="SELECT id,membership_number,name,email,phone,total_fine,joined_date FROM members;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) v.emplace_back(sqlite3_column_int(s,0),(const char*)sqlite3_column_text(s,1),(const char*)sqlite3_column_text(s,2),(const char*)sqlite3_column_text(s,3),(const char*)sqlite3_column_text(s,4),sqlite3_column_double(s,5),(const char*)sqlite3_column_text(s,6));
    sqlite3_finalize(s); return v;
}
Member Database::getMemberById(int id) {
    const char* sql="SELECT id,membership_number,name,email,phone,total_fine,joined_date FROM members WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    Member m;
    if(sqlite3_step(s)==SQLITE_ROW){m.id=sqlite3_column_int(s,0);m.membership_number=(const char*)sqlite3_column_text(s,1);m.name=(const char*)sqlite3_column_text(s,2);m.email=(const char*)sqlite3_column_text(s,3);m.phone=(const char*)sqlite3_column_text(s,4);m.total_fine=sqlite3_column_double(s,5);m.joined_date=(const char*)sqlite3_column_text(s,6);}
    sqlite3_finalize(s); return m;
}
bool Database::updateMemberFine(int id, double amt) {
    const char* sql="UPDATE members SET total_fine=total_fine+? WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_double(s,1,amt); sqlite3_bind_int(s,2,id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}
bool Database::clearMemberFine(int id) {
    const char* sql="UPDATE members SET total_fine=0.0 WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}

// ── Transactions ────────────────────────────────────────────────────────────
int Database::addTransaction(int member_id, int book_id,
                             const std::string& issue_date, const std::string& due_date,
                             int staff_id) {
    const char* sql="INSERT INTO transactions (member_id,book_id,issue_date,due_date,status,fine,staff_id) VALUES (?,?,?,?,'issued',0.0,?);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_int(s,1,member_id); sqlite3_bind_int(s,2,book_id);
    sqlite3_bind_text(s,3,issue_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,due_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,5,staff_id);
    sqlite3_step(s); sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db_);
}
bool Database::returnBook(int txn_id, const std::string& ret_date, double fine, int staff_id) {
    const char* sql="UPDATE transactions SET return_date=?,status='returned',fine=?,staff_id=CASE WHEN staff_id=0 THEN ? ELSE staff_id END WHERE id=? AND status='issued';";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,ret_date.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(s,2,fine); sqlite3_bind_int(s,3,staff_id); sqlite3_bind_int(s,4,txn_id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}

// JOIN with staff table to get staff_name
static Transaction rowToTxn(sqlite3_stmt* s) {
    Transaction t;
    t.id=sqlite3_column_int(s,0); t.member_id=sqlite3_column_int(s,1); t.book_id=sqlite3_column_int(s,2);
    t.member_name=(const char*)sqlite3_column_text(s,3); t.book_title=(const char*)sqlite3_column_text(s,4);
    t.issue_date=(const char*)sqlite3_column_text(s,5); t.due_date=(const char*)sqlite3_column_text(s,6);
    t.return_date=(const char*)sqlite3_column_text(s,7); t.status=(const char*)sqlite3_column_text(s,8);
    t.fine=sqlite3_column_double(s,9); t.staff_id=sqlite3_column_int(s,10);
    auto sn=sqlite3_column_text(s,11);
    t.staff_name = sn ? (const char*)sn : "Unknown";
    return t;
}
static const char* TXN_QUERY =
    "SELECT t.id,t.member_id,t.book_id,m.name,b.title,"
    "t.issue_date,t.due_date,IFNULL(t.return_date,''),t.status,t.fine,"
    "t.staff_id,IFNULL(sf.name,'Unknown') "
    "FROM transactions t "
    "JOIN members m ON t.member_id=m.id "
    "JOIN books b ON t.book_id=b.id "
    "LEFT JOIN staff sf ON t.staff_id=sf.id ";

std::vector<Transaction> Database::getAllTransactions() {
    std::vector<Transaction> v;
    std::string sql = std::string(TXN_QUERY) + "ORDER BY t.id DESC;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql.c_str(),-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) v.push_back(rowToTxn(s));
    sqlite3_finalize(s); return v;
}
std::vector<Transaction> Database::getActiveTransactions() {
    std::vector<Transaction> v;
    std::string sql = std::string(TXN_QUERY) + "WHERE t.status='issued' ORDER BY t.due_date ASC;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql.c_str(),-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) v.push_back(rowToTxn(s));
    sqlite3_finalize(s); return v;
}
Transaction Database::getTransactionById(int id) {
    std::string sql = std::string(TXN_QUERY) + "WHERE t.id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql.c_str(),-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    Transaction t;
    if(sqlite3_step(s)==SQLITE_ROW) t=rowToTxn(s);
    sqlite3_finalize(s); return t;
}

// ── Staff ──────────────────────────────────────────────────────────────────
int Database::addStaff(const std::string& username, const std::string& password,
                       const std::string& name, const std::string& email, const std::string& role) {
    auto t=std::time(nullptr); auto tm=safe_localtime(&t);
    std::ostringstream oss; oss<<std::put_time(&tm,"%Y-%m-%d");
    // New staff: first_login=1 (must change password)
    const char* sql="INSERT INTO staff (username,password,name,email,role,created_date,first_login) VALUES (?,?,?,?,?,?,1);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,password.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,name.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,5,role.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,6,oss.str().c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db_);
}
bool Database::removeStaff(int id) {
    if(id==1) return false;
    const char* sql="DELETE FROM staff WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}
std::vector<StaffMember> Database::getAllStaff() {
    std::vector<StaffMember> v;
    const char* sql="SELECT id,username,name,email,role,created_date,first_login FROM staff ORDER BY id;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    while(sqlite3_step(s)==SQLITE_ROW) {
        StaffMember m;
        m.id=sqlite3_column_int(s,0); m.username=(const char*)sqlite3_column_text(s,1);
        m.name=(const char*)sqlite3_column_text(s,2); m.email=(const char*)sqlite3_column_text(s,3);
        m.role=(const char*)sqlite3_column_text(s,4); m.created_date=(const char*)sqlite3_column_text(s,5);
        m.first_login=sqlite3_column_int(s,6)!=0;
        v.push_back(m);
    }
    sqlite3_finalize(s); return v;
}
bool Database::validateStaff(const std::string& username, const std::string& password, StaffMember& out) {
    const char* sql="SELECT id,username,name,email,role,created_date,first_login FROM staff WHERE username=? AND password=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,password.c_str(),-1,SQLITE_TRANSIENT);
    bool found=false;
    if(sqlite3_step(s)==SQLITE_ROW){
        out.id=sqlite3_column_int(s,0); out.username=(const char*)sqlite3_column_text(s,1);
        out.name=(const char*)sqlite3_column_text(s,2); out.email=(const char*)sqlite3_column_text(s,3);
        out.role=(const char*)sqlite3_column_text(s,4); out.created_date=(const char*)sqlite3_column_text(s,5);
        out.first_login=sqlite3_column_int(s,6)!=0;
        found=true;
    }
    sqlite3_finalize(s); return found;
}
bool Database::updateStaffProfile(int staff_id, const std::string& email, const std::string& new_password) {
    bool ok=false;
    if (!email.empty()) {
        const char* sql="UPDATE staff SET email=? WHERE id=?;";
        sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
        sqlite3_bind_text(s,1,email.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(s,2,staff_id);
        sqlite3_step(s); ok=(sqlite3_changes(db_)>0); sqlite3_finalize(s);
    }
    if (!new_password.empty()) {
        const char* sql="UPDATE staff SET password=? WHERE id=?;";
        sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
        sqlite3_bind_text(s,1,new_password.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(s,2,staff_id);
        sqlite3_step(s); ok=(sqlite3_changes(db_)>0); sqlite3_finalize(s);
    }
    return ok;
}
bool Database::clearFirstLogin(int staff_id) {
    const char* sql="UPDATE staff SET first_login=0 WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr); sqlite3_bind_int(s,1,staff_id);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}

// Update admin username in staff table
bool Database::updateAdminUsername(const std::string& new_username, const std::string& password) {
    // Verify password first
    const char* chk = "SELECT COUNT(*) FROM staff WHERE id=1 AND password=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_, chk, -1, &s, nullptr);
    sqlite3_bind_text(s, 1, password.c_str(), -1, SQLITE_TRANSIENT);
    int ok = 0;
    if (sqlite3_step(s) == SQLITE_ROW) ok = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    if (!ok) return false; // wrong password

    // Check username not already taken
    const char* chk2 = "SELECT COUNT(*) FROM staff WHERE username=? AND id!=1;";
    sqlite3_prepare_v2(db_, chk2, -1, &s, nullptr);
    sqlite3_bind_text(s, 1, new_username.c_str(), -1, SQLITE_TRANSIENT);
    int taken = 0;
    if (sqlite3_step(s) == SQLITE_ROW) taken = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    if (taken) return false; // username taken

    const char* sql = "UPDATE staff SET username=? WHERE id=1;";
    sqlite3_prepare_v2(db_, sql, -1, &s, nullptr);
    sqlite3_bind_text(s, 1, new_username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(s); int c = sqlite3_changes(db_); sqlite3_finalize(s);

    // Also store in settings for reference
    if (c > 0) setSetting("admin_username", new_username);
    return c > 0;
}

// ── Settings ──────────────────────────────────────────────────────────────
std::string Database::getSetting(const std::string& key, const std::string& def) {
    const char* sql="SELECT value FROM settings WHERE key=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,key.c_str(),-1,SQLITE_TRANSIENT);
    std::string val=def;
    if(sqlite3_step(s)==SQLITE_ROW) val=(const char*)sqlite3_column_text(s,0);
    sqlite3_finalize(s); return val;
}
bool Database::setSetting(const std::string& key, const std::string& value) {
    const char* sql="INSERT OR REPLACE INTO settings (key,value) VALUES (?,?);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db_,sql,-1,&s,nullptr);
    sqlite3_bind_text(s,1,key.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,value.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(s); int c=sqlite3_changes(db_); sqlite3_finalize(s); return c>0;
}
double Database::getFineRate() { return std::stod(getSetting("fine_rate_per_day","2.0")); }
int    Database::getAllowedDays() { return std::stoi(getSetting("allowed_days","14")); }
bool   Database::setFineRate(double r) { return setSetting("fine_rate_per_day",std::to_string(r)); }
bool   Database::setAllowedDays(int d) { return setSetting("allowed_days",std::to_string(d)); }
