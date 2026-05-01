#include "LibraryService.h"
#include <sstream>
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

LibraryService::LibraryService(Database& db) : db_(db) {}

std::string LibraryService::getTodayDate() {
    auto t=std::time(nullptr); auto tm=safe_localtime(&t);
    std::ostringstream oss; oss<<std::put_time(&tm,"%Y-%m-%d"); return oss.str();
}

std::string LibraryService::addDays(const std::string& date, int days) {
    std::tm tm{}; std::istringstream ss(date); ss>>std::get_time(&tm,"%Y-%m-%d");
    tm.tm_mday+=days; std::mktime(&tm);
    std::ostringstream oss; oss<<std::put_time(&tm,"%Y-%m-%d"); return oss.str();
}

int LibraryService::calculateOverdueDays(const std::string& due_date) {
    std::tm due_tm{}; std::istringstream ss(due_date); ss>>std::get_time(&due_tm,"%Y-%m-%d");
    due_tm.tm_hour=0; due_tm.tm_min=0; due_tm.tm_sec=0; due_tm.tm_isdst=-1;
    auto t=std::time(nullptr); auto now_tm=safe_localtime(&t);
    now_tm.tm_hour=0; now_tm.tm_min=0; now_tm.tm_sec=0; now_tm.tm_isdst=-1;
    std::time_t due_t=std::mktime(&due_tm), now_t=std::mktime(&now_tm);
    int d=static_cast<int>(std::difftime(now_t,due_t)/86400);
    return d>0?d:0;
}

std::map<std::string,std::string> LibraryService::addBook(
    const std::string& title, const std::string& author,
    const std::string& genre, int quantity) {
    if (title.empty()||author.empty()||genre.empty()||quantity<=0)
        return {{"success","false"},{"message","Title, author, genre required and quantity > 0"}};
    int id=db_.addBook(title,author,genre,quantity);
    return {{"success","true"},{"message","Book added"},{"id",std::to_string(id)}};
}

std::map<std::string,std::string> LibraryService::removeBook(int book_id) {
    if (book_id<=0) return {{"success","false"},{"message","Invalid book ID"}};
    if (!db_.removeBook(book_id))
        return {{"success","false"},{"message","Cannot remove — copies issued or not found"}};
    return {{"success","true"},{"message","Book removed"}};
}

std::vector<Book> LibraryService::searchBooks(const std::string& q) { return db_.searchBooks(q); }
std::vector<Book> LibraryService::getAllBooks()       { return db_.getAllBooks(); }
std::vector<Book> LibraryService::getAvailableBooks() { return db_.getAvailableBooks(); }

std::map<std::string,std::string> LibraryService::addMember(
    const std::string& name, const std::string& email, const std::string& phone) {
    if (name.empty()||email.empty()||phone.empty())
        return {{"success","false"},{"message","Name, email and phone are required"}};
    int id=db_.addMember(name,email,phone);
    Member m=db_.getMemberById(id);
    return {{"success","true"},{"message","Member registered"},
            {"id",std::to_string(id)},{"membership_number",m.membership_number}};
}

std::map<std::string,std::string> LibraryService::removeMember(int member_id) {
    if (member_id<=0) return {{"success","false"},{"message","Invalid member ID"}};
    if (!db_.removeMember(member_id))
        return {{"success","false"},{"message","Cannot remove — active loans or not found"}};
    return {{"success","true"},{"message","Member removed"}};
}

std::vector<Member> LibraryService::getAllMembers() { return db_.getAllMembers(); }

std::map<std::string,std::string> LibraryService::issueBook(
    int member_id, int book_id, const std::string& custom_due_date) {
    Member m=db_.getMemberById(member_id);
    if (m.id==0) return {{"success","false"},{"message","Member not found"}};
    Book b=db_.getBookById(book_id);
    if (b.id==0) return {{"success","false"},{"message","Book not found"}};
    if (b.available<=0) return {{"success","false"},{"message","No copies available: "+b.title}};

    std::string issue_d=getTodayDate();
    std::string due_d;
    if (!custom_due_date.empty() && custom_due_date.size()==10)
        due_d=custom_due_date;
    else
        due_d=addDays(issue_d, db_.getAllowedDays());

    if (!db_.updateBookAvailability(book_id,-1))
        return {{"success","false"},{"message","Failed to update availability"}};

    int txn_id=db_.addTransaction(member_id,book_id,issue_d,due_d);
    return {{"success","true"},{"message","Book issued"},
            {"transaction_id",std::to_string(txn_id)},
            {"issue_date",issue_d},{"due_date",due_d},
            {"member",m.name},{"book",b.title}};
}

