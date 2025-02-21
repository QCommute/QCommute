#pragma once

#include <cstdio>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <cassert>

template<size_t D>
struct Point {
    int32_t p[D];
  
    Point() {
        for(size_t i = 0; i < D; i++)
            p[i] = 0;
    }

    bool operator<(const Point<D>& other) const {
        for(size_t i = 0; i < D; i++)
            if(p[i] != other.p[i])
                return p[i] < other.p[i];
        return false;
    }

    Point<D> operator+(const Point<D>& other) const {
        Point<D> res;
        for(size_t i = 0; i < D; i++)
            res.p[i] = p[i] + other.p[i];
        return res;
    }

    Point<D> operator-() const {
        Point<D> res;
        for(size_t i = 0; i < D; i++)
            res.p[i] = -p[i];
        return res;
    }

    Point<D> operator-(const Point<D>& other) const {
        return operator+(-other);
    }

    static Point<D> elementwiseMin(const Point<D>& p1, const Point<D>& p2) {
        Point<D> res;
        for(size_t i = 0; i < D; i++)
            res.p[i] = std::min(p1.p[i], p2.p[i]);
        return res;
    }

    static Point<D> elementwiseMax(const Point<D>& p1, const Point<D>& p2) {
        Point<D> res;
        for(size_t i = 0; i < D; i++)
            res.p[i] = std::max(p1.p[i], p2.p[i]);
        return res;
    }

    bool operator==(const Point<D>& other) const {
        for(size_t i = 0; i < D; i++)
            if(p[i] != other.p[i])
                return false;
        return true;
    }
};

template<size_t D>
class Sigma {
private:
    Point<D> p_;
    int32_t type_;
public:
    Sigma() : p_(), type_(0) {
    }
    
    Sigma(const std::string& str, size_t ptr, size_t& end) {
        assert(str[ptr] == 'S');
        ptr++;
        
        assert(str[ptr] >= 'x' && str[ptr] <= 'z');
        type_ = str[ptr] - 'x' + 1;
        ptr++;

        assert(str[ptr] == '(');
        ptr++;

        for(size_t i = 0; i < D; i++) {
            int32_t coord = 0;
            while(str[ptr] >= '0' && str[ptr] <= '9') {
                coord = coord * 10 + (str[ptr] - '0');
                ptr++;
            }
            p_.p[i] = coord;
            
            if(i == D - 1)
                assert(str[ptr] == ')');
            else
                assert(str[ptr] == ',');
            ptr++;
        }
        end = ptr;
    }

    Sigma(std::initializer_list<int32_t> coords, int32_t type) {
        assert(coords.size() == D);
        size_t next = 0;
        for(int32_t x : coords) {
            p_.p[next] = x;
            next++;
        }
        type_ = type;
    }
  
    Point<D> getPoint() const {
        return p_;
    }
    
    int32_t getC() const {
        return type_;
    }

    void setPoint(const Point<D>& p) {
        p_ = p;
    }

    void setC(int32_t type) {
        type_ = type;
    }

    bool operator<(const Sigma& other) const {
        return std::tie(p_, type_) < std::tie(other.p_, other.type_);
    }

    void print() const {
        char symbols[3] = {'x', 'y', 'z'};
        std::cout << 'S' << symbols[type_ - 1] << '(';
        for(size_t i = 0; i < D; i++) {
            if(i > 0)
                std::cout << ',';
            std::cout << p_.p[i];
        }
        std::cout << ')';
    }
};

using Sigma1d = Sigma<1>;
using Sigma2d = Sigma<2>;
using Sigma3d = Sigma<3>;
