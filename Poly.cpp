#include "Poly.h"
#include <map>
#include <cassert>
#include <iostream>
#include "SerializationHelpers.h"

Poly::Poly() {
    powers_ = {POLY_END};
}

Poly::Poly(const std::string& str) {
    assert(str[0] == '[');
    assert(str.back() == ']');

    std::map<std::vector<uint8_t>, BigInt> data;
    size_t cur = 0;
    while(str[cur] != ']') {
        assert(str[cur] == ';' || str[cur] == '[');
        size_t next = cur + 1;
        while(str[next] != ';' && str[next] != ']')
            next++;

        BigInt coefCur;
        std::vector<uint8_t> powersCur;

        std::string strMonom(str.begin() + cur + 1, str.begin() + next);
        size_t startOfNumber = 0;
        for(size_t i = 1; i <= strMonom.size(); i++)
            if(i == strMonom.size() || strMonom[i] == ',') {
                if(startOfNumber == 0)
                    coefCur = BigInt(strMonom.substr(startOfNumber, i - startOfNumber));
                else
                    powersCur.push_back(std::stoi(strMonom.substr(startOfNumber, i - startOfNumber)));
                startOfNumber = i + 1;
            }

        auto it = data.find(powersCur);
        if(it == data.end())
            data[powersCur] = coefCur;
        else {
            it->second += coefCur;
            if(it->second == 0)
                data.erase(it);
        }
        cur = next;
    }
    
    for(const auto& [powers, coeff] : data) {
        powers_.insert(powers_.end(), powers.begin(), powers.end());
        powers_.push_back(MONOM_END);
        coefs_.push_back(coeff);
    }
    powers_.push_back(POLY_END);
}

Poly::Poly(const std::vector<uint8_t>& data, size_t& ptr) {
    uint8_t x;
    size_t numMonoms = 0;
    do {
        x = deserializeUInt8(data, ptr);
        powers_.push_back(x);
        if(x == MONOM_END)
            numMonoms++;
    } while(x != POLY_END);

    for(size_t i = 0; i < numMonoms; i++)
        coefs_.emplace_back(data, ptr);
}

bool Poly::isZero() const {
    return coefs_.empty();
}

void Poly::print() const {
    std::cout << '[';
    size_t ptr = 0;
    size_t nextCoef = 0;
    while(powers_[ptr] != POLY_END) {
        if(ptr != 0)
            std::cout << ';';
        coefs_[nextCoef++].print();
        while(powers_[ptr] != MONOM_END) {
            std::cout << ',' << +(powers_[ptr]);
            ptr++;
        }
        ptr++;
    }
    std::cout << ']';
}

void Poly::serialize(std::vector<uint8_t>& res) const {
    for(size_t i = 0; i < powers_.size(); i++)
        serializeUInt8(res, powers_[i]);
    for(size_t i = 0; i < coefs_.size(); i++)
        coefs_[i].serialize(res);
}

