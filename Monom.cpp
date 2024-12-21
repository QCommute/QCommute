#include "Sigma.h"
#include <cstdlib>
#include <gmpxx.h>
#include <memory>
using namespace std;
//класс мономов, в прицнипе я думал переписать его и сделать класс полиномов, но пока не сделал.
//моном содежит список степеней и коэффициент. в списке степеней каждому элементу соответвует степень некого параметра, число параметров в принципе не ограничено
bool Monom::isZero()
{
    return (mpz_cmp_si(coef, 0)==0);
}
void Monom::setCoef(mpz_t c_)
{
    mpz_set(coef,c_);
}
void Monom::Prod(const Monom &mn)
{
    mpz_mul(coef,coef,mn.coef);
    for (int i = 0; i < (int)mn.pows.size(); i++) {
        if (i>=(int)pows.size()){
            pows.push_back(mn.pows[i]);
        }
           else{
            pows[i]+=mn.pows[i];
        }
    }
}
void Monom::Sum(const Monom &mn)
{
    mpz_add(coef,coef,mn.coef);
}
void Monom::Prod2(int n)
{
    mpz_mul_si(coef,coef,n);
}
Monom::Monom(const std::string &s, int pos, int &npos)
{
    mpz_init(coef);
    if (s[pos] != '[' && s[pos] != ';') {
        assert(0);
    }
    pos++;
    while (s[pos] != ']' && s[pos] != ';'){
        if (s[pos]==','){
            pos++;
        }
        string nums="";
        int num=0;
        int temp=1;
        int pow=1;
        int j=0;
        while ((s[pos+j] >= '0' && s[pos+j] <= '9') || (s[pos] == '-')){
            nums+=s[pos+j];
            j++;
        }
        for (int k = pos+j-1; k > pos-1; k--) {
            temp = s[k] - '0';
            num+=temp*pow;
            pow*=10;
            temp=1;
        }
        if (mpz_cmp_si(coef, 0)==0 && (int)pows.size()==0){
            mpz_set_str(coef, nums.c_str(), 10);
        }
        else{
            pows.push_back(num);
        }
        pos+=j;
    }
    npos = pos;
}



Monom::Monom(const Monom &mn)
{
    mpz_init(coef);
    for (int i = 0; i < (int)mn.pows.size(); i++) {
        pows.push_back(mn.pows[i]);
    }
    mpz_set(coef,mn.coef);
}
bool Monom::operator==(const Monom &mn)
{
    if ((int)mn.pows.size()!= (int)pows.size()){
        return false;
    }
    if ((int)pows.size()==0){
        return true;
    }
    for (int i = 0; i < (int)mn.pows.size(); i++) {
        if (pows[i]!=mn.pows[i]){
            return false;
        }
    }
    return true;
}
void Monom::print()
{
    mpz_out_str(stdout, 10, coef);
    for (int i = 0; i < (int)pows.size(); i++) {
         std::cout << ',' << +pows[i];
    }
}
Monom::~Monom()
{
   //mpz_clear(coef);
}



void deepCopyMonom(std::shared_ptr<Monom> &to, std::shared_ptr<Monom> from)
{
    to = std::make_shared<Monom>(*std::dynamic_pointer_cast<Monom>(from));
}
