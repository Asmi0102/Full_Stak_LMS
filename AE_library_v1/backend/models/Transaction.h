#pragma once
#include <string>

/**
 * @brief Represents a book issue/return transaction.
 * Tracks who borrowed what, when it was returned, and any fines.
 */
struct Transaction {
    int id;
    int member_id;
    int book_id;
    std::string member_name;   // Denormalized for easy display
    std::string book_title;    // Denormalized for easy display
    std::string issue_date;
    std::string due_date;      // Calculated from issue_date + allowed_days
    std::string return_date;   // Empty if still issued
    std::string status;        // "issued" or "returned"
    double fine;               // Fine charged for late return

    Transaction() : id(0), member_id(0), book_id(0), fine(0.0) {}
};
