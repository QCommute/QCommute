#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <functional>
#include "Sigma.h"
#include "Poly.h"

int LeviChivita(int i, int j);

// Class representing an expression. Likely the central class of the program.

class Expression {
    friend struct ExpressionHasher;
    
    template<size_t D>
    friend void commuteImpl(const Expression& O1, const Expression& O2, std::function<void(Expression&& newExpression)> callback);
private:
    // polynomial coefficient
    Poly poly;
    // list of Pauli operator coordinates
    std::vector<int> sigmaCoords;
    uint32_t numSigmaX;
    uint32_t numSigmaY;
    int32_t dim; // 0 means dimension is undefined (occurs when an empty list is parsed from a string)

    bool im;

    void setSigmas1D(const Sigma<1>* sigmas, size_t sigmasSize);
    void setSigmas2D(const Sigma<2>* sigmas, size_t sigmasSize);
    void setSigmas3D(const Sigma<3>* sigmas, size_t sigmasSize);

    template<size_t D>
    std::vector<Sigma<D>> getList() const;
public:
    Expression() = default;
    // Constructor from a string
    Expression(const std::string& s);
    // Constructor from a list of Pauli operators and a coefficient (default 1)
    Expression(const Sigma<1>* v, size_t vSize, const Poly& r, bool imm);
    Expression(const Sigma<2>* v, size_t vSize, const Poly& r, bool imm);
    Expression(const Sigma<3>* v, size_t vSize, const Poly& r, bool imm);
    // Constructor for loading from a serialized representation
    Expression(const std::vector<uint8_t>& data, size_t& ptr);
    
    void Commute(const Expression& O, std::function<void(Expression&& newExpression)> callback) const;
    bool operator==(const Expression &ex1) const;
    bool operator<(const Expression &ex1) const;
    Poly& getPoly();
    const Poly& getPoly() const;
    std::tuple<size_t, size_t, size_t> getNumSigmas() const;
    
    void print() const;
    void serialize(std::vector<uint8_t>& res) const;
};

struct ExpressionHasher {
    size_t operator()(const Expression& expr) const;
};
