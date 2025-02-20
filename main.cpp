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

struct Node {
    Expression expr;
    Node* next;

    Node(Expression&& expr_, Node* next_) : expr(std::move(expr_)) {
        next = next_;
    }
};

#define BLOCK_SIZE 512

std::vector<uint8_t> loadFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> data;
    data.resize(size);
    stream.read((char*)(data.data()), size);
    return data;
}

void storeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream stream(path, std::ios::binary);
    stream.write((char*)(data.data()), data.size());
}

std::vector<Expression> loadAndDeleteBlock(const std::string& path) {
    std::vector<uint8_t> data = loadFile(path);

    std::vector<Expression> res;
    size_t ptr = 0;
    while(ptr < data.size())
        res.emplace_back(data, ptr);

    return res;
}

std::vector<Expression> loadAndDeleteHashTableBlock(const std::string& dir, size_t level, size_t start) {
    return loadAndDeleteBlock(dir + "/" + std::to_string(level) + "/" + std::to_string(start) + ".comm");
}

void storeBlock(const std::string& path, const std::vector<Expression>& exprs) {
    std::vector<uint8_t> data;
    for(size_t i = 0; i < exprs.size(); i++)
        exprs[i].serialize(data);
    storeFile(path, data);
}

size_t moveHashTableBlockToDisk(const std::string& dir, size_t level, size_t start, size_t tableSize, Node** indices) {
    std::vector<Expression> exprs;
    for(size_t i = start; i < std::min(start + BLOCK_SIZE, tableSize); i++) {
        Node* cur_node = indices[i];
        while(cur_node != nullptr) {
            Node* next_node = cur_node->next;
            if(!cur_node->expr.getPoly().isZero())
                exprs.push_back(std::move(cur_node->expr));
            delete cur_node;
            cur_node = next_node;
        }
        indices[i] = nullptr;
    }
    storeBlock(dir + "/" + std::to_string(level) + "/" + std::to_string(start) + ".comm", exprs);
    return exprs.size();
}

std::string directoryPath;
typedef std::chrono::high_resolution_clock Clock;

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
    
    directoryPath = "./tmp"; //место где хранится файлы с коммутаторами
    //std::cout << sizeof(Sigma2d)<< std::endl;
    auto start_time = Clock::now();//просто часы чтобы мерить время исполнения
    setlocale(LC_ALL, "");
    std::vector <Expression> H;//Переменная Гамильтониана
    std::vector<Expression> O_start;//Переменная Наблюдаемой
    size_t O_size = 0;
    
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
        O_start.emplace_back(str);
        O_size++;
        std::getline(file, str);
    }
    std::getline(file, str);
    if (str=="n:"){
        std::getline(file, str);
    }
    int n = std::stoi(str);
    file.close();
    std::cerr << "Файл считан корректно"<< std::endl;

    // Сохраняем нулевой слой (O из входного файла) на диск
    if(!std::filesystem::exists(directoryPath + "/0"))
        std::filesystem::create_directory(directoryPath + "/0");
    size_t prevTableSize = O_start.size();
    for(size_t i = 0; i < O_start.size(); i += BLOCK_SIZE) {
        std::vector<Expression> cur(O_start.begin() + i, O_start.begin() + std::min(O_start.size(), i + BLOCK_SIZE));
        storeBlock(directoryPath + "/0/" + std::to_string(i) + ".comm", cur);
    }

    for(int32_t t = 0; t < n; t++) {
        std::cerr << "Этап " << t + 1 << std::endl;
        if(!std::filesystem::exists(directoryPath + "/" + std::to_string(t + 1))) {
            std::filesystem::create_directory(directoryPath + "/" + std::to_string(t + 1));
            std::cerr << "Создана директория: " << t + 1 << std::endl;
        }

        auto time2 = Clock::now();
        size_t tableSize = H.size() * O_size;
        Node** indices = new Node*[tableSize];
        for(size_t i = 0; i < tableSize; i++)
            indices[i] = nullptr;
        std::mutex* indexMutexes = new std::mutex[(tableSize + 15) / 16];

        size_t nextHash = 0;
        std::mutex taskMutex;
        
        std::vector<std::thread> threads;
        size_t num_threads = std::thread::hardware_concurrency();
        for(size_t tt = 0; tt < num_threads; tt++) {
            threads.emplace_back([&nextHash, prevTableSize, &taskMutex, indices, tableSize, indexMutexes, &H, t]() {
                while(true) {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    if(nextHash >= prevTableSize)
                        return;
                    size_t curHash = nextHash;
                    nextHash += BLOCK_SIZE;
                    lock.unlock();
                    
                    std::vector<Expression> expressionsCur = loadAndDeleteHashTableBlock(directoryPath, t, curHash);
                    
                    for (size_t i = 0; i < H.size(); i++)
                        for(size_t j = 0; j < expressionsCur.size(); j++)
                            H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                                size_t hash = ExpressionHasher()(newExpression);
                                std::lock_guard<std::mutex> guard(indexMutexes[(hash % tableSize) / 16]);
                                Node* cur_node = indices[hash % tableSize];
                                bool found = false;
                                while(cur_node != nullptr) {
                                    if(cur_node->expr == newExpression) {
                                        cur_node->expr.getPoly() += newExpression.getPoly();
                                        found = true;
                                        break;
                                    }
                                    cur_node = cur_node->next;
                                }
                                if(!found) {
                                    indices[hash % tableSize] = new Node(std::move(newExpression), indices[hash % tableSize]);
                                }
                            }); //коммутируем все салгаемые с гамильтонанном.
                }
            });
        }
        for(size_t j = 0; j < threads.size(); j++)
            threads[j].join();
        
        delete[] indexMutexes;
        auto time3 = Clock::now();
        std::cerr << "Подсчет коммутаторов и приведение подобных: " << (std::chrono::duration_cast<std::chrono::microseconds>(time3 - time2).count())/1000000.0 << " сек" << std::endl;
        
        // Сохраняем новый слой на диск и удаляем его из памяти
        O_size = 0;
        for(size_t i = 0; i < tableSize; i += BLOCK_SIZE)
            O_size += moveHashTableBlockToDisk(directoryPath, t + 1, i, tableSize, indices);

        delete[] indices;
        prevTableSize = tableSize;
    
        std::cerr << std::endl;
    }
    
    //вывод результата, пока он делается просто на экран
    for(size_t i = 0; i < prevTableSize; i += BLOCK_SIZE)
        for(const auto& expr : loadAndDeleteHashTableBlock(directoryPath, n, i))
            expr.print();

auto time1 = Clock::now();
std::cerr << "Вычисление заняло "<< (std::chrono::duration_cast<std::chrono::microseconds>(time1 - start_time).count())/1000000.0 << " сек" << std::endl;
//    getc(stdin);
//    getc(stdin);
        return 0;
}
