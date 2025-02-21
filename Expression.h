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
#include <functional>
#include "Sigma.h"
#include "Poly.h"

int LeviChivita(int i, int j);

//Класс для выражения. Возможно основной класс в программе

class Expression {
    friend struct ExpressionHasher;
    
    template<size_t D>
    friend void commuteImpl(const Expression& O1, const Expression& O2, std::function<void(Expression&& newExpression)> callback);
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

    template<size_t D>
    std::vector<Sigma<D>> getList() const;
public:
    Expression() = default;
    //Конструктор от строки
    Expression(const std::string& s);
    //Конструктор от списка множителей и коэффициента, который по умолчанию 1
    Expression(const std::vector<Sigma1d> &v, const Poly& r, bool imm);
    Expression(const std::vector<Sigma2d> &v, const Poly& r, bool imm);
    //Конструктор для загрузки из сериализованного представления
    Expression(const std::vector<uint8_t>& data, size_t& ptr);
    
    void Commute(const Expression& O, std::function<void(Expression&& newExpression)> callback);
    bool operator==(const Expression &ex1) const;
    Poly& getPoly();
    const Poly& getPoly() const;
    
    void print() const;
    void serialize(std::vector<uint8_t>& res) const;
};

struct ExpressionHasher {
    size_t operator()(const Expression& expr) const;
};
