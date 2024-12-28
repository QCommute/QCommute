#include <cstdint>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include <memory>
#include <iostream>
#include <cassert>
#include <ctime>
#include <typeinfo>
#include <typeindex>
#include <cmath>
#include <cstdio>
#include <bitset>
#include "Expression.h"
#include "Sigma.h"
//#include <conio.h>
#include <thread>
#include <mutex>
#include <omp.h>
#include <stdio.h>
#include <gmpxx.h>
#include <filesystem>
//#include <ginac/ginac.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <queue>
int indcount;
int lorindcount;
std::string directoryPath;
typedef std::chrono::high_resolution_clock Clock;

using namespace std;

int raiseToPow(int x, int power)
{
    int result;
    int i;
    result =1;
    for (i=1; i<=power;i++)
    {
        result = result*x;
    }
    return(result);
}

struct Node {
    Expression expr;
    Node* next;

    Node(Expression&& expr_, Node* next_) : expr(expr_) {
        next = next_;
    }
};

int
main(int argc, char** argv)
{
    if(std::string(argv[1]) == "validate") {
        std::vector<Expression> src, new_;
        std::string str;
        
        std::ifstream file_src(argv[2], std::ifstream::in);
        while(std::getline(file_src, str))
            src.push_back(Expression(str));

        std::ifstream file_new(argv[3], std::ifstream::in);
        while(std::getline(file_new, str))
            new_.push_back(Expression(str));

        std::unordered_set<Expression, ExpressionHasher> remain_src;
        for(size_t i = 0; i < src.size(); i++)
            remain_src.insert(src[i]);
        if(remain_src.size() != src.size()) {
            std::cout << "В первом файле есть подобные слагаемые, проверка не удалась" << std::endl;
            return 0;
        }
        if(src.size() != new_.size()) {
            std::cout << "В файлах разное количество слагаемых" << std::endl;
            return 0;
        }

        for(size_t i = 0; i < new_.size(); i++) {
            auto it = remain_src.find(new_[i]);
            if(it == remain_src.end()) {
                std::cout << "Файлы не совпадают" << std::endl;
                return 0;
            }
            if(!(it->getPoly() == new_[i].getPoly())) {
                std::cout << "Файлы не совпадают" << std::endl;
                return 0;
            }
            remain_src.erase(it);
        }
        std::cout << "Файлы совпадают" << std::endl;
        return 0;
    }
    
    directoryPath="./tmp"; //место где хранится файлы с коммутаторами
    //std::cout << sizeof(Sigma2d)<< std::endl;
    auto start_time = Clock::now();//просто часы чтобы мерить время исполнения
    setlocale(LC_ALL, "");
    std::vector <Expression> H;//Переменная Гамильтониана
    std::vector <Expression> O;//Переменная Наблюдаемой
    std::vector <Expression> O1;
    std::vector <int> I;
    std::ifstream file(argv[1], std::ifstream::in);//огткрфваем файл в котором содерижится наблюдаемая и гамильтониан
    std::string str;
    std::getline(file, str);
    if (str=="H:"){
        std::getline(file, str); 
    }
    else{
        assert(0);
    }
    while(str!="O:") {
        Expression t(str);//Создаем из строчки выражение для гамильтониана 
        H.push_back(t);//и помещаем в список
        std::getline(file, str);
    }
    //std::getline(file, str);
    if (str=="O:"){
        std::getline(file, str);
    }
    else{
        assert(0);
    }
    while(str!="n:") {
        Expression t(str);//Создаем из строчки выражение для наблюдаемой
        O.push_back(t);//и помещаем в список
        std::getline(file, str);
    }
    std::getline(file, str);
    if (str=="n:"){
        std::getline(file, str);
    }
    int n=0;
    for (int i = 0; i < (int)str.length(); i++) {
        n+=(str[i]-'0')*raiseToPow(10,(int)str.length()-i-1);//порядок разложения (сколько вложенных коммутаторов нужно посчитать)
    }
    file.close();
    std::cerr << "Файл считан корректно"<< std::endl;

    for(int32_t t = 0; t < n; t++) {
        std::cerr << "Этап " << t + 1 << std::endl;
        directoryPath += std::to_string(t + 1);
        if (!std::filesystem::exists(directoryPath)) {
            std::filesystem::create_directory(directoryPath);
            std::cerr << "Создана директория: " << t + 1 << std::endl;
        } 

        auto time2 = Clock::now();
        size_t tableSize = H.size() * O.size();
        Node** indices = new Node*[tableSize];
        for(size_t i = 0; i < tableSize; i++)
            indices[i] = nullptr;
        std::mutex* indexMutexes = new std::mutex[tableSize];
        for (size_t i = 0; i < H.size(); i++) {
            std::queue<size_t> tasks;
            for (size_t j = 0; j < O.size(); j++)
                tasks.push(j);
            std::mutex taskMutex;

            std::vector<std::thread> threads;
            size_t num_threads = std::thread::hardware_concurrency();
            for(size_t tt = 0; tt < num_threads; tt++) {
                threads.emplace_back([&tasks, &taskMutex, indices, tableSize, indexMutexes, &O, &H, i]() {
                    while(true) {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        if(tasks.empty())
                            return;
                        size_t j = tasks.front();
                        tasks.pop();
                        lock.unlock();
                        
                        std::vector<Expression> cur;
                        H[i].Commute(cur, O[j]); //коммутируем все салгаемые с гамильтонанном.
                        
                        for(size_t k = 0; k < cur.size(); k++) {
                            size_t hash = ExpressionHasher()(cur[k]);
                            std::lock_guard<std::mutex> guard(indexMutexes[hash % tableSize]);
                            Node* cur_node = indices[hash % tableSize];
                            bool found = false;
                            while(cur_node != nullptr) {
                                if(cur_node->expr == cur[k]) {
                                    cur_node->expr.getPoly() += cur[k].getPoly();
                                    found = true;
                                    break;
                                }
                                cur_node = cur_node->next;
                            }
                            if(!found) {
                                indices[hash % tableSize] = new Node(std::move(cur[k]), indices[hash % tableSize]);
                            }
                        }
                    }
                });
            }
            for(size_t j = 0; j < threads.size(); j++)
                threads[j].join();
        }
        delete[] indexMutexes;
        auto time3 = Clock::now();
        std::cerr << "Подсчет коммутаторов и приведение подобных: " << (std::chrono::duration_cast<std::chrono::microseconds>(time3 - time2).count())/1000000.0 << " сек" << std::endl;

        std::vector<Node*> O1_tmp;
        for(size_t i = 0; i < tableSize; i++) {
            Node* cur_node = indices[i];
            while(cur_node != nullptr) {
                O1_tmp.push_back(cur_node);
                cur_node = cur_node->next;
            }
        }
        delete[] indices;

        for(size_t i = 0; i < O1_tmp.size(); i++) {
            O1.push_back(std::move(O1_tmp[i]->expr));
            delete O1_tmp[i];
        }
        
        size_t next = 0;
        for(size_t i = 0; i < O1.size(); i++)
            if(!O1[i].getPoly().isZero()) {
                if(i != next)
                    std::swap(O1[next], O1[i]);
                next++;
            }
        while(O1.size() > next)
            O1.pop_back();
        O.swap(O1);
        O1.clear();
    
        directoryPath.erase(directoryPath.size()-1);
        std::cerr << std::endl;
    }
    
//вывод результата, пока он делается просто на экран
if ((int)O.size()!=0){
    for (int i= 0; i < (int)O.size(); i++) {
        O[i].print();
    }
}
else{
    for (int i= 0; i < (int)O1.size(); i++) {
        O1[i].print();
    }
}

auto time1 = Clock::now();
std::cerr << "Вычисление заняло "<< (std::chrono::duration_cast<std::chrono::microseconds>(time1 - start_time).count())/1000000.0 << " сек" << std::endl;
//    getc(stdin);
//    getc(stdin);
        return 0;
}
