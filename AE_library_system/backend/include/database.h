#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "../models/Book.h"
#include "../models/Member.h"
#include "../models/Transaction.h"

struct StaffMember {
    int id;
    std::string username;
    std::string name;
    std::string email;
    std::string role;
    std::string created_date;
    bool first_login;   // true = must change password on first login
    StaffMember() : id(0), first_login(false) {}
};

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void initializeTables();

    // Books
    int  addBook(const std::string& title, const std::string& author,
                 const std::string& genre, int quantity);
    bool removeBook(int book_id);
    std::vector<Book> searchBooks(const std::string& query);
    std::vector<Book> getAllBooks();
    std::vector<Book> getAvailableBooks();
    Book getBookById(int book_id);
    bool updateBookAvailability(int book_id, int delta);

    // Members
    int  addMember(const std::string& name, const std::string& email,
                   const std::string& phone);
    bool removeMember(int member_id);
    std::vector<Member> getAllMembers();
    Member getMemberById(int member_id);
    bool updateMemberFine(int member_id, double fine_amount);
    bool clearMemberFine(int member_id);

    // Transactions — now include staff_id
    int  addTransaction(int member_id, int book_id,
                        const std::string& issue_date,
                        const std::string& due_date,
                        int staff_id = 0);
    bool returnBook(int transaction_id, const std::string& return_date,
                    double fine, int staff_id = 0);
    std::vector<Transaction> getAllTransactions();
    std::vector<Transaction> getActiveTransactions();
    Transaction getTransactionById(int transaction_id);

    // Staff management
    int  addStaff(const std::string& username, const std::string& password,
                  const std::string& name, const std::string& email,
                  const std::string& role);
    bool removeStaff(int staff_id);
    std::vector<StaffMember> getAllStaff();
    bool validateStaff(const std::string& username, const std::string& password,
                       StaffMember& out_member);
    // Admin username change
    bool updateAdminUsername(const std::string& new_username, const std::string& password);

    // Staff profile update (by staff themselves)
    bool updateStaffProfile(int staff_id, const std::string& email,
                            const std::string& new_password);
    // Mark first_login as done
    bool clearFirstLogin(int staff_id);

    // Settings (includes admin profile)
    double getFineRate();
    int    getAllowedDays();
    bool   setFineRate(double rate);
    bool   setAllowedDays(int days);
    std::string getSetting(const std::string& key, const std::string& default_val = "");
    bool   setSetting(const std::string& key, const std::string& value);

private:
    sqlite3* db_;
    void exec(const std::string& sql);
    std::string generateMembershipNumber();
};
