#include "Monom.h"
#include <cstdlib>

//класс мономов, в прицнипе я думал переписать его и сделать класс полиномов, но пока не сделал.
//моном содежит список степеней и коэффициент. в списке степеней каждому элементу соответвует степень некого параметра, число параметров в принципе не ограничено

Monom::Monom(const std::vector<uint8_t>& pows, const BigInt& coeff) : pows(pows), coef(coeff) {
    // Do nothing
}

bool Monom::operator==(const Monom& other) const {
    return (pows == other.pows) && (coef == other.coef);
}

bool Monom::isZero() const {
    return coef == 0;
}

int Monom::comparePowers(const Monom& other) const {
    for(size_t i = 0; i < std::min(pows.size(), other.pows.size()); i++)
        if(pows[i] != other.pows[i])
            return (pows[i] < other.pows[i]) ? -1 : 1;
    if(pows.size() < other.pows.size())
        return -1;
    return (pows.size() == other.pows.size()) ? 0 : 1;
}

void Monom::Sum(const Monom& mn) {
    coef += mn.coef;
}

Monom& Monom::operator*=(const Monom& other) {
    coef *= other.coef;
    for (size_t i = 0; i < other.pows.size(); i++) {
        if(i >= pows.size())
            pows.push_back(other.pows[i]);
        else
            pows[i] += other.pows[i];
    }
    return *this;
}

Monom& Monom::operator*=(int n) {
    coef *= n;
    return *this;
}

void Monom::print() const {
    coef.print();
    for (size_t i = 0; i < pows.size(); i++)
        std::cout << ',' << +(pows[i]);
}

