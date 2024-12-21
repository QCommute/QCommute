#include "Expression.h"
#include <cmath>
#include <fstream>
#include <cstdio>
#include <memory>
#include <thread>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <algorithm>
#include <bitset>
extern int indcount;
extern int lorindcount;
extern std::string directoryPath;
using namespace std;
//Expression самый главный класс, он включает в себя список элементов класса Sigma (строки Паули) и многочлен (список мономов)
//конструктор выражения от строки
Expression::Expression(std::string s)
{
    //Ну это ее при тебе писал, по первому символу вызываем нужные конструкторы, выделяем память и закидываем в список
    im=false;
    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i]=='i'){
            im=true;
            i++;
        }
        if (s[i] == '[' || s[i] == ';') {
            int tmp;
            std::shared_ptr<Monom> mon(new Monom(s,i,tmp));
            i = tmp;
            poly.push_back(mon);
            if (s[i] == ']'){
                i++;
            }
        } else if (s[i] == 'S') {
            int tmp;
            int j=1;
            while (s[i+j] != ')'){
                j++;
            }
            if (j==4){
                std::shared_ptr<Sigma1d> ptr(new Sigma1d(s,i,tmp));
                lst.push_back(ptr);
            }
            else{
                if (j==6){
                    std::shared_ptr<Sigma2d> ptr(new Sigma2d(s,i,tmp));
                    lst.push_back(ptr);
                }
                else{
                    if (j==8){
                        std::shared_ptr<Sigma3d> ptr(new Sigma3d(s,i,tmp));
                        lst.push_back(ptr);
                    }
                    else{
                        assert(0);
                    }
                }
            }
            i = tmp;
        }
        else if (s[i] == '*') {

        } else {
            assert(0);
        }
    }
}

