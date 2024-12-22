#pragma once

#include <vector>
#include <memory>
#include "Monom.h"

class Poly {
private:
    std::vector<std::shared_ptr<Monom>> data_;
public:
    void addMonom(std::shared_ptr<Monom> monom);
    std::vector<std::shared_ptr<Monom>> getData();
    bool isZero() const;
    void print() const;

    void deepCopyFrom(const Poly& other);

    Poly& operator+=(const Poly& other);
    Poly operator*(const Poly& other) const;
};