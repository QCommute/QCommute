#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <gmpxx.h>

class Monom {
private:
    std::vector<uint8_t> pows; //список степеней
    mpz_t coef; //коэффициент
public:
    Monom(const std::string &s);
    Monom(const Monom& other);
    Monom& operator=(const Monom& other);
    Monom(Monom&& other);
    Monom& operator=(Monom&& other);

    bool operator==(const Monom& other) const;
    bool isZero() const;
    bool samePowers(const Monom& other) const;
    bool lessPowers(const Monom& other) const;
    
    void Sum(const Monom &mn);
    Monom& operator*=(const Monom& other);
    Monom& operator*=(int n);

    //std::vector<int> getPows() const;
    void setCoef(mpz_t c_);
    void getCoef(mpz_t& res) const;
    //void setPoint(int p_);
    void print() const;
    ~Monom();
};

