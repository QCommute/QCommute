#include "Poly2.h"
#include <iostream>
#include "Poly.h"

Poly2::Poly2(const Poly& poly) {
    size_t ptr = 0;
    size_t nextCoef = 0;
    while(poly.powers_[ptr] != Poly::POLY_END) {
        std::vector<uint8_t> powersCur;
        while(poly.powers_[ptr] != Poly::MONOM_END) {
            powersCur.push_back(poly.powers_[ptr]);
            ptr++;
        }
        ptr++;

        data_[powersCur] = poly.coefs_[nextCoef++];
    }
}

void Poly2::printAsMathematica(size_t groupByFirst, std::function<std::string(size_t)> varNameFunc) const {
    std::map<std::vector<uint8_t>, std::map<std::vector<uint8_t>, BigInt>> groupedMonoms;
    
    for(const auto& [powers, coeff] : data_) {
        std::vector<uint8_t> powersCur = powers;
        while(powersCur.size() < groupByFirst)
            powersCur.push_back(0);
        
        groupedMonoms[std::vector<uint8_t>(powersCur.begin(), powersCur.begin() + groupByFirst)]
            [std::vector<uint8_t>(powersCur.begin() + groupByFirst, powersCur.end())] = coeff;
    }

    bool firstGroup = true;
    for(const auto& [firstPowers, monoms] : groupedMonoms) {
        if(firstGroup)
            firstGroup = false;
        else
            std::cout << " + ";

        bool firstOuterVar = true;
        for(size_t i = 0; i < groupByFirst; i++)
            if(firstPowers[i] != 0) {
                if(firstOuterVar)
                    firstOuterVar = false;
                else
                    std::cout << " ";
                std::cout << varNameFunc(i);
                if(firstPowers[i] > 1)
                    std::cout << '^' << (+firstPowers[i]);
            }

        bool needParentheses = !firstOuterVar;
        if(needParentheses)
            std::cout << " (";
        else if(!firstOuterVar)
            std::cout << " ";

        bool firstMonom = true;
        for(const auto& [lastPowers, coeff] : monoms) {
            if(firstMonom)
                firstMonom = false;
            else
                std::cout << " + ";
            coeff.print();
            
            for(size_t i = 0; i < lastPowers.size(); i++)
                if(lastPowers[i] != 0) {
                    std::cout << " ";
                    std::cout << varNameFunc(i + groupByFirst);
                    if(lastPowers[i] > 1)
                        std::cout << '^' << (+lastPowers[i]);
                }
        }

        if(needParentheses)
            std::cout << ")";
    }
}

Poly2& Poly2::operator+=(const Poly2& other) {
    for(const auto& [powers, coeff] : other.data_) {
        data_[powers] += coeff;
        if(data_[powers] == 0)
            data_.erase(powers);
    }
    return *this;
}

