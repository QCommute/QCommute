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
#include <memory>
#include <gmpxx.h>
#include "Expression.h"
class Expression;

class Monom
{
private:
    std::vector<uint8_t> pows; //список степеней
    mpz_t coef; //коэффициент
public:
    //Конструктор от простого оператора и аргумента
    //Monom(const std::vector<int> pows_, int coef_);
    //Конструктор от строки
    bool operator==(const Monom &mn);
    Monom(const Monom &mn);
    Monom(const std::string &s, int pos, int &npos);
    Monom operator* (const Monom &mn);
    //Monom operator+ (const Monom &mn);
    //Перегрузка конструктора копирования (последние 2 важны т.к. мы храним список указателей и при копировании недостаточно просто скопировать указатели, нужно копировать память на которую они ссылаются. Конструктор копирования и оператор присваивания недостаточно умны чтобы это делать, поэтому их приходится переопределять и перегружать самостоятельно)
    void print();
    bool isZero();
    void Prod(const Monom &mn);
    void Sum(const Monom &mn);
    void Prod2(int n);
    //std::vector<int> getPows() const;
    void setCoef(mpz_t c_);
   //void setPoint(int p_);
    ~Monom();
};

void deepCopyMonom(std::shared_ptr<Monom> &to, std::shared_ptr<Monom> from);
