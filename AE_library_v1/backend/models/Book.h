#pragma once
#include <string>

struct Book {
    int id;
    std::string title;
    std::string author;
    std::string genre;
    int quantity;
    int available;

    Book() : id(0), quantity(0), available(0) {}

    Book(int id, std::string title, std::string author, std::string genre, int quantity, int available)
        : id(id), title(std::move(title)), author(std::move(author)),
          genre(std::move(genre)), quantity(quantity), available(available) {}
};
