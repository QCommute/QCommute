#include "Poly.h"
#include <gmp-x86_64.h>

Poly::Poly(const std::string& str) {
    assert(str[0] == '[');
    assert(str.back() == ']');

    size_t cur = 0;
    while(str[cur] != ']') {
        assert(str[cur] == ';' || str[cur] == '[');
        size_t next = cur + 1;
        while(str[next] != ';' && str[next] != ']')
            next++;
        data_.emplace_back(str.substr(cur + 1, next - cur - 1));
        cur = next;
    }
    
    std::sort(data_.begin(), data_.end(), [](const Monom& m1, const Monom& m2) {
        return m1.lessPowers(m2);
    });
    size_t ptr = 0;
    std::vector<size_t> goodIndices = {0};
    for(size_t i = 1; i < data_.size(); i++) {
        if(data_[i].samePowers(data_[i - 1])) {
            data_[ptr].Sum(data_[i]);
        } else {
            ptr = i;
            goodIndices.push_back(i);   
        }
    }
    std::vector<Monom> data2;
    for(size_t i : goodIndices)
        if(!data_[i].isZero())
            data2.push_back(data_[i]);
    data_ = data2;
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
        if(ptr1 < data_.size() && !dataRes.empty() && dataRes.back().samePowers(data_[ptr1])) {
            dataRes.back().Sum(data_[ptr1]);
            if(dataRes.back().isZero())
                dataRes.pop_back();
            ptr1++;
        } else if(ptr2 < other.data_.size() && !dataRes.empty() && dataRes.back().samePowers(other.data_[ptr2])) {
            dataRes.back().Sum(other.data_[ptr2]);
            if(dataRes.back().isZero())
                dataRes.pop_back();
            ptr2++;
        } else if(ptr1 < data_.size() && (ptr2 == other.data_.size() || data_[ptr1].lessPowers(other.data_[ptr2]))) {
            dataRes.push_back(data_[ptr1]);
            ptr1++;
        } else {
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