//Просто глубокое копирование списка и присваивание коэффициента
Expression::Expression(const std::vector<std::shared_ptr<Sigma>> &v,  const std::vector<std::shared_ptr<Monom>> &r, bool imm)
{
    im=imm;
    for (int i = 0; i < (int)v.size(); i++) {
        std::shared_ptr<Sigma> t;
        deepCopyBase(t, v[i]);
        lst.push_back(t);
    }
    for (int i = 0; i < (int)r.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, r[i]);
        poly.push_back(t);
    }
}
//конструктор от выражения
Expression::Expression(const Expression &ex1)
{
    im=ex1.im;
    for (int i = 0; i < (int)ex1.lst.size(); i++) {
        std::shared_ptr<Sigma> t;
        deepCopyBase(t, ex1.lst[i]);
        lst.push_back(t);
    }
    for (int i = 0; i < (int)ex1.poly.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, ex1.poly[i]);
        poly.push_back(t);
    }
} 
//присваивание выражения
Expression& Expression::operator=(const Expression &ex1)
{
im=ex1.im;
    //for (int i = 0; i < (int)lst.size(); i++) {
    //    delete lst[i];
    //}
    lst.clear();
    poly.clear();
    for (int i = 0; i < (int)ex1.lst.size(); i++) {
        std::shared_ptr<Sigma> t;
        deepCopyBase(t, ex1.lst[i]);
        lst.push_back(t);
    }
    for (int i = 0; i < (int)ex1.poly.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, ex1.poly[i]);
        poly.push_back(t);
    }
    return *this;
}
//много вспомогательных функций
std::vector<std::shared_ptr<Sigma>> Expression::getList() const
{
    return lst;
}
std::vector<std::shared_ptr<Monom>> Expression::getPoly()
{
    return poly;
}
void Expression::PolyErase(int i)
{
    poly.erase(poly.begin()+i);

}
void Expression::PolyAdd(std::shared_ptr<Monom> t)
{
    poly.push_back(t);
}
int Expression::getLength() const
{
    return (int)lst.size();
}
//самый главный оператор (коммутатор списка с элементов)
void Expression::Commute(std::vector <Expression> &O1,  const Expression &O)
{
    std::vector<std::shared_ptr<Monom>> pol1;
    std::vector<std::shared_ptr<Monom>> pol2;
    //работа с полиномами, а именно их умножение
    for (int i = 0; i < (int)O.poly.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, O.poly[i]);
        pol1.push_back(t);
        pol2.push_back(t);
    }
    for (int i = 0; i < (int)pol1.size(); i++) {
        pol1[i]->Prod(*poly[0]);
    }
    for (int i = 1; i < (int)poly.size(); i++) {
        for (int j = 0; j < (int)pol2.size(); j++) {
            pol2[j]->Prod(*poly[i]);
        }
        for (int j = 0; j < (int)pol2.size(); j++) {
            for (int k = 0; k < (int)pol1.size(); k++) {
                if (*pol2[j]==*pol1[k]){
                    pol1[k]->Sum(*pol2[j]);
                    pol2.erase(pol2.begin()+j);
                    j--;
                    break;
                }
            }
        }
        for (int j = 0; j < (int)pol2.size(); j++) {
            pol1.push_back(pol2[j]);
        }
        pol2.clear();

        for (int j = 0; j < (int)O.poly.size(); j++) {
            std::shared_ptr<Monom> t;
            deepCopyMonom(t, O.poly[j]);
            pol2.push_back(t);
        }
    }
    for (int i = 0; i < (int)pol1.size(); i++) {
        pol1[i]->Prod2(2);
    }
    pol2.clear();

    //отсюда начинается сама коммутация в прицнипе тут осбо модно ничего не комментировать главное то как записывается результат в конце , там будет комментарий
    std::vector<std::shared_ptr<Sigma>> lst1;
    for (int i = 0; i < (int)O.lst.size(); i++){
        lst1.push_back(O.lst[i]);
    }
    for (int i = 0; i < (int)lst.size(); i++) {
         for (int j = 0; j < (int)lst1.size(); j++){
             if (lst[i]->getType()!=lst1[j]->getType()){
                continue;
             }
             auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst[i]);
             auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
             auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst[i]);
             auto s11 = std::dynamic_pointer_cast<Sigma1d>(lst1[j]);
             auto s21 = std::dynamic_pointer_cast<Sigma2d>(lst1[j]);
             auto s31 = std::dynamic_pointer_cast<Sigma3d>(lst1[j]);
             if (s1){
                bool b=false;
                if (s1->getC()==s11->getC()){
                    continue;
                }
                int imm=0;
                int nc=LeviChivita(s1->getC(),s11->getC());
                imm++;
                if (O.im==true){
                    imm++;
                }
                 if (im==true){
                    imm++;
                }
                if (nc<0){
                    imm+=2;
                    nc=nc*(-1);
                }
                int pd=s11->getPoint()-s1->getPoint();
                std::vector<std::shared_ptr<Sigma>> lst3;
                for (int k = 0; k < (int)lst.size(); k++) {
                    std::shared_ptr<Sigma> tt;
                    deepCopyBase(tt, lst[k]);
                    auto t = std::dynamic_pointer_cast<Sigma1d>(tt);
                    if (k==i){
                        t->setC(nc);
                    }
                    if (pd>0){
                        t->setPoint(t->getPoint()+pd);
                    }
                    lst3.push_back(t);
                }
                for (int k = 0; k < (int)lst1.size(); k++) {
                    if (k==j){
                        continue;
                    }
                    std::shared_ptr<Sigma> tt;
                    deepCopyBase(tt, lst1[k]);
                    auto t = std::dynamic_pointer_cast<Sigma1d>(tt);
                    if (pd<0){
                        t->setPoint(t->getPoint()-pd);
                    }
                    lst3.push_back(t);
                }
                int mp=0;
                int nn=0;
                for (int k = 0; k < (int)lst3.size(); k++) {
                    auto t = std::dynamic_pointer_cast<Sigma1d>(lst3[k]);
                    bool bb=false;
                    for (int l = k+1; l < (int)lst3.size(); l++) {
                        auto t1 = std::dynamic_pointer_cast<Sigma1d>(lst3[l]);
                        if (t->getPoint()==t1->getPoint()){
                            if (t->getC()==t1->getC()){
                                bb=true;
                                lst3.erase(lst3.begin()+l);
                                lst3.erase(lst3.begin()+k);
                                k--;
                                break;
                            }
                            else{
                                nn++;
                                nc=LeviChivita(t->getC(),t1->getC());
                                imm++;
                                if (nc<0){
                                    nc=nc*(-1);
                                    imm+=2;
                                }
                                t->setC(nc);
                                lst3.erase(lst3.begin()+l);
                                l--;
                                continue;
                            }
                        }
                    }
                    if (bb==false){
                        if (mp==0){
                            mp=t->getPoint();
                        }
                        else{
                            if (mp>t->getPoint()){
                                mp=t->getPoint();
                            }
                        }
                    }
                }
                if ((nn % 2) != 0){
                    continue;
                }
                if (mp>1){
                    for (int k = 0; k < (int)lst3.size(); k++) {
                        auto t = std::dynamic_pointer_cast<Sigma1d>(lst3[k]);
                        t->setPoint(t->getPoint()-mp+1);
                    }
                }
                imm=imm % 4;
                bool im1=false;
                if (imm==1){
                    im1=true;
                }
                if (imm==2){
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                    b=true;
                }
                if (imm==3){
                    im1=true;
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                    b=true;
                }
                Expression n_exp(lst3,pol1,im1);
                if (b==true){
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                }
                O1.push_back(n_exp);
             }
             if (s2){
                bool b=false;
                if (s2->getC()==s21->getC()){
                    continue;
                }
                int imm=0;
                int nc=LeviChivita(s2->getC(),s21->getC());
                imm++;
                if (O.im==true){
                    imm++;
                }
                 if (im==true){
                    imm++;
                }
                if (nc<0){
                    imm+=2;
                    nc=nc*(-1);
                }
                int pd1=s21->getPoint1()-s2->getPoint1();
                int pd2=s21->getPoint2()-s2->getPoint2();
                std::vector<std::shared_ptr<Sigma>> lst3;
                for (int k = 0; k < (int)lst.size(); k++) {
                    std::shared_ptr<Sigma> tt;
                    deepCopyBase(tt, lst[k]);
                    auto t = std::dynamic_pointer_cast<Sigma2d>(tt);
                    if (k==i){
                        t->setC(nc);
                    }
                    if (pd1>0){
                        t->setPoint1(t->getPoint1()+pd1);
                    }
                    if (pd2>0){
                        t->setPoint2(t->getPoint2()+pd2);
                    }
                    lst3.push_back(t);
                }
                for (int k = 0; k < (int)lst1.size(); k++) {
                    if (k==j){
                        continue;
                    }
                    std::shared_ptr<Sigma> tt;
                    deepCopyBase(tt, lst1[k]);
                    auto t = std::dynamic_pointer_cast<Sigma2d>(tt);
                    if (pd1<0){
                        t->setPoint1(t->getPoint1()-pd1);
                    }
                    if (pd2<0){
                        t->setPoint2(t->getPoint2()-pd2);
                    }
                    lst3.push_back(t);
                }
                int mp1=0;
                int mp2=0;
                int nn=0;
                for (int k = 0; k < (int)lst3.size(); k++) {
                    auto t = std::dynamic_pointer_cast<Sigma2d>(lst3[k]);
                    bool bb=false;
                    for (int l = k+1; l < (int)lst3.size(); l++) {
                        auto t1 = std::dynamic_pointer_cast<Sigma2d>(lst3[l]);
                        if (t->getPoint1()==t1->getPoint1() && t->getPoint2()==t1->getPoint2()){
                            if (t->getC()==t1->getC()){
                                bb=true;
                                lst3.erase(lst3.begin()+l);
                                lst3.erase(lst3.begin()+k);
                                k--;
                                break;
                            }
                            else{
                                nn++;
                                nc=LeviChivita(t->getC(),t1->getC());
                                imm++;
                                if (nc<0){
                                    nc=nc*(-1);
                                    imm+=2;
                                }
                                t->setC(nc);
                                lst3.erase(lst3.begin()+l);
                                l--;
                                continue;
                            }
                        }
                    }
                    if (bb==false){
                        if (mp1==0){
                            mp1=t->getPoint1();
                        }
                        else{
                            if (mp1>t->getPoint1()){
                                mp1=t->getPoint1();
                            }
                        }
                        if (mp2==0){
                            mp2=t->getPoint2();
                        }
                        else{
                            if (mp2>t->getPoint2()){
                                mp2=t->getPoint2();
                            }
                        }
                    }
                }
                if ((nn % 2) != 0){
                    continue;
                }
                if (mp1>1){
                    for (int k = 0; k < (int)lst3.size(); k++) {
                        auto t = std::dynamic_pointer_cast<Sigma2d>(lst3[k]);
                        t->setPoint1(t->getPoint1()-mp1+1);
                    }
                }
                if (mp2>1){
                    for (int k = 0; k < (int)lst3.size(); k++) {
                        auto t = std::dynamic_pointer_cast<Sigma2d>(lst3[k]);
                        t->setPoint2(t->getPoint2()-mp2+1);
                    }
                }
                imm=imm % 4;
                bool im1=false;
                if (imm==1){
                    im1=true;
                }
                if (imm==2){
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                    b=true;
                }
                if (imm==3){
                    im1=true;
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                    b=true;
                }
                Expression n_exp(lst3,pol1,im1); //это строчка с коэффициентов, таких утт создается много, все они в целом дают результат 
                if (b==true){
                    for (int k = 0; k < (int)pol1.size(); k++) {
                        pol1[k]->Prod2(-1);
                    }
                }
                uint8_t Tl= n_exp.TotalLength();
                uint8_t Mp= n_exp.MaxPoint();
                mpz_t mbr_x;
                mpz_t mbr_y;
                mpz_t mbr_z;
                mpz_init(mbr_x);
                mpz_init(mbr_y);
                mpz_init(mbr_z);
                n_exp.MakeMbr(mbr_x, mbr_y, mbr_z, Mp); //Вот тут создаем как раз те самые числа
                int numBitsx = mpz_sizeinbase(mbr_x, 2);
                int numBitsy = mpz_sizeinbase(mbr_y, 2);
                int numBitsz = mpz_sizeinbase(mbr_z, 2);
                uint8_t numBytesx = (numBitsx + 7) / 8; // округляем в большую сторону
                uint8_t numBytesy = (numBitsy + 7) / 8; // округляем в большую сторону
                uint8_t numBytesz = (numBitsz + 7) / 8; // округляем в большую сторону
                char* bufferx = new char[numBytesx];
                char* buffery = new char[numBytesy];
                char* bufferz = new char[numBytesz];
                mpz_export(bufferx, nullptr, 1, 1, 0, 0, mbr_x);
                mpz_export(buffery, nullptr, 1, 1, 0, 0, mbr_y);
                mpz_export(bufferz, nullptr, 1, 1, 0, 0, mbr_z);
                #pragma omp critical
                 {
                    //cout << mbr_x<< "ddwdw" << mbr_y << endl;
                    //создаем файл с соответвующим названием и помещаем в него эти числа
                    std::ofstream ofs(directoryPath+"/"+std::to_string(Mp)+"_"+std::to_string(Tl)+".bin", std::ios::binary);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(bufferx, numBytesx);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(buffery, numBytesx);
                    ofs.write(reinterpret_cast<char*>(&numBytesx), sizeof(numBytesx));                      
                    ofs.write(bufferz, numBytesx);
                    ofs.close(); //пока я эти файлы не использую, не успел ещё это сделать, просто в них скалдываем, еще нужно скалдывать в файлы полиномы, пока тоже не реализовано
                    O1.push_back(n_exp); //так что пока кладем в опретивную память и создаем новый список
                 }
                delete[] bufferx;
                delete[] buffery;
                delete[] bufferz;
             }
         }
    }
}