Poly& Poly::operator+=(const Poly& other) {
    if(other.isZero())
        return *this;
    if(isZero()) {
        powers_ = other.powers_;
        coefs_ = other.coefs_;
        return *this;
    }
    
    std::vector<uint8_t> powersRes;
    std::vector<BigInt> coefsRes;
    size_t ptr1 = 0, ptr2 = 0;
    size_t nextCoef1 = 0, nextCoef2 = 0;
    while(powers_[ptr1] != POLY_END || other.powers_[ptr2] != POLY_END) {
        bool take1 = false, take2 = false;
        if(powers_[ptr1] == POLY_END)
            take2 = true;
        else if(other.powers_[ptr2] == POLY_END)
            take1 = true;
        else {
            int powerCompRes = 0;
            for(size_t i = 0; ; i++) {
                if(powers_[ptr1 + i] == other.powers_[ptr2 + i] && powers_[ptr1 + i] == MONOM_END)
                    break;
                if(powers_[ptr1 + i] != other.powers_[ptr2 + i]) {
                    if(powers_[ptr1 + i] == MONOM_END)
                        powerCompRes = -1;
                    else if(other.powers_[ptr2 + i] == MONOM_END)
                        powerCompRes = 1;
                    else
                        powerCompRes = (powers_[ptr1 + i] < other.powers_[ptr2 + i]) ? -1 : 1;
                    break;
                }
            }
            
            if(powerCompRes < 0)
                take1 = true;
            else if(powerCompRes > 0)
                take2 = true;
            else {
                coefsRes.push_back(coefs_[nextCoef1++]);
                coefsRes.back() += other.coefs_[nextCoef2++];
                if(coefsRes.back() == 0) {
                    coefsRes.pop_back();
                    while(powers_[ptr1] != MONOM_END)
                        ptr1++;
                    ptr1++;
                    while(other.powers_[ptr2] != MONOM_END)
                        ptr2++;
                    ptr2++;
                } else {
                    size_t oldPtr1 = ptr1;
                    for(; powers_[ptr1] != MONOM_END; ptr1++)
                        powersRes.push_back(powers_[ptr1]);
                    ptr1++;
                    ptr2 += ptr1 - oldPtr1;
                    powersRes.push_back(MONOM_END);
                }
            }
        }

        if(take1) {
            for(; powers_[ptr1] != MONOM_END; ptr1++)
                powersRes.push_back(powers_[ptr1]);
            ptr1++;
            powersRes.push_back(MONOM_END);
            coefsRes.push_back(coefs_[nextCoef1++]);
        } else if(take2) {
            for(; other.powers_[ptr2] != MONOM_END; ptr2++)
                powersRes.push_back(other.powers_[ptr2]);
            ptr2++;
            powersRes.push_back(MONOM_END);
            coefsRes.push_back(other.coefs_[nextCoef2++]);
        }
    }
    powersRes.push_back(POLY_END);
    
    powers_ = powersRes;
    coefs_ = coefsRes;
    return *this;
}

Poly Poly::operator*(const Poly& other) const {
    Poly res;
    size_t monomStart = 0;
    for(int32_t i = 0; i < coefs_.size(); i++) {
        Poly tmp;
        tmp.coefs_ = other.coefs_;
        for(size_t j = 0; j < tmp.coefs_.size(); j++)
            tmp.coefs_[j] *= coefs_[i];
        tmp.powers_.clear();

        size_t monomEnd = monomStart;
        while(powers_[monomEnd] != MONOM_END)
            monomEnd++;
        size_t lenAdded = monomEnd - monomStart;
        
        size_t lenCur = 0;
        for(size_t i = 0; i < other.powers_.size(); i++) {
            if(other.powers_[i] == POLY_END) {
                tmp.powers_.push_back(POLY_END);
                break;
            } else if(other.powers_[i] == MONOM_END) {
                while(lenCur < lenAdded) {
                    tmp.powers_.push_back(powers_[monomStart + lenCur]);
                    lenCur++;
                }
                tmp.powers_.push_back(MONOM_END);
                lenCur = 0;
            } else {
                tmp.powers_.push_back(other.powers_[i] + ((lenCur < lenAdded) ? powers_[monomStart + lenCur] : 0));
                lenCur++;
            }
        }
        // поскольку ко всем векторам степеней прибавился один и тот же новый вектор, сортировать заново не требуется

        monomStart = monomEnd + 1;
        res += tmp;
    }
    return res;
}

bool Poly::operator==(const Poly& other) const {
    return powers_ == other.powers_ && coefs_ == other.coefs_;
}

Poly& Poly::operator*=(int n) {
    if(n == 0) {
        powers_ = {POLY_END};
        coefs_.clear();
    } else {
        for(size_t i = 0; i < coefs_.size(); i++)
            coefs_[i] *= n;
    }
    return *this;
}

Poly Poly::operator*(int n) const {
    Poly res = *this;
    res *= n;
    return res;
}
