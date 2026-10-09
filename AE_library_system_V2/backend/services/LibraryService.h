#pragma once
#include "../include/database.h"
#include <string>
#include <map>

class LibraryService {
public:
    explicit LibraryService(Database& db);

    std::map<std::string,std::string> addBook(const std::string& title, const std::string& author, const std::string& genre, int quantity);
    std::map<std::string,std::string> removeBook(int book_id);
    std::vector<Book> searchBooks(const std::string& query);
    std::vector<Book> getAllBooks();
    std::vector<Book> getAvailableBooks();

    std::map<std::string,std::string> addMember(const std::string& name, const std::string& email, const std::string& phone);
    std::map<std::string,std::string> removeMember(int member_id);
    std::vector<Member> getAllMembers();

    std::map<std::string,std::string> issueBook(int member_id, int book_id, const std::string& custom_due="", int staff_id=0);
    std::map<std::string,std::string> returnBook(int transaction_id, int staff_id=0);
    std::vector<Transaction> getAllTransactions();
    std::vector<Transaction> getActiveTransactions();

    std::map<std::string,std::string> clearFine(int member_id);

    std::map<std::string,std::string> addStaff(const std::string& username, const std::string& password, const std::string& name, const std::string& email, const std::string& role);
    std::map<std::string,std::string> removeStaff(int staff_id);
    std::vector<StaffMember> getAllStaff();
    std::map<std::string,std::string> validateStaffLogin(const std::string& username, const std::string& password);
    std::map<std::string,std::string> updateStaffProfile(int staff_id, const std::string& email, const std::string& new_password);
    std::map<std::string,std::string> clearFirstLogin(int staff_id);

    // Admin profile (persistent via settings table)
    std::map<std::string,std::string> getAdminProfile();
    std::map<std::string,std::string> updateAdminProfile(const std::string& name, const std::string& email, const std::string& new_password, const std::string& current_password);
    std::map<std::string,std::string> updateAdminUsername(const std::string& new_username, const std::string& password);

    std::map<std::string,std::string> setFineRate(double rate);
    std::map<std::string,std::string> setAllowedDays(int days);
    double getFineRate();
    int    getAllowedDays();

private:
    Database& db_;
    int  calculateOverdueDays(const std::string& due_date);
    std::string getTodayDate();
    std::string addDays(const std::string& date, int days);
};
