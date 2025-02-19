#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <memory>
#include "Sigma.h"
#include "Poly.h"
int PrimeNumber(int n);
int LeviChivita(int i, int j);
bool prime(long int n);
class Sigma;

//Класс для выражения. Возможно основной класс в программе

class Expression {
    friend struct ExpressionHasher;
private:
    //коэффициент
    Poly poly;
    //Список сигм
    std::vector<int> sigmaCoords;
    uint32_t numSigmaX;
    uint32_t numSigmaY;
    int32_t dim; // 0 - размерность не определена (возникает при получении пустого списка из строки)

    bool im;

    void setSigmas1D(const std::vector<Sigma1d>& sigmas);
    void setSigmas2D(const std::vector<Sigma2d>& sigmas);

    std::vector<Sigma1d> getList1D() const;
public:
    Expression() = default;
    //Конструктор от строки
    Expression(const std::string& s);
    //Конструктор от списка множителей и коэффициента, который по умолчанию 1
    Expression(const std::vector<Sigma1d> &v, const Poly& r, bool imm);
    Expression(const std::vector<Sigma2d> &v, const Poly& r, bool imm);
    //Конструктор для загрузки из сериализованного представления
    Expression(const std::vector<uint8_t>& data, size_t& ptr);
    
    void Commute(std::vector <Expression> &O1,  const Expression &O);
    bool operator==(const Expression &ex1) const;
    int getLength() const;
    uint8_t TotalLength();
    uint8_t MaxPoint();
    void MakeMbr(mpz_t mmbr_x, mpz_t mbr_y, mpz_t mbr_z, uint8_t Mp);
    std::vector<std::shared_ptr<Sigma>> getList() const;
    Poly& getPoly();
    const Poly& getPoly() const;
    
    void print() const;
    void serialize(std::vector<uint8_t>& res) const;
};

struct ExpressionHasher {
    size_t operator()(const Expression& expr) const;
};