std::map<std::string,std::string> LibraryService::returnBook(int transaction_id) {
    Transaction txn=db_.getTransactionById(transaction_id);
    if (txn.id==0) return {{"success","false"},{"message","Transaction not found"}};
    if (txn.status=="returned") return {{"success","false"},{"message","Already returned"}};

    int overdue_days=calculateOverdueDays(txn.due_date);
    double fine=overdue_days*db_.getFineRate();
    std::string ret_d=getTodayDate();

    if (!db_.returnBook(transaction_id,ret_d,fine))
        return {{"success","false"},{"message","Failed to update transaction"}};

    db_.updateBookAvailability(txn.book_id,+1);
    if (fine>0) db_.updateMemberFine(txn.member_id,fine);

    return {{"success","true"},{"message","Book returned"},
            {"return_date",ret_d},{"overdue_days",std::to_string(overdue_days)},
            {"fine",std::to_string(fine)},{"book",txn.book_title},{"member",txn.member_name}};
}

std::vector<Transaction> LibraryService::getAllTransactions()    { return db_.getAllTransactions(); }
std::vector<Transaction> LibraryService::getActiveTransactions() { return db_.getActiveTransactions(); }

std::map<std::string,std::string> LibraryService::clearFine(int member_id) {
    Member m=db_.getMemberById(member_id);
    if (m.id==0) return {{"success","false"},{"message","Member not found"}};
    db_.clearMemberFine(member_id);
    return {{"success","true"},{"message","Fine cleared for "+m.name}};
}

std::map<std::string,std::string> LibraryService::addStaff(
    const std::string& username, const std::string& password,
    const std::string& name, const std::string& email, const std::string& role) {
    if (username.empty()||password.empty()||name.empty())
        return {{"success","false"},{"message","Username, password and name are required"}};
    int id=db_.addStaff(username,password,name,email,role);
    return {{"success","true"},{"message","Staff added"},{"id",std::to_string(id)},{"username",username}};
}

std::map<std::string,std::string> LibraryService::removeStaff(int staff_id) {
    if (staff_id==1) return {{"success","false"},{"message","Cannot remove the system admin"}};
    if (!db_.removeStaff(staff_id)) return {{"success","false"},{"message","Staff not found"}};
    return {{"success","true"},{"message","Staff removed"}};
}

std::vector<StaffMember> LibraryService::getAllStaff() { return db_.getAllStaff(); }

std::map<std::string,std::string> LibraryService::validateStaffLogin(
    const std::string& username, const std::string& password) {
    StaffMember s;
    if (!db_.validateStaff(username,password,s))
        return {{"success","false"},{"message","Invalid credentials"}};
    return {{"success","true"},{"message","Login successful"},
            {"id",std::to_string(s.id)},{"name",s.name},
            {"role",s.role},{"username",s.username}};
}

std::map<std::string,std::string> LibraryService::setFineRate(double rate) {
    if (rate<0) return {{"success","false"},{"message","Rate must be >= 0"}};
    db_.setFineRate(rate);
    return {{"success","true"},{"message","Fine rate updated"},{"fine_rate_per_day",std::to_string(rate)}};
}

std::map<std::string,std::string> LibraryService::setAllowedDays(int days) {
    if (days<=0) return {{"success","false"},{"message","Days must be > 0"}};
    db_.setAllowedDays(days);
    return {{"success","true"},{"message","Allowed days updated"},{"allowed_days",std::to_string(days)}};
}

double LibraryService::getFineRate()    { return db_.getFineRate(); }
int    LibraryService::getAllowedDays() { return db_.getAllowedDays(); }
