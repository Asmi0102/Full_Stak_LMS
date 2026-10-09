#pragma once
#include <string>

struct Member {
    int id;
    std::string membership_number;
    std::string name;
    std::string email;
    std::string phone;
    double total_fine;
    std::string joined_date;

    Member() : id(0), total_fine(0.0) {}

    Member(int id, std::string membership_number, std::string name,
           std::string email, std::string phone, double total_fine,
           std::string joined_date)
        : id(id), membership_number(std::move(membership_number)),
          name(std::move(name)), email(std::move(email)),
          phone(std::move(phone)), total_fine(total_fine),
          joined_date(std::move(joined_date)) {}
};
