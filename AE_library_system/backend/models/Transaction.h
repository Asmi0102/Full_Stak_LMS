#pragma once
#include <string>

struct Transaction {
    int id;
    int member_id;
    int book_id;
    std::string member_name;
    std::string book_title;
    std::string issue_date;
    std::string due_date;
    std::string return_date;
    std::string status;
    double fine;
    int staff_id;           // ID of staff who handled this transaction
    std::string staff_name; // Name of staff who handled this transaction

    Transaction() : id(0), member_id(0), book_id(0), fine(0.0), staff_id(0) {}
};
