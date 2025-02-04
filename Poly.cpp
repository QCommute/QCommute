#include "Poly.h"
#include <map>

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
    
    for(const auto& [powers, coeff] : data)
        data_.emplace_back(powers, coeff);
}

bool Poly::isZero() const {
    return data_.empty();
}

void Poly::print() const {
    std::cout << '[';
    for (int i = 0; i < (int)data_.size(); i++) {
        if (i){
            std::cout << ';';
        }
        data_[i].print();
    }
    std::cout << ']';
}

Poly& Poly::operator+=(const Poly& other) {
    if(other.isZero())
        return *this;
    if(isZero()) {
        data_ = other.data_;
        return *this;
    }
    
    std::vector<Monom> dataRes;
    size_t ptr1 = 0, ptr2 = 0;
    while(ptr1 < data_.size() || ptr2 < other.data_.size()) {
        bool take1 = false, take2 = false;
        if(ptr1 == data_.size())
            take2 = true;
        else if(ptr2 == other.data_.size())
            take1 = true;
        else {
            int powerCompRes = data_[ptr1].comparePowers(other.data_[ptr2]);
            if(powerCompRes < 0)
                take1 = true;
            else if(powerCompRes > 0)
                take2 = true;
            else {
                dataRes.push_back(data_[ptr1]);
                dataRes.back().Sum(other.data_[ptr2]);
                if(dataRes.back().isZero())
                    dataRes.pop_back();
                ptr1++;
                ptr2++;
            }
        }

        if(take1) {
            dataRes.push_back(data_[ptr1]);
            ptr1++;
        } else if(take2) {
            dataRes.push_back(other.data_[ptr2]);
            ptr2++;
        }
    }
    data_ = dataRes;
    return *this;
}

Poly Poly::operator*(const Poly& other) const {
    Poly res;
    for(int32_t i = 0; i < data_.size(); i++) {
        Poly tmp = other;
        for(int32_t j = 0; j < tmp.data_.size(); j++)
            tmp.data_[j] *= data_[i];
        // поскольку ко всем векторам степеней прибавился один и тот же новый вектор, сортировать заново не требуется
        res += tmp;
    }
    return res;
}

bool Poly::operator==(const Poly& other) const {
    return data_ == other.data_;
}

Poly& Poly::operator*=(int n) {
    if(n == 0)
        data_.clear();
    else {
        for(size_t i = 0; i < data_.size(); i++)
            data_[i] *= n;
    }
    return *this;
}

Poly Poly::operator*(int n) const {
    Poly res = *this;
    res *= n;
    return res;
}