uint8_t Expression::TotalLength()
{
    return lst.size();
}
uint8_t Expression::MaxPoint()
{
    uint8_t mp=0;
    for (int i = 0; i < (int)lst.size(); i++){
        auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst[i]);
        auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
        auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst[i]);
        if (s1){
            if (s1->getPoint()>mp){
                mp=s1->getPoint();
            }
        }
        if (s2){
            if (s2->getPoint1()>mp){
                mp=s2->getPoint1();
            }
            if (s2->getPoint2()>mp){
                mp=s2->getPoint2();
            }
        }
    }
    return mp;
}

void Expression::MakeMbr(mpz_t mbr_x, mpz_t mbr_y, mpz_t mbr_z, uint8_t Mp)//функция которая создает три числа из строчки Паули
{
    for (int i = 0; i < (int)lst.size(); i++){
        auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst[i]);
        if (s2->getC()==1){
            mpz_setbit(mbr_x, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
        if (s2->getC()==2){
            mpz_setbit(mbr_y, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
        if (s2->getC()==3){
            mpz_setbit(mbr_z, Mp*(s2->getPoint1()-1)+s2->getPoint2()-1);
        }
    }
}


bool Expression::operator==(const Expression &ex1) const
{
    if ((int)ex1.lst.size()!=(int)lst.size()){
        return false;
    }
    if (ex1.im!=im){
        return false;
    }
    std::vector<std::shared_ptr<Sigma>> lst1;
    std::vector<std::shared_ptr<Sigma>> lst2;
    for (int i = 0; i < (int)lst.size(); i++){
        lst1.push_back(ex1.lst[i]);
        lst2.push_back(lst[i]);
    }
    int k=0;
    while ((int)lst2.size()){
            if (lst1[0]->getType()==lst2[k]->getType()){
                auto s1 = std::dynamic_pointer_cast<Sigma1d>(lst1[0]);
                auto s2 = std::dynamic_pointer_cast<Sigma2d>(lst1[0]);
                auto s3 = std::dynamic_pointer_cast<Sigma3d>(lst1[0]);
                auto s11 = std::dynamic_pointer_cast<Sigma1d>(lst2[k]);
                auto s21 = std::dynamic_pointer_cast<Sigma2d>(lst2[k]);
                auto s31 = std::dynamic_pointer_cast<Sigma3d>(lst2[k]);
                if (s1){
                    if (*s11==*s1){
                        lst2.erase(lst2.begin()+k);
                        lst1.erase(lst1.begin());
                        k=0;
                        continue;
                    }
                    else{
                        k++;
                        if (k==(int)lst2.size()){
                            return false;
                        }
                        continue;
                    }
                }
                if (s2){
                    if (*s21==*s2){
                        lst2.erase(lst2.begin()+k);
                        lst1.erase(lst1.begin());
                        k=0;
                        continue;
                    }
                    else{
                        k++;
                        if (k==(int)lst2.size()){
                            return false;
                        }
                        continue;
                    }
                }
                if (s3){
                    if (*s31==*s3){
                        lst2.erase(lst2.begin()+k);
                        lst1.erase(lst1.begin());
                        k=0;
                        continue;
                    }
                    else{
                        k++;
                        if (k==(int)lst2.size()){
                            return false;
                        }
                        continue;
                    }
                }
            }
            else{
                k++;
                if (k==(int)lst2.size()){
                    return false;
                    }
                continue;
            }
    }
    return true;
}


void Expression::setCoeff(std::vector<std::shared_ptr<Monom>> &r)
{
    poly.clear();
    for (int i = 0; i < (int)r.size(); i++) {
        std::shared_ptr<Monom> t;
        deepCopyMonom(t, r[i]);
        poly.push_back(t);
    }
}



int LeviChivita(int i, int j){
if (i==1){
    if (j==2){
        return 3;
    }
    if (j==3){
        return -2;
    }
}
if (i==2){
    if (j==3){
        return 1;
    }
    if (j==1){
        return -3;
    }
}
if (i==3){
    if (j==2){
        return -1;
    }
    if (j==1){
        return 2;
    }
}
return 0;
}

int PrimeNumber(int n){
    int k=0;
    long int test=1;
    while(k<n){
          test++;
          if (prime(test)==true){
            k++;
          }
    }
    return test;
}
bool prime(long int n){
	for(long int i=2;i<=sqrt(n);i++)
		if(n%i==0)
			return false;
	return true;
}
//std::vector<int> Expression::getCoeff() const
//{
 //   return coef;
//}
void Expression::print()
{
    if (im==true){
        std::cout << "i*";
    }
    std::cout << '[';
    for (int i = 0; i < (int)poly.size(); i++) {
        if (i){
            std::cout << ';';
        }
        poly[i]->print();
    }
    std::cout << "]*";
    for (int i = 0; i < (int)lst.size(); i++) {
        lst[i]->print();

        if (i != (int)lst.size() - 1) {
            std::cout << '*';
        }
    }
    std::cout << std::endl;
}


Expression::~Expression()
{
   // for (int i = 0; i < (int)poly.size(); i++) {
    //    delete poly[i];
    //}
}

size_t ExpressionHasher::operator()(const Expression& expr) const {
    return expr.lst.size() * 2 + (expr.im ? 1 : 0);
}
