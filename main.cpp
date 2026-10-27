#include <cstdint>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <map>
#include <tuple>
#include "Expression.h"
#include "SerializationHelpers.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <unordered_set>
#include <cmath>
#include <iomanip>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Poly2.h"

struct Node {
    Expression expr;
    Node* next;

    Node(Expression&& expr_, Node* next_) : expr(std::move(expr_)) {
        next = next_;
    }
};

#define BLOCK_SIZE 2048
static constexpr size_t FILES_PER_SHARD = 65536;
static constexpr size_t SHARDING_THRESHOLD_FILES = 4'000'000;

struct Config {
    std::string directoryPath;
    size_t memoryLimitInBytes;
    bool overwriteMainDirectory;
    bool textOutput;
    int32_t startFromLayer = 0;
    bool hasExplicitMeta = false;
    size_t explicitOSize = 0;
    size_t explicitTableSize = 0;
    double explicitExpRatio = 0.0;
    double explicitAvgBytesPerHash = 0.0;
};
Config config;

// Вспомогательная сериализация 64-битных чисел для чекпоинтов
void serializeUInt64(std::vector<uint8_t>& res, uint64_t x) {
    serializeUInt32(res, static_cast<uint32_t>(x & 0xFFFFFFFFULL));
    serializeUInt32(res, static_cast<uint32_t>(x >> 32));
}

uint64_t deserializeUInt64(const std::vector<uint8_t>& data, size_t& ptr) {
    uint64_t lo = deserializeUInt32(data, ptr);
    uint64_t hi = deserializeUInt32(data, ptr);
    return lo | (hi << 32);
}

std::string getFlatPathToHashTableFile(size_t level, size_t start) {
    return config.directoryPath + "/" + std::to_string(level) + "/" + std::to_string(start) + ".comm";
}

std::string getShardedPathToHashTableFile(size_t level, size_t start) {
    size_t blockIdx = start / BLOCK_SIZE;
    size_t shardIdx = blockIdx / FILES_PER_SHARD;
    return config.directoryPath + "/" + std::to_string(level) + "/shard_" + std::to_string(shardIdx) + "/" + std::to_string(start) + ".comm";
}

std::string getRecoveredPathToHashTableFile(size_t level, size_t start) {
    return config.directoryPath + "/" + std::to_string(level) + "/recovered/" + std::to_string(start) + ".comm";
}

std::string resolvePathToHashTableFileForRead(size_t level, size_t start) {
    std::string flatPath = getFlatPathToHashTableFile(level, start);
    if (std::filesystem::exists(flatPath))
        return flatPath;
    std::string recPath = getRecoveredPathToHashTableFile(level, start);
    if (std::filesystem::exists(recPath))
        return recPath;
    std::string shardPath = getShardedPathToHashTableFile(level, start);
    if (std::filesystem::exists(shardPath))
        return shardPath;
    return flatPath;
}

std::string getPathToHashTableFileForWrite(size_t level, size_t start, size_t totalTableSize) {
    size_t totalFiles = (totalTableSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (totalFiles >= SHARDING_THRESHOLD_FILES) {
        return getShardedPathToHashTableFile(level, start);
    }
    return getFlatPathToHashTableFile(level, start);
}

std::vector<uint8_t> loadFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        std::cerr << "CRITICAL ERROR: Cannot open file for reading: " << path
                  << " (" << std::strerror(errno) << ")" << std::endl;
        std::abort();
    }
    std::streamsize size = stream.tellg();
    if (size < 0) {
        std::cerr << "CRITICAL ERROR: tellg() failed on file: " << path << std::endl;
        std::abort();
    }
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0 && !stream.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "CRITICAL ERROR: Failed to read " << size << " bytes from: " << path << std::endl;
        std::abort();
    }
    return data;
}

size_t getLogicalFileSize(const std::string& path) {
    struct stat stats{};
    if (stat(path.c_str(), &stats) != 0)
        return 0;
    return static_cast<size_t>(stats.st_size);
}

size_t getRealFileSize(const std::string& path) {
    struct stat stats{};
    if (stat(path.c_str(), &stats) != 0) {
        std::cerr << "CRITICAL ERROR: stat() failed on: " << path
                  << " (" << std::strerror(errno) << ")" << std::endl;
        std::abort();
    }
    return static_cast<size_t>(stats.st_blocks) * 512;
}

std::pair<size_t, size_t> storeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        std::filesystem::path fsPath(path);
        std::error_code ec;
        std::filesystem::create_directories(fsPath.parent_path(), ec);
        stream.open(path, std::ios::binary);
    }
    if (!stream) {
        std::cerr << "CRITICAL ERROR: Cannot open file for writing: " << path
                  << " (" << std::strerror(errno) << ")" << std::endl;
        std::abort();
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    stream.close();
    if (!stream) {
        std::cerr << "CRITICAL ERROR: Failed to write/flush file: " << path
                  << " (" << std::strerror(errno) << ")" << std::endl;
        std::abort();
    }

    size_t realFileSize = getRealFileSize(path);
    return {data.size(), realFileSize};
}

std::vector<Expression> loadAndDeleteBlock(const std::string& path) {
    std::vector<uint8_t> data = loadFile(path);

    std::vector<Expression> res;
    size_t ptr = 0;
    while (ptr < data.size())
        res.emplace_back(data, ptr);

    return res;
}

std::pair<size_t, size_t> storeBlock(const std::string& path, const std::vector<Expression>& exprs) {
    std::vector<uint8_t> data;
    for (size_t i = 0; i < exprs.size(); i++)
        exprs[i].serialize(data);
    return storeFile(path, data);
}

struct DiskMovementResult {
    size_t numCommutators;
    size_t writtenDiskSize;
    size_t realDiskSize;
};

DiskMovementResult moveHashTableBlockToDisk(size_t level, size_t start, size_t maxHash,
                                            size_t totalTableSize, Node** indices, size_t tableOffset) {
    std::vector<Expression> exprs;
    for (size_t i = start; i < std::min(start + BLOCK_SIZE, maxHash); i++) {
        Node* cur_node = indices[i - tableOffset];
        while (cur_node != nullptr) {
            Node* next_node = cur_node->next;
            if (!cur_node->expr.getPoly().isZero())
                exprs.push_back(std::move(cur_node->expr));
            delete cur_node;
            cur_node = next_node;
        }
        indices[i - tableOffset] = nullptr;
    }
    std::string outPath = getPathToHashTableFileForWrite(level, start, totalTableSize);
    auto [writtenDiskSize, realDiskSize] = storeBlock(outPath, exprs);
    return {exprs.size(), writtenDiskSize, realDiskSize};
}

DiskMovementResult moveHashTableToDisk(size_t level, Node** indices, size_t minHash,
                                       size_t maxHash, size_t totalTableSize) {
    size_t totalFiles = (totalTableSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (totalFiles >= SHARDING_THRESHOLD_FILES) {
        size_t firstShard = (minHash / BLOCK_SIZE) / FILES_PER_SHARD;
        size_t lastShard = ((maxHash - 1) / BLOCK_SIZE) / FILES_PER_SHARD;
        for (size_t s = firstShard; s <= lastShard; ++s) {
            std::filesystem::create_directories(
                config.directoryPath + "/" + std::to_string(level) + "/shard_" + std::to_string(s));
        }
    }

    DiskMovementResult res = {0, 0, 0};
    size_t nextHash = minHash;
    std::mutex taskMutex, resMutex;

    std::vector<std::thread> threads;
    size_t num_threads = std::thread::hardware_concurrency();
    for (size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&nextHash, &res, &taskMutex, &resMutex, indices, level, minHash, maxHash, totalTableSize]() {
            while (true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if (nextHash >= maxHash)
                    return;
                size_t curHash = nextHash;
                nextHash += BLOCK_SIZE;
                lock.unlock();

                auto [added, writtenDiskSize, realDiskSize] =
                    moveHashTableBlockToDisk(level, curHash, maxHash, totalTableSize, indices, minHash);

                std::lock_guard<std::mutex> lock2(resMutex);
                res.numCommutators += added;
                res.writtenDiskSize += writtenDiskSize;
                res.realDiskSize += realDiskSize;
            }
        });
    }
    for (size_t j = 0; j < threads.size(); j++)
        threads[j].join();

    return res;
}

size_t getMemoryUsage() {
    std::ifstream stream("/proc/self/status");
    std::string str;
    while (stream >> str) {
        if (str == "VmRSS:") {
            std::string value;
            stream >> value;

            std::string suffix;
            stream >> suffix;
            assert(suffix == "kB");

            return std::stoll(value) * 1024;
        }
    }
    return 0;
}

using Clock = std::chrono::high_resolution_clock;

struct ComputationResult {
    size_t O_size;
    size_t writtenDiskSize;
    size_t realDiskSize;
    size_t maxMemoryUsed;
};

ComputationResult computePartOfNextLayerImpl(const std::vector<Expression>& H, size_t prevTableSize,
                                             size_t curLevelNum, size_t curTableSize,
                                             size_t minHash, size_t maxHash) {
    assert(minHash % BLOCK_SIZE == 0);
    assert(maxHash >= curTableSize || maxHash % BLOCK_SIZE == 0);
    assert(maxHash > minHash);

    size_t memoryUsageStart = getMemoryUsage();

    auto timeLayerStart = Clock::now();
    Node** indices = new Node*[maxHash - minHash];
    for (size_t i = 0; i < maxHash - minHash; i++)
        indices[i] = nullptr;
    std::mutex* indexMutexes = new std::mutex[(maxHash - minHash + 15) / 16];

    size_t nextHash = 0;
    std::mutex taskMutex;

    std::vector<std::thread> threads;
    size_t num_threads = std::thread::hardware_concurrency();
    for (size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&nextHash, prevTableSize, &taskMutex, indices, curTableSize, indexMutexes, &H, curLevelNum, minHash, maxHash]() {
            while (true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                if (nextHash >= prevTableSize)
                    return;
                size_t curHash = nextHash;
                nextHash += BLOCK_SIZE;
                lock.unlock();

                std::vector<Expression> expressionsCur =
                    loadAndDeleteBlock(resolvePathToHashTableFileForRead(curLevelNum - 1, curHash));

                for (size_t i = 0; i < H.size(); i++)
                    for (size_t j = 0; j < expressionsCur.size(); j++)
                        H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                            size_t hash = ExpressionHasher()(newExpression);
                            if (hash % curTableSize < minHash || hash % curTableSize >= maxHash)
                                return;
                            std::lock_guard<std::mutex> guard(indexMutexes[(hash % curTableSize - minHash) / 16]);
                            Node* prev_node = nullptr;
                            Node* cur_node = indices[hash % curTableSize - minHash];
                            bool found = false;
                            while (cur_node != nullptr) {
                                if (cur_node->expr == newExpression) {
                                    cur_node->expr.getPoly() += newExpression.getPoly();
                                    found = true;
                                    break;
                                } else if (newExpression < cur_node->expr)
                                    break;
                                prev_node = cur_node;
                                cur_node = cur_node->next;
                            }
                            if (!found) {
                                if (prev_node == nullptr)
                                    indices[hash % curTableSize - minHash] =
                                        new Node(std::move(newExpression), indices[hash % curTableSize - minHash]);
                                else
                                    prev_node->next = new Node(std::move(newExpression), cur_node);
                            }
                        });
            }
        });
    }
    for (size_t j = 0; j < threads.size(); j++)
        threads[j].join();

    size_t memoryUsageEnd = getMemoryUsage();

    delete[] indexMutexes;
    auto timeLayerEnd = Clock::now();
    std::cerr << "\e[90m" << "Collecting and combining like terms: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeLayerEnd - timeLayerStart).count() / 1000000.0
              << " sec" << "\e[0m" << std::endl;

    auto [O_size, writtenDiskSize, realDiskSize] =
        moveHashTableToDisk(curLevelNum, indices, minHash, maxHash, curTableSize);
    delete[] indices;

    auto timeMoveEnd = Clock::now();
    std::cerr << "\e[90m" << "Saving to disk: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeMoveEnd - timeLayerEnd).count() / 1000000.0
              << " sec" << "\e[0m" << std::endl;
    return {O_size, writtenDiskSize, realDiskSize,
            (memoryUsageEnd >= memoryUsageStart) ? (memoryUsageEnd - memoryUsageStart) : 0};
}

ComputationResult computePartOfNextLayer(const std::vector<Expression>& H, size_t prevTableSize,
                                         size_t curLevelNum, size_t curTableSize,
                                         size_t minHash = 0, size_t maxHash = std::numeric_limits<size_t>::max()) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        std::cerr << "CRITICAL ERROR: pipe() failed" << std::endl;
        exit(1);
    }

    int fork_res = fork();
    if (fork_res < 0) {
        std::cerr << "CRITICAL ERROR: fork() failed" << std::endl;
        exit(1);
    }
    if (fork_res == 0) {
        close(pipefd[0]);

        ComputationResult res = computePartOfNextLayerImpl(H, prevTableSize, curLevelNum, curTableSize, minHash, maxHash);

        ssize_t written = 0;
        while (written < static_cast<ssize_t>(sizeof(ComputationResult))) {
            ssize_t w = write(pipefd[1], reinterpret_cast<char*>(&res) + written, sizeof(ComputationResult) - written);
            if (w <= 0) exit(1);
            written += w;
        }
        close(pipefd[1]);
        exit(0);
    } else {
        close(pipefd[1]);

        int wstatus;
        waitpid(fork_res, &wstatus, 0);
        if (!(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)) {
            std::cerr << "Failed to complete computation successfully (child exit status / signal)" << std::endl;
            exit(1);
        }

        ComputationResult res;
        ssize_t read_ = 0;
        while (read_ < static_cast<ssize_t>(sizeof(ComputationResult))) {
            ssize_t r = read(pipefd[0], reinterpret_cast<char*>(&res) + read_, sizeof(ComputationResult) - read_);
            if (r <= 0) {
                std::cerr << "CRITICAL ERROR: Failed to read ComputationResult from child pipe" << std::endl;
                exit(1);
            }
            read_ += r;
        }
        close(pipefd[0]);
        return res;
    }
}

struct LayerMeta {
    size_t O_size = 0;
    size_t tableSize = 0;
    double expansionRatio = 0.0;
    double avgBytesPerHash = 0.0;
    size_t writtenDiskSize = 0;
    size_t realDiskSize = 0;
};

void saveLayerMeta(size_t level, const LayerMeta& meta) {
    std::string metaPath = config.directoryPath + "/" + std::to_string(level) + "/layer_meta.txt";
    std::ofstream out(metaPath);
    if (out) {
        out << meta.O_size << " " << meta.tableSize << " "
            << std::setprecision(17) << meta.expansionRatio << " "
            << std::setprecision(17) << meta.avgBytesPerHash << " "
            << meta.writtenDiskSize << " " << meta.realDiskSize << "\n";
    }
}

bool loadLayerMeta(size_t level, LayerMeta& meta) {
    std::string metaPath = config.directoryPath + "/" + std::to_string(level) + "/layer_meta.txt";
    std::ifstream in(metaPath);
    if (!in) return false;
    return static_cast<bool>(in >> meta.O_size >> meta.tableSize >> meta.expansionRatio
                                >> meta.avgBytesPerHash >> meta.writtenDiskSize >> meta.realDiskSize);
}

LayerMeta scanLayerDirectory(size_t level, double defaultAvgBytesPerHash) {
    std::string layerPath = config.directoryPath + "/" + std::to_string(level);
    std::cerr << "Scanning existing layer " << level << " at " << layerPath << " ..." << std::endl;

    std::vector<std::string> files;
    size_t maxStart = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(layerPath);
         it != std::filesystem::recursive_directory_iterator{}; ++it) {
        if (it->is_regular_file() && it->path().extension() == ".comm") {
            files.push_back(it->path().string());
            size_t startVal = std::stoull(it->path().stem().string());
            if (startVal > maxStart) maxStart = startVal;
        }
    }

    std::atomic<size_t> nextIdx{0};
    std::atomic<size_t> totalTerms{0};
    std::atomic<size_t> totalWritten{0};
    std::atomic<size_t> totalReal{0};

    size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    for (size_t tt = 0; tt < num_threads; ++tt) {
        threads.emplace_back([&]() {
            size_t localTerms = 0, localWritten = 0, localReal = 0;
            while (true) {
                size_t idx = nextIdx.fetch_add(1, std::memory_order_relaxed);
                if (idx >= files.size()) break;
                const std::string& fpath = files[idx];
                localWritten += getLogicalFileSize(fpath);
                localReal += getRealFileSize(fpath);
                auto exprs = loadAndDeleteBlock(fpath);
                localTerms += exprs.size();
            }
            totalTerms.fetch_add(localTerms, std::memory_order_relaxed);
            totalWritten.fetch_add(localWritten, std::memory_order_relaxed);
            totalReal.fetch_add(localReal, std::memory_order_relaxed);
        });
    }
    for (auto& th : threads) th.join();

    LayerMeta meta;
    meta.O_size = totalTerms.load();
    meta.tableSize = files.empty() ? 0 : (maxStart + BLOCK_SIZE);
    meta.expansionRatio = 2.90;
    meta.avgBytesPerHash = defaultAvgBytesPerHash;
    meta.writtenDiskSize = totalWritten.load();
    meta.realDiskSize = totalReal.load();
    return meta;
}

// ============================================================================
// 1. `recover-layer` subcommand: recalculates lost `.comm` blocks in a single pass
// ============================================================================
int runRecoverLayer(const std::string& inputFile, const std::string& resDir,
                    size_t targetLevel, size_t prevTableSize, size_t curTableSize) {
    config.directoryPath = resDir;
    auto timeStart = Clock::now();

    std::vector<Expression> H;
    std::ifstream file(inputFile);
    if (!file) {
        std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
        return 1;
    }
    std::string str;
    std::getline(file, str);
    assert(str == "H:");
    std::getline(file, str);
    while (str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }
    file.close();
    std::cerr << "Hamiltonian parsed (" << H.size() << " terms)." << std::endl;

    size_t totalBlocks = (curTableSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    std::vector<bool> blockExists(totalBlocks, false);
    std::string targetDir = resDir + "/" + std::to_string(targetLevel);

    std::cerr << "Scanning directory " << targetDir << " for existing .comm files..." << std::endl;
    size_t presentCount = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(targetDir);
         it != std::filesystem::recursive_directory_iterator{}; ++it) {
        if (it->is_regular_file() && it->path().extension() == ".comm") {
            std::error_code ec;
            auto fsize = it->file_size(ec);
            if (!ec && fsize > 0) {
                size_t startVal = std::stoull(it->path().stem().string());
                size_t b = startVal / BLOCK_SIZE;
                if (b < totalBlocks && !blockExists[b]) {
                    blockExists[b] = true;
                    presentCount++;
                }
            }
        }
    }

    std::vector<size_t> missingStarts;
    std::vector<int64_t> blockToCompactIdx(totalBlocks, -1);
    for (size_t b = 0; b < totalBlocks; ++b) {
        if (!blockExists[b]) {
            blockToCompactIdx[b] = static_cast<int64_t>(missingStarts.size());
            missingStarts.push_back(b * BLOCK_SIZE);
        }
    }

    std::cerr << "Total expected blocks:     " << totalBlocks << std::endl;
    std::cerr << "Present valid blocks:      " << presentCount << std::endl;
    std::cerr << "Missing blocks to recover: " << missingStarts.size() << std::endl;

    if (missingStarts.empty()) {
        std::cerr << "Nothing to recover! All " << totalBlocks << " files are present." << std::endl;
        return 0;
    }

    size_t totalCompactSlots = missingStarts.size() * BLOCK_SIZE;
    std::cerr << "Allocating compact hash table for " << totalCompactSlots << " slots..." << std::endl;
    Node** indices = new Node*[totalCompactSlots];
    for (size_t i = 0; i < totalCompactSlots; ++i)
        indices[i] = nullptr;
    std::mutex* indexMutexes = new std::mutex[(totalCompactSlots + 15) / 16];

    size_t totalPrevBlocks = (prevTableSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    std::atomic<size_t> processedPrevBlocks{0};
    size_t nextHash = 0;
    std::mutex taskMutex;

    size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    auto timeComputeStart = Clock::now();
    for (size_t tt = 0; tt < num_threads; ++tt) {
        threads.emplace_back([&]() {
            while (true) {
                size_t curHash;
                {
                    std::lock_guard<std::mutex> lock(taskMutex);
                    if (nextHash >= prevTableSize)
                        return;
                    curHash = nextHash;
                    nextHash += BLOCK_SIZE;
                }

                std::vector<Expression> expressionsCur =
                    loadAndDeleteBlock(resolvePathToHashTableFileForRead(targetLevel - 1, curHash));

                for (size_t i = 0; i < H.size(); ++i) {
                    for (size_t j = 0; j < expressionsCur.size(); ++j) {
                        H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                            size_t slot = ExpressionHasher()(newExpression) % curTableSize;
                            size_t b = slot / BLOCK_SIZE;
                            int64_t compactBlock = blockToCompactIdx[b];
                            if (compactBlock < 0)
                                return;

                            size_t compactSlot = static_cast<size_t>(compactBlock) * BLOCK_SIZE + (slot % BLOCK_SIZE);
                            std::lock_guard<std::mutex> guard(indexMutexes[compactSlot / 16]);
                            Node* prev_node = nullptr;
                            Node* cur_node = indices[compactSlot];
                            bool found = false;
                            while (cur_node != nullptr) {
                                if (cur_node->expr == newExpression) {
                                    cur_node->expr.getPoly() += newExpression.getPoly();
                                    found = true;
                                    break;
                                } else if (newExpression < cur_node->expr) {
                                    break;
                                }
                                prev_node = cur_node;
                                cur_node = cur_node->next;
                            }
                            if (!found) {
                                if (prev_node == nullptr)
                                    indices[compactSlot] = new Node(std::move(newExpression), indices[compactSlot]);
                                else
                                    prev_node->next = new Node(std::move(newExpression), cur_node);
                            }
                        });
                    }
                }

                size_t done = processedPrevBlocks.fetch_add(1, std::memory_order_relaxed) + 1;
                if (done % 200000 == 0 || done == totalPrevBlocks) {
                    double pct = 100.0 * done / totalPrevBlocks;
                    std::cerr << "[RECOVER] Processed " << done << " / " << totalPrevBlocks
                              << " input blocks (" << std::fixed << std::setprecision(1) << pct << "%)" << std::endl;
                }
            }
        });
    }
    for (auto& th : threads)
        th.join();
    delete[] indexMutexes;

    auto timeComputeEnd = Clock::now();
    std::cerr << "Collecting missing terms took: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeComputeEnd - timeComputeStart).count() / 1000000.0
              << " sec" << std::endl;

    std::string recoveredDir = targetDir + "/recovered";
    std::filesystem::create_directories(recoveredDir);

    std::atomic<size_t> nextMissingIdx{0};
    std::atomic<size_t> recoveredTerms{0};
    std::atomic<size_t> recoveredBytes{0};
    std::vector<std::thread> saveThreads;

    for (size_t tt = 0; tt < num_threads; ++tt) {
        saveThreads.emplace_back([&]() {
            while (true) {
                size_t m = nextMissingIdx.fetch_add(1, std::memory_order_relaxed);
                if (m >= missingStarts.size())
                    break;

                size_t start = missingStarts[m];
                std::vector<Expression> exprs;
                for (size_t off = 0; off < BLOCK_SIZE && (start + off) < curTableSize; ++off) {
                    size_t compactSlot = m * BLOCK_SIZE + off;
                    Node* cur_node = indices[compactSlot];
                    while (cur_node != nullptr) {
                        Node* next_node = cur_node->next;
                        if (!cur_node->expr.getPoly().isZero())
                            exprs.push_back(std::move(cur_node->expr));
                        delete cur_node;
                        cur_node = next_node;
                    }
                    indices[compactSlot] = nullptr;
                }
                std::string outPath = getRecoveredPathToHashTableFile(targetLevel, start);
                auto [writtenDiskSize, realDiskSize] = storeBlock(outPath, exprs);
                recoveredTerms.fetch_add(exprs.size(), std::memory_order_relaxed);
                recoveredBytes.fetch_add(writtenDiskSize, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : saveThreads)
        th.join();
    delete[] indices;

    double totalSec = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - timeStart).count() / 1000000.0;
    std::cerr << "Successfully recovered " << missingStarts.size() << " files ("
              << recoveredTerms.load() << " terms, ~" << recoveredBytes.load() / 1'000'000
              << " MB) in " << totalSec << " sec!" << std::endl;
    return 0;
}

// ============================================================================
// 2. Next-layer-stats subcommand: calculates the moment and quench for layer n+1 IN MEMORY
//    without saving layer n+1 .comm files to disk (using window-based checkpoints)
// ============================================================================
void saveWindowStatsCheckpoint(const std::string& chkPath, size_t minHash, size_t maxHash,
                               size_t winOSize, size_t maxMemUsed,
                               const Poly& momentPoly, const Poly& quenchPoly) {
    std::vector<uint8_t> buf;
    serializeUInt64(buf, minHash);
    serializeUInt64(buf, maxHash);
    serializeUInt64(buf, winOSize);
    serializeUInt64(buf, maxMemUsed);
    momentPoly.serialize(buf);
    quenchPoly.serialize(buf);

    std::string tmpPath = chkPath + ".tmp";
    storeFile(tmpPath, buf);
    std::filesystem::rename(tmpPath, chkPath);
}

bool loadWindowStatsCheckpoint(const std::string& chkPath, size_t& minHash, size_t& maxHash,
                               size_t& winOSize, size_t& maxMemUsed,
                               Poly& momentPoly, Poly& quenchPoly) {
    if (!std::filesystem::exists(chkPath) || getLogicalFileSize(chkPath) == 0)
        return false;
    std::vector<uint8_t> buf = loadFile(chkPath);
    size_t ptr = 0;
    minHash = deserializeUInt64(buf, ptr);
    maxHash = deserializeUInt64(buf, ptr);
    winOSize = deserializeUInt64(buf, ptr);
    maxMemUsed = deserializeUInt64(buf, ptr);
    momentPoly = Poly(buf, ptr);
    quenchPoly = Poly(buf, ptr);
    return true;
}

ComputationResult computeNextLayerStatsWindowImpl(
    const std::vector<Expression>& H, size_t prevTableSize,
    size_t curLevelNum, size_t curTableSize,
    size_t minHash, size_t maxHash, const std::string& chkPath,
    bool computeMoment, bool computeQuench)
{
    assert(minHash % BLOCK_SIZE == 0);
    assert(maxHash >= curTableSize || maxHash % BLOCK_SIZE == 0);
    assert(maxHash > minHash);

    size_t memoryUsageStart = getMemoryUsage();
    auto timeLayerStart = Clock::now();

    Node** indices = new Node*[maxHash - minHash];
    for (size_t i = 0; i < maxHash - minHash; i++)
        indices[i] = nullptr;
    std::mutex* indexMutexes = new std::mutex[(maxHash - minHash + 15) / 16];

    size_t nextHash = 0;
    std::mutex taskMutex;
    size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&]() {
            while (true) {
                size_t curHash;
                {
                    std::lock_guard<std::mutex> lock(taskMutex);
                    if (nextHash >= prevTableSize)
                        return;
                    curHash = nextHash;
                    nextHash += BLOCK_SIZE;
                }

                std::vector<Expression> expressionsCur =
                    loadAndDeleteBlock(resolvePathToHashTableFileForRead(curLevelNum - 1, curHash));

                for (size_t i = 0; i < H.size(); i++) {
                    for (size_t j = 0; j < expressionsCur.size(); j++) {
                        H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                            size_t hash = ExpressionHasher()(newExpression);
                            size_t slot = hash % curTableSize;
                            if (slot < minHash || slot >= maxHash)
                                return;
                            size_t localSlot = slot - minHash;
                            std::lock_guard<std::mutex> guard(indexMutexes[localSlot / 16]);
                            Node* prev_node = nullptr;
                            Node* cur_node = indices[localSlot];
                            bool found = false;
                            while (cur_node != nullptr) {
                                if (cur_node->expr == newExpression) {
                                    cur_node->expr.getPoly() += newExpression.getPoly();
                                    found = true;
                                    break;
                                } else if (newExpression < cur_node->expr)
                                    break;
                                prev_node = cur_node;
                                cur_node = cur_node->next;
                            }
                            if (!found) {
                                if (prev_node == nullptr)
                                    indices[localSlot] = new Node(std::move(newExpression), indices[localSlot]);
                                else
                                    prev_node->next = new Node(std::move(newExpression), cur_node);
                            }
                        });
                    }
                }
            }
        });
    }
    for (auto& th : threads)
        th.join();

    size_t memoryUsageEnd = getMemoryUsage();
    delete[] indexMutexes;

    auto timeLayerEnd = Clock::now();
    std::cerr << "\e[90m" << "Collecting and combining like terms: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeLayerEnd - timeLayerStart).count() / 1000000.0
              << " sec" << "\e[0m" << std::endl;

    std::atomic<size_t> nextReduceSlot{minHash};
    std::atomic<size_t> totalTerms{0};
    std::mutex mergeMutex;
    Poly windowMoment;
    std::map<std::tuple<size_t, size_t, size_t>, Poly> globalQuenchMap;

    std::vector<std::thread> reduceThreads;
    for (size_t tt = 0; tt < num_threads; tt++) {
        reduceThreads.emplace_back([&]() {
            Poly localMoment;
            std::map<std::tuple<size_t, size_t, size_t>, Poly> localQuenchMap;
            size_t localCount = 0;

            while (true) {
                size_t start = nextReduceSlot.fetch_add(BLOCK_SIZE, std::memory_order_relaxed);
                if (start >= maxHash)
                    break;
                size_t end = std::min(start + BLOCK_SIZE, maxHash);
                for (size_t s = start; s < end; ++s) {
                    Node* cur_node = indices[s - minHash];
                    while (cur_node != nullptr) {
                        Node* next_node = cur_node->next;
                        if (!cur_node->expr.getPoly().isZero()) {
                            localCount++;
                            const Poly& p = cur_node->expr.getPoly();
                            if (computeMoment) {
                                localMoment += (p * p);
                            }
                            if (computeQuench) {
                                auto sigCounts = cur_node->expr.getNumSigmas();
                                localQuenchMap[sigCounts] += p;
                            }
                        }
                        delete cur_node;
                        cur_node = next_node;
                    }
                    indices[s - minHash] = nullptr;
                }
            }

            totalTerms.fetch_add(localCount, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(mergeMutex);
            if (computeMoment) {
                windowMoment += localMoment;
            }
            if (computeQuench) {
                for (const auto& [sigTriple, polySum] : localQuenchMap) {
                    globalQuenchMap[sigTriple] += polySum;
                }
            }
        });
    }
    for (auto& th : reduceThreads)
        th.join();
    delete[] indices;

    Poly windowQuench;
    if (computeQuench) {
        for (const auto& [sigTriple, polySum] : globalQuenchMap) {
            if (polySum.isZero())
                continue;
            auto [nx, ny, nz] = sigTriple;
            Poly multiplyBy = Poly("[1," + std::to_string(nx) + "," + std::to_string(ny) + "," + std::to_string(nz) + "]");
            windowQuench += polySum.prependVariables(3) * multiplyBy;
        }
    }

    size_t maxMem = (memoryUsageEnd >= memoryUsageStart) ? (memoryUsageEnd - memoryUsageStart) : 0;
    saveWindowStatsCheckpoint(chkPath, minHash, maxHash, totalTerms.load(), maxMem, windowMoment, windowQuench);

    auto timeReduceEnd = Clock::now();
    std::cerr << "\e[90m" << "In-memory stats reduction & checkpoint: "
              << std::chrono::duration_cast<std::chrono::microseconds>(timeReduceEnd - timeLayerEnd).count() / 1000000.0
              << " sec" << "\e[0m" << std::endl;

    return {totalTerms.load(), 0, 0, maxMem};
}

ComputationResult computeNextLayerStatsWindow(
    const std::vector<Expression>& H, size_t prevTableSize,
    size_t curLevelNum, size_t curTableSize,
    size_t minHash, size_t maxHash, const std::string& chkPath,
    bool computeMoment, bool computeQuench)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) std::abort();
    int fork_res = fork();
    if (fork_res < 0) std::abort();
    if (fork_res == 0) {
        close(pipefd[0]);
        ComputationResult res = computeNextLayerStatsWindowImpl(
            H, prevTableSize, curLevelNum, curTableSize, minHash, maxHash, chkPath, computeMoment, computeQuench);
        ssize_t written = 0;
        while (written < static_cast<ssize_t>(sizeof(ComputationResult))) {
            ssize_t w = write(pipefd[1], reinterpret_cast<char*>(&res) + written, sizeof(ComputationResult) - written);
            if (w <= 0) exit(1);
            written += w;
        }
        close(pipefd[1]);
        exit(0);
    } else {
        close(pipefd[1]);
        int wstatus;
        waitpid(fork_res, &wstatus, 0);
        if (!(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)) {
            std::cerr << "Child failed in computeNextLayerStatsWindow" << std::endl;
            exit(1);
        }
        ComputationResult res;
        ssize_t read_ = 0;
        while (read_ < static_cast<ssize_t>(sizeof(ComputationResult))) {
            ssize_t r = read(pipefd[0], reinterpret_cast<char*>(&res) + read_, sizeof(ComputationResult) - read_);
            if (r <= 0) exit(1);
            read_ += r;
        }
        close(pipefd[0]);
        return res;
    }
}

// Run: ./QCommute next-layer-stats <FILE> <RES_DIR> <BASE_LAYER> [--stat all|momentum|quench] [--memory-limit MB] [--layer-meta ...]
int runNextLayerStats(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " next-layer-stats [FILE] [RES_DIR] [BASE_LAYER] "
                  << "[--stat all|momentum|quench] [--memory-limit MB] [--layer-meta O_size tableSize expRatio avgBytes]" << std::endl;
        return 1;
    }
    std::string inputFile = argv[2];
    config.directoryPath = argv[3];
    size_t baseLayer = std::stoull(argv[4]);
    size_t targetLayer = baseLayer + 1;
    config.memoryLimitInBytes = std::numeric_limits<size_t>::max();
    config.hasExplicitMeta = false;
    bool computeMoment = true;
    bool computeQuench = true;

    for (int i = 5; i < argc;) {
        if (std::string(argv[i]) == "--memory-limit") {
            config.memoryLimitInBytes = std::stoull(argv[i + 1]) * 1'048'576;
            i += 2;
        } else if (std::string(argv[i]) == "--stat") {
            std::string mode = argv[i + 1];
            if (mode == "all") {
                computeMoment = true;
                computeQuench = true;
            } else if (mode == "momentum" || mode == "moment") {
                computeMoment = true;
                computeQuench = false;
            } else if (mode == "quench") {
                computeMoment = false;
                computeQuench = true;
            } else {
                std::cerr << "Unknown --stat mode: " << mode << " (expected: all, momentum, or quench)" << std::endl;
                return 1;
            }
            i += 2;
        } else if (std::string(argv[i]) == "--layer-meta") {
            config.hasExplicitMeta = true;
            config.explicitOSize = std::stoull(argv[i + 1]);
            config.explicitTableSize = std::stoull(argv[i + 2]);
            config.explicitExpRatio = std::stod(argv[i + 3]);
            config.explicitAvgBytesPerHash = std::stod(argv[i + 4]);
            i += 5;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            return 1;
        }
    }

    std::vector<Expression> H;
    std::ifstream file(inputFile);
    if (!file) {
        std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
        return 1;
    }
    std::string str;
    std::getline(file, str);
    assert(str == "H:");
    std::getline(file, str);
    while (str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }
    file.close();

    LayerMeta meta;
    if (config.hasExplicitMeta) {
        meta.O_size = config.explicitOSize;
        meta.tableSize = config.explicitTableSize;
        meta.expansionRatio = config.explicitExpRatio;
        meta.avgBytesPerHash = config.explicitAvgBytesPerHash;
    } else if (!loadLayerMeta(baseLayer, meta)) {
        meta = scanLayerDirectory(baseLayer, 372.20);
    }

    size_t O_size = meta.O_size;
    size_t prevTableSize = meta.tableSize;
    double prevExpansionRatio = meta.expansionRatio;
    double averageBytesPerHash = meta.avgBytesPerHash;
    size_t tableSize = std::max<size_t>(1, static_cast<size_t>(std::ceil(prevExpansionRatio * O_size)));

    std::string statsDir = config.directoryPath + "/stats_layer_" + std::to_string(targetLayer);
    std::filesystem::create_directories(statsDir);

    std::cerr << "[next-layer-stats] Computing in-memory stats (moment=" << computeMoment
              << ", quench=" << computeQuench << ") for Layer " << targetLayer
              << " from Layer " << baseLayer << std::endl;

    auto safeSubtract = [](size_t x, size_t y) { return (x >= y) ? (x - y) : 0; };
    size_t memoryUsagePerOldExpression = safeSubtract(averageBytesPerHash, (sizeof(Node) - sizeof(Expression)) + sizeof(std::mutex) / 16);
    size_t fixedMemoryUsage = std::thread::hardware_concurrency() * (BLOCK_SIZE * memoryUsagePerOldExpression + 131'072);

    size_t cur_O_size = 0;
    size_t sumOfMemoryUsages = 0;
    size_t windowIndex = 0;
    Poly2 totalMoment;
    Poly2 totalQuench;

    auto timeLayerStart = Clock::now();
    for (size_t i = 0; i < tableSize;) {
        std::string chkPath = statsDir + "/window_" + std::to_string(windowIndex) + ".chk";
        size_t chkMin = 0, chkMax = 0, chkOSize = 0, chkMem = 0;
        Poly winMoment, winQuench;

        if (loadWindowStatsCheckpoint(chkPath, chkMin, chkMax, chkOSize, chkMem, winMoment, winQuench) && chkMin == i) {
            size_t numHashes = chkMax - chkMin;
            cur_O_size += chkOSize;
            sumOfMemoryUsages += chkMem;
            averageBytesPerHash = std::max((double)sumOfMemoryUsages / (i + numHashes), (double)(sizeof(Node) + sizeof(Node*)));
            if (computeMoment) totalMoment += Poly2(winMoment);
            if (computeQuench) totalQuench += Poly2(winQuench);
            std::cerr << "[CHECKPOINT] Loaded window=" << windowIndex << " slots=[" << chkMin << ", " << chkMax
                      << ") O_size_so_far=" << cur_O_size << std::endl;
            i = chkMax;
            windowIndex++;
            continue;
        }

        size_t maxNumHashes = safeSubtract(config.memoryLimitInBytes, fixedMemoryUsage) / averageBytesPerHash;
        size_t numHashes = std::min(tableSize - i, std::max<size_t>(BLOCK_SIZE, maxNumHashes - maxNumHashes % BLOCK_SIZE));

        std::cerr << "[DIAG]   window=" << windowIndex
                  << " slots=[" << i << ", " << i + numHashes << ")"
                  << " maxNumHashes=" << maxNumHashes << std::endl;

        ComputationResult res = computeNextLayerStatsWindow(
            H, prevTableSize, targetLayer, tableSize, i, i + numHashes, chkPath, computeMoment, computeQuench);

        bool ok = loadWindowStatsCheckpoint(chkPath, chkMin, chkMax, chkOSize, chkMem, winMoment, winQuench);
        assert(ok);
        if (computeMoment) totalMoment += Poly2(winMoment);
        if (computeQuench) totalQuench += Poly2(winQuench);

        cur_O_size += res.O_size;
        sumOfMemoryUsages += res.maxMemoryUsed;
        averageBytesPerHash = std::max((double)sumOfMemoryUsages / (i + numHashes), (double)(sizeof(Node) + sizeof(Node*)));

        std::cerr << "[DIAG]   window=" << windowIndex
                  << " O_size_so_far=" << cur_O_size
                  << " rawMemUsed=" << res.maxMemoryUsed / 1'000'000 << " MB"
                  << " avgBytesPerHash=" << std::fixed << std::setprecision(2) << averageBytesPerHash << std::endl;

        i += numHashes;
        windowIndex++;
    }

    double totalTime = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - timeLayerStart).count() / 1000000.0;
    std::cerr << "Layer " << targetLayer << " stats completed in " << totalTime
              << " sec; total unique terms: " << cur_O_size << std::endl;

    std::cout << "(* Layer " << targetLayer << " total terms: " << cur_O_size << " *)" << std::endl;
    if (computeMoment) {
        std::cout << "moment_" << targetLayer << " = ";
        if (totalMoment == Poly2()) std::cout << "0";
        else totalMoment.printAsMathematica(0, [](size_t ind) { return "h" + std::to_string(ind); });
        std::cout << ";" << std::endl << std::endl;
    }
    if (computeQuench) {
        std::cout << "quenchCoef_" << targetLayer << " = ";
        if (totalQuench == Poly2()) std::cout << "0";
        else totalQuench.printAsMathematica(3, [](size_t ind) {
            if (ind < 3) return "p" + std::string(1, 'x' + ind);
            return "h" + std::to_string(ind - 3);
        });
        std::cout << ";" << std::endl;
    }

    return 0;
}

// ============================================================================
// 3. The quench-z subcommand: an algorithm for computing an additional m steps for z-polarization
// ============================================================================
inline bool passesXYFilter(const Expression& expr, size_t maxXY) {
    auto [nx, ny, nz] = expr.getNumSigmas();
    return (nx <= maxXY) && (ny <= maxXY);
}

ComputationResult computeFilteredLayerWindowImpl(
    const std::vector<Expression>& H, size_t prevTableSize,
    const std::string& prevLayerDir, const std::string& curLayerDir,
    size_t curTableSize, size_t minHash, size_t maxHash,
    size_t maxXY_in, size_t maxXY_out)
{
    size_t memoryUsageStart = getMemoryUsage();

    Node** indices = new Node*[maxHash - minHash];
    for (size_t i = 0; i < maxHash - minHash; i++)
        indices[i] = nullptr;
    std::mutex* indexMutexes = new std::mutex[(maxHash - minHash + 15) / 16];

    size_t nextHash = 0;
    std::mutex taskMutex;
    size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&]() {
            while (true) {
                size_t curHash;
                {
                    std::lock_guard<std::mutex> lock(taskMutex);
                    if (nextHash >= prevTableSize) return;
                    curHash = nextHash;
                    nextHash += BLOCK_SIZE;
                }

                std::string fpath = prevLayerDir + "/" + std::to_string(curHash) + ".comm";
                if (!std::filesystem::exists(fpath)) {
                    std::string rec = prevLayerDir + "/recovered/" + std::to_string(curHash) + ".comm";
                    if (std::filesystem::exists(rec)) fpath = rec;
                    else {
                        size_t shardIdx = (curHash / BLOCK_SIZE) / FILES_PER_SHARD;
                        fpath = prevLayerDir + "/shard_" + std::to_string(shardIdx) + "/" + std::to_string(curHash) + ".comm";
                    }
                }

                std::vector<Expression> expressionsCur = loadAndDeleteBlock(fpath);

                for (size_t j = 0; j < expressionsCur.size(); j++) {
                    if (!passesXYFilter(expressionsCur[j], maxXY_in))
                        continue;

                    for (size_t i = 0; i < H.size(); i++) {
                        H[i].Commute(expressionsCur[j], [&](Expression&& newExpression) {
                            if (!passesXYFilter(newExpression, maxXY_out))
                                return;

                            size_t hash = ExpressionHasher()(newExpression);
                            size_t slot = hash % curTableSize;
                            if (slot < minHash || slot >= maxHash)
                                return;

                            size_t localSlot = slot - minHash;
                            std::lock_guard<std::mutex> guard(indexMutexes[localSlot / 16]);
                            Node* prev_node = nullptr;
                            Node* cur_node = indices[localSlot];
                            bool found = false;
                            while (cur_node != nullptr) {
                                if (cur_node->expr == newExpression) {
                                    cur_node->expr.getPoly() += newExpression.getPoly();
                                    found = true;
                                    break;
                                } else if (newExpression < cur_node->expr)
                                    break;
                                prev_node = cur_node;
                                cur_node = cur_node->next;
                            }
                            if (!found) {
                                if (prev_node == nullptr)
                                    indices[localSlot] = new Node(std::move(newExpression), indices[localSlot]);
                                else
                                    prev_node->next = new Node(std::move(newExpression), cur_node);
                            }
                        });
                    }
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    size_t memoryUsageEnd = getMemoryUsage();
    delete[] indexMutexes;

    size_t totalFiles = (curTableSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    bool useShards = (totalFiles >= SHARDING_THRESHOLD_FILES);
    if (useShards) {
        size_t firstShard = (minHash / BLOCK_SIZE) / FILES_PER_SHARD;
        size_t lastShard = ((maxHash - 1) / BLOCK_SIZE) / FILES_PER_SHARD;
        for (size_t s = firstShard; s <= lastShard; ++s)
            std::filesystem::create_directories(curLayerDir + "/shard_" + std::to_string(s));
    }

    std::atomic<size_t> nextSaveHash{minHash};
    std::atomic<size_t> totalAdded{0}, totalWritten{0}, totalReal{0};
    std::vector<std::thread> saveThreads;

    for (size_t tt = 0; tt < num_threads; tt++) {
        saveThreads.emplace_back([&]() {
            while (true) {
                size_t start = nextSaveHash.fetch_add(BLOCK_SIZE, std::memory_order_relaxed);
                if (start >= maxHash) break;

                std::vector<Expression> exprs;
                for (size_t i = start; i < std::min(start + BLOCK_SIZE, maxHash); i++) {
                    Node* cur_node = indices[i - minHash];
                    while (cur_node != nullptr) {
                        Node* next_node = cur_node->next;
                        if (!cur_node->expr.getPoly().isZero())
                            exprs.push_back(std::move(cur_node->expr));
                        delete cur_node;
                        cur_node = next_node;
                    }
                    indices[i - minHash] = nullptr;
                }
                std::string outPath;
                if (useShards) {
                    size_t shardIdx = (start / BLOCK_SIZE) / FILES_PER_SHARD;
                    outPath = curLayerDir + "/shard_" + std::to_string(shardIdx) + "/" + std::to_string(start) + ".comm";
                } else {
                    outPath = curLayerDir + "/" + std::to_string(start) + ".comm";
                }
                auto [wSize, rSize] = storeBlock(outPath, exprs);
                totalAdded.fetch_add(exprs.size(), std::memory_order_relaxed);
                totalWritten.fetch_add(wSize, std::memory_order_relaxed);
                totalReal.fetch_add(rSize, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : saveThreads) th.join();
    delete[] indices;

    return {totalAdded.load(), totalWritten.load(), totalReal.load(),
            (memoryUsageEnd >= memoryUsageStart) ? (memoryUsageEnd - memoryUsageStart) : 0};
}

ComputationResult computeFilteredLayerWindow(
    const std::vector<Expression>& H, size_t prevTableSize,
    const std::string& prevLayerDir, const std::string& curLayerDir,
    size_t curTableSize, size_t minHash, size_t maxHash,
    size_t maxXY_in, size_t maxXY_out)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) std::abort();
    int fork_res = fork();
    if (fork_res == 0) {
        close(pipefd[0]);
        ComputationResult res = computeFilteredLayerWindowImpl(
            H, prevTableSize, prevLayerDir, curLayerDir, curTableSize, minHash, maxHash, maxXY_in, maxXY_out);
        ssize_t written = 0;
        while (written < static_cast<ssize_t>(sizeof(ComputationResult)))
            written += write(pipefd[1], reinterpret_cast<char*>(&res) + written, sizeof(ComputationResult) - written);
        close(pipefd[1]);
        exit(0);
    } else {
        close(pipefd[1]);
        int wstatus;
        waitpid(fork_res, &wstatus, 0);
        if (!(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)) {
            std::cerr << "Child failed in computeFilteredLayerWindow" << std::endl;
            exit(1);
        }
        ComputationResult res;
        ssize_t read_ = 0;
        while (read_ < static_cast<ssize_t>(sizeof(ComputationResult)))
            read_ += read(pipefd[0], reinterpret_cast<char*>(&res) + read_, sizeof(ComputationResult) - read_);
        close(pipefd[0]);
        return res;
    }
}

struct QuenchScanStats {
    Poly2 zQuench;
    size_t countIn = 0;       // Number of terms with nx, ny <= maxXY_in
    size_t countNextCore = 0; // The number of terms that already satisfy nx, ny <= maxXY_next
    size_t maxStart = 0;
};

QuenchScanStats extractZQuenchAndCountB0(const std::string& layerDir,
                                         size_t maxXY_in, size_t maxXY_next) {
    auto it = std::filesystem::recursive_directory_iterator(layerDir);
    Poly2 res;
    std::atomic<size_t> b0Count{0};
    std::atomic<size_t> nextCoreCount{0};
    std::atomic<size_t> maxStartAtom{0};
    std::mutex taskMutex, resMutex;
    size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (size_t tt = 0; tt < num_threads; tt++) {
        threads.emplace_back([&]() {
            std::map<size_t, Poly> localZMap;
            size_t localB0 = 0;
            size_t localNextCore = 0;
            size_t localMaxStart = 0;
            while (true) {
                std::unique_lock<std::mutex> lock(taskMutex);
                while (it != std::filesystem::recursive_directory_iterator{} &&
                       !(it->is_regular_file() && it->path().extension() == ".comm")) {
                    ++it;
                }
                if (it == std::filesystem::recursive_directory_iterator{})
                    break;
                std::string filepath = it->path().string();
                size_t startVal = std::stoull(it->path().stem().string());
                ++it;
                lock.unlock();

                if (startVal > localMaxStart) localMaxStart = startVal;

                std::vector<Expression> expressions = loadAndDeleteBlock(filepath);
                for (const Expression& expr : expressions) {
                    auto [nx, ny, nz] = expr.getNumSigmas();
                    if (nx <= maxXY_in && ny <= maxXY_in) {
                        localB0++;
                        if (nx <= maxXY_next && ny <= maxXY_next)
                            localNextCore++;
                    }
                    if (nx == 0 && ny == 0) {
                        localZMap[nz] += expr.getPoly();
                    }
                }
            }
            Poly2 localRes;
            for (const auto& [nz, pSum] : localZMap) {
                if (!pSum.isZero()) {
                    Poly multiplyBy = Poly("[1," + std::to_string(nz) + "]");
                    localRes += Poly2(pSum.prependVariables(1) * multiplyBy);
                }
            }
            b0Count.fetch_add(localB0, std::memory_order_relaxed);
            nextCoreCount.fetch_add(localNextCore, std::memory_order_relaxed);
            size_t curMax = maxStartAtom.load();
            while (localMaxStart > curMax && !maxStartAtom.compare_exchange_weak(curMax, localMaxStart)) {}
            std::lock_guard<std::mutex> lock2(resMutex);
            res += localRes;
        });
    }
    for (auto& th : threads) th.join();
    return {res, b0Count.load(), nextCoreCount.load(), maxStartAtom.load()};
}

int runQuenchZExtension(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " quench-z [FILE] [RES_DIR] [N_MAX] [M] "
                  << "[--memory-limit MB] [--exp-ratio R] [--keep-temp]" << std::endl;
        return 1;
    }
    std::string inputFile = argv[2];
    config.directoryPath = argv[3];
    size_t n_max = std::stoull(argv[4]);
    size_t m = std::stoull(argv[5]);
    config.memoryLimitInBytes = std::numeric_limits<size_t>::max();
    bool keepTemp = false;
    double userExpRatio = 0.0;

    for (int i = 6; i < argc;) {
        if (std::string(argv[i]) == "--memory-limit") {
            config.memoryLimitInBytes = std::stoull(argv[i + 1]) * 1'048'576;
            i += 2;
        } else if (std::string(argv[i]) == "--exp-ratio") {
            userExpRatio = std::stod(argv[i + 1]);
            i += 2;
        } else if (std::string(argv[i]) == "--keep-temp") {
            keepTemp = true;
            i++;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            return 1;
        }
    }

    std::vector<Expression> H;
    std::ifstream file(inputFile);
    std::string str;
    std::getline(file, str);
    assert(str == "H:");
    std::getline(file, str);
    while (str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }
    file.close();

    // Determine the base growth multiplier for the model's full layer (1D / 2D / 3D)
    LayerMeta meta;
    double modelExpRatio = (userExpRatio > 0.0) ? userExpRatio :
                           (loadLayerMeta(n_max, meta) && meta.expansionRatio > 1.0) ? meta.expansionRatio :
                           std::max(1.8, 0.95 * static_cast<double>(H.size()));

    std::string baseLayerDir = config.directoryPath + "/" + std::to_string(n_max);
    std::cerr << "Scanning layer n_max=" << n_max << " for B_0 = >L^" << n_max << " A<_" << m
              << " (modelExpRatio=" << modelExpRatio << ") ..." << std::endl;

    size_t firstOutFilter = (m >= 1) ? (m - 1) : 0;
    QuenchScanStats scan0 = extractZQuenchAndCountB0(baseLayerDir, m, firstOutFilter);
    size_t prevTableSize = scan0.maxStart + BLOCK_SIZE;
    size_t O_size = scan0.countIn;
    size_t nextCoreCount = scan0.countNextCore;

    std::cerr << "B_0 size (nx, ny <= " << m << "): " << O_size
              << ", core for next filter (nx, ny <= " << firstOutFilter << "): " << nextCoreCount
              << " (prevTableSize=" << prevTableSize << ")" << std::endl;

    auto printQuenchPoly = [](const Poly2& p) {
        if (p == Poly2()) {
            std::cout << "0";
        } else {
            p.printAsMathematica(1, [](size_t ind) {
                if (ind == 0) return std::string("pz");
                return "h" + std::to_string(ind - 1);
            });
        }
    };

    std::cout << "quenchZCoef = {" << std::endl;
    std::cout << "(* Step " << n_max << " *)" << std::endl;
    printQuenchPoly(scan0.zQuench);

    std::string workRoot = config.directoryPath + "/quench_z_from_" + std::to_string(n_max) + "_m" + std::to_string(m);
    std::filesystem::create_directories(workRoot);

    double averageBytesPerHash = 380.0;
    std::string prevDir = baseLayerDir;

    for (size_t k = 1; k <= m; ++k) {
        size_t maxXY_in  = (k == 1) ? m : (m - k + 1);
        size_t maxXY_out = m - k;
        std::string curDir = workRoot + "/B_" + std::to_string(k);
        if (std::filesystem::exists(curDir))
            std::filesystem::remove_all(curDir);
        std::filesystem::create_directories(curDir);

        //  Universal estimate of the size $B_k$ via the measured core count (nextCoreCount)
        size_t baseForPrediction = (nextCoreCount > 0) ? nextCoreCount : std::max<size_t>(1, O_size / 8);
        size_t tableSize = std::max<size_t>(
            BLOCK_SIZE,
            static_cast<size_t>(std::ceil(modelExpRatio * static_cast<double>(baseForPrediction)))
        );

        std::cerr << "\n\e[32m[quench-z] Step k=" << k << "/" << m
                  << " (Layer " << n_max + k << ", filter >...<_" << maxXY_out << ")\e[0m"
                  << " input_B_" << k - 1 << "_size=" << O_size
                  << " core_size=" << nextCoreCount
                  << " tableSize=" << tableSize << std::endl;

        auto safeSubtract = [](size_t x, size_t y) { return (x >= y) ? (x - y) : 0; };
        size_t memoryUsagePerOldExpression = safeSubtract(averageBytesPerHash, (sizeof(Node) - sizeof(Expression)) + sizeof(std::mutex) / 16);
        size_t fixedMemoryUsage = std::thread::hardware_concurrency() * (BLOCK_SIZE * memoryUsagePerOldExpression + 131'072);

        size_t cur_O_size = 0;
        size_t sumOfMemoryUsages = 0;
        for (size_t i = 0; i < tableSize;) {
            size_t maxNumHashes = safeSubtract(config.memoryLimitInBytes, fixedMemoryUsage) / averageBytesPerHash;
            size_t numHashes = std::min(tableSize - i, std::max<size_t>(BLOCK_SIZE, maxNumHashes - maxNumHashes % BLOCK_SIZE));

            ComputationResult res = computeFilteredLayerWindow(
                H, prevTableSize, prevDir, curDir, tableSize, i, i + numHashes, maxXY_in, maxXY_out);

            cur_O_size += res.O_size;
            sumOfMemoryUsages += res.maxMemoryUsed;
            if (res.maxMemoryUsed > 0 && cur_O_size > 0) {
                // Divide the actual memory by the number of elements (so that sparse tables do not lead to an underestimate)
                averageBytesPerHash = std::clamp((double)sumOfMemoryUsages / cur_O_size, 320.0, 600.0);
            }
            i += numHashes;
        }

        size_t nextOutFilter = (m > k) ? (m - k - 1) : 0;
        QuenchScanStats scanCur = extractZQuenchAndCountB0(curDir, maxXY_out, nextOutFilter);

        std::cout << "," << std::endl;
        std::cout << "(* Step " << n_max + k << " *)" << std::endl;
        printQuenchPoly(scanCur.zQuench);
        std::cout.flush();

        if (!keepTemp && k >= 2) {
            std::filesystem::remove_all(prevDir);
        }

        O_size = cur_O_size;
        nextCoreCount = scanCur.countNextCore;
        prevTableSize = tableSize;
        prevDir = curDir;
    }

    if (!keepTemp && std::filesystem::exists(prevDir) && prevDir != baseLayerDir) {
        std::filesystem::remove_all(prevDir);
    }

    std::cout << std::endl << "};" << std::endl;
    return 0;
}

std::string help_usage1 = "validate [FILE1] [FILE2]";
std::string help_usage2 = "momentum [DIRECTORY] [OPTIONAL_ONLY_LAYER]";
std::string help_usage3 = "quench [DIRECTORY] [OPTIONAL_ONLY_LAYER]";
std::string help_usage4 = "recover-layer [FILE] [RES_DIR] [LAYER] [PREV_TABLE_SIZE] [CUR_TABLE_SIZE]";
std::string help_usage5 = "next-layer-stats [FILE] [RES_DIR] [BASE_LAYER] [--memory-limit MB] [--layer-meta ...]";
std::string help_usage6 = "quench-z [FILE] [RES_DIR] [N_MAX] [M] [--memory-limit MB] [--keep-temp]";
std::string help_usage7 = "--help | --version | [FILE] [OPTION]...";

int main(int argc, char** argv) {
    std::cerr << "QCommute v1.1  |  Copyright (c) 2025-2026 Ilya Shirokov, Viacheslav Khrushchev" << std::endl;
    std::cerr << "License: CC BY 4.0  <https://creativecommons.org/licenses/by/4.0/>" << std::endl;
    std::cerr << std::endl;

    if (argc <= 1) {
        std::cerr << "Usage: " << argv[0] << " " << help_usage7 << std::endl;
        std::cerr << "Run '" << argv[0] << " --help' for full usage information." << std::endl;
        return 1;
    }

    if (std::string(argv[1]) == "--version") {
        return 0;
    } else if (std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " " << help_usage1 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage2 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage3 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage4 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage5 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage6 << std::endl;
        std::cout << "   or: " << argv[0] << " " << help_usage7 << std::endl;
        return 0;
    } else if (std::string(argv[1]) == "recover-layer") {
        if (argc < 7) {
            std::cerr << "Usage: " << argv[0] << " " << help_usage4 << std::endl;
            return 1;
        }
        return runRecoverLayer(argv[2], argv[3], std::stoull(argv[4]), std::stoull(argv[5]), std::stoull(argv[6]));
    } else if (std::string(argv[1]) == "next-layer-stats") {
        return runNextLayerStats(argc, argv);
    } else if (std::string(argv[1]) == "quench-z") {
        return runQuenchZExtension(argc, argv);
    } else if (std::string(argv[1]) == "validate") {
        std::vector<Expression> src, new_;
        std::string str;

        std::ifstream file_src(argv[2], std::ifstream::in);
        while (std::getline(file_src, str))
            src.push_back(Expression(str));

        std::ifstream file_new(argv[3], std::ifstream::in);
        while (std::getline(file_new, str))
            new_.push_back(Expression(str));

        std::unordered_set<Expression, ExpressionHasher> remain_src;
        for (size_t i = 0; i < src.size(); i++)
            remain_src.insert(src[i]);
        if (remain_src.size() != src.size()) {
            std::cout << "The first file contains duplicate terms, validation failed" << std::endl;
            return 0;
        }
        if (src.size() != new_.size()) {
            std::cout << "The files have a different number of terms" << std::endl;
            return 0;
        }

        for (size_t i = 0; i < new_.size(); i++) {
            auto it = remain_src.find(new_[i]);
            if (it == remain_src.end()) {
                std::cout << "Files do not match" << std::endl;
                return 0;
            }
            if (!(it->getPoly() == new_[i].getPoly())) {
                std::cout << "Files do not match" << std::endl;
                return 0;
            }
            remain_src.erase(it);
        }
        std::cout << "Files match" << std::endl;
        return 0;
    } else if (std::string(argv[1]) == "momentum" || std::string(argv[1]) == "quench") {
        if (argc <= 2) {
            std::cout << "Please specify the path to the computed commutators" << std::endl;
            return 1;
        }
        std::string directoryPath(argv[2]);
        size_t startStep = 1;
        size_t endStep = std::numeric_limits<size_t>::max();
        if (argc >= 4) {
            startStep = std::stoull(argv[3]);
            endStep = startStep;
        }

        auto mapToPolyAndSum = [&](const std::string& layerPath, std::function<Poly(const Expression& expr)> f) {
            auto it = std::filesystem::recursive_directory_iterator(layerPath);
            Poly2 res;
            std::vector<std::thread> threads;
            size_t num_threads = std::thread::hardware_concurrency();
            std::mutex taskMutex, resMutex;
            for (size_t tt = 0; tt < num_threads; tt++) {
                threads.emplace_back([&it, &res, &taskMutex, &resMutex, &f]() {
                    Poly2 resCur;
                    while (true) {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        while (it != std::filesystem::recursive_directory_iterator{} &&
                               !(it->is_regular_file() && it->path().extension() == ".comm")) {
                            ++it;
                        }
                        if (it == std::filesystem::recursive_directory_iterator{})
                            break;
                        std::string filepath = it->path().string();
                        ++it;
                        lock.unlock();

                        std::vector<Expression> expressions = loadAndDeleteBlock(filepath);
                        for (const Expression& expr : expressions)
                            resCur += Poly2(f(expr));
                    }
                    std::lock_guard<std::mutex> lock2(resMutex);
                    res += resCur;
                });
            }
            for (size_t j = 0; j < threads.size(); j++)
                threads[j].join();

            return res;
        };

        std::cout << ((std::string(argv[1]) == "momentum") ? "moments = {" : "quenchCoef = {") << std::endl;
        for (size_t i = startStep; i <= endStep; i++) {
            std::string layerPath = directoryPath + "/" + std::to_string(i);
            if (!std::filesystem::exists(layerPath))
                break;

            std::cerr << "\e[32m" << "Step " << i << "\e[0m" << std::endl;
            auto timeLayerStart = Clock::now();

            if (std::string(argv[1]) == "momentum") {
                Poly2 momentum = mapToPolyAndSum(layerPath, [](const Expression& expr) {
                    return expr.getPoly() * expr.getPoly();
                });
                if (i > startStep)
                    std::cout << "," << std::endl;
                momentum.printAsMathematica(0, [](size_t ind) {
                    return "h" + std::to_string(ind);
                });
                std::cerr << "Commutator moment " << i << " computed" << std::endl;
            } else {
                Poly2 coef = mapToPolyAndSum(layerPath, [](const Expression& expr) {
                    auto [numSigmaX, numSigmaY, numSigmaZ] = expr.getNumSigmas();
                    Poly multiplyBy = Poly("[1," + std::to_string(numSigmaX) + "," + std::to_string(numSigmaY) + "," + std::to_string(numSigmaZ) + "]");
                    return expr.getPoly().prependVariables(3) * multiplyBy;
                });
                if (i > startStep)
                    std::cout << "," << std::endl;
                coef.printAsMathematica(3, [](size_t ind) {
                    if (ind < 3)
                        return "p" + std::string(1, 'x' + ind);
                    return "h" + std::to_string(ind - 3);
                });
                std::cerr << "Quench coefficient of commutator " << i << " computed" << std::endl;
            }

            double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - timeLayerStart).count() / 1000000.0;
            std::cerr << "Total time on step: " << timeOnCurLayer << " sec" << std::endl;
            std::cerr << std::endl;
        }
        std::cout << std::endl;
        std::cout << "};" << std::endl;
        return 0;
    }

    config.directoryPath = "./tmp";
    config.memoryLimitInBytes = std::numeric_limits<size_t>::max();
    config.overwriteMainDirectory = false;
    config.textOutput = false;
    config.startFromLayer = 0;

    for (int i = 2; i < argc;) {
        if (std::string(argv[i]) == "--memory-limit") {
            config.memoryLimitInBytes = std::stoull(argv[i + 1]) * 1'048'576;
            i += 2;
        } else if (std::string(argv[i]) == "--res-dir") {
            config.directoryPath = std::string(argv[i + 1]);
            i += 2;
        } else if (std::string(argv[i]) == "--force") {
            config.overwriteMainDirectory = true;
            i++;
        } else if (std::string(argv[i]) == "--text-output") {
            config.textOutput = true;
            i++;
        } else if (std::string(argv[i]) == "--start-from-layer") {
            config.startFromLayer = std::stoi(argv[i + 1]);
            i += 2;
        } else if (std::string(argv[i]) == "--layer-meta") {
            config.hasExplicitMeta = true;
            config.explicitOSize = std::stoull(argv[i + 1]);
            config.explicitTableSize = std::stoull(argv[i + 2]);
            config.explicitExpRatio = std::stod(argv[i + 3]);
            config.explicitAvgBytesPerHash = std::stod(argv[i + 4]);
            i += 5;
        } else {
            std::cerr << "Unknown command-line argument: \"" << argv[i] << "\"" << std::endl;
            return 1;
        }
    }

    setlocale(LC_ALL, "");

    std::vector<Expression> H;
    std::vector<Expression> O_start;
    size_t O_size = 0;

    std::ifstream file(argv[1], std::ifstream::in);
    std::string str;

    std::getline(file, str);
    assert(str == "H:");

    std::getline(file, str);
    while (str != "O:") {
        H.emplace_back(str);
        std::getline(file, str);
    }

    assert(str == "O:");

    std::getline(file, str);
    while (str != "n:") {
        O_start.emplace_back(str);
        O_size++;
        std::getline(file, str);
    }

    assert(str == "n:");
    std::getline(file, str);
    int n = std::stoi(str);

    file.close();
    std::cerr << "File parsed successfully" << std::endl;

    size_t prevTableSize = O_start.size();
    size_t prevWrittenDiskSize = 0;
    size_t prevRealDiskSize = 0;
    double prevExpansionRatio = H.size();
    double averageBytesPerHash = sizeof(Node) + sizeof(Node*);

    if (config.startFromLayer == 0) {
        if (!std::filesystem::exists(config.directoryPath))
            std::filesystem::create_directory(config.directoryPath);
        if (!std::filesystem::is_empty(config.directoryPath)) {
            if (!config.overwriteMainDirectory) {
                std::cerr << "Error: directory " + config.directoryPath + " is not empty" << std::endl;
                std::cerr << "Add the --force option to overwrite its contents, or specify an empty directory" << std::endl;
                return 1;
            } else {
                std::cerr << "--force specified, clearing non-empty directory " + config.directoryPath + "..." << std::endl;
                std::vector<std::filesystem::path> toDelete;
                for (auto it = std::filesystem::directory_iterator(config.directoryPath);
                     it != std::filesystem::directory_iterator{}; it++)
                    toDelete.push_back(*it);
                for (const std::filesystem::path& path : toDelete)
                    std::filesystem::remove_all(path);
            }
        }
        std::cerr << std::endl;

        if (!std::filesystem::exists(config.directoryPath + "/0"))
            std::filesystem::create_directory(config.directoryPath + "/0");
        for (size_t i = 0; i < O_start.size(); i += BLOCK_SIZE) {
            std::vector<Expression> cur(O_start.begin() + i, O_start.begin() + std::min(O_start.size(), i + BLOCK_SIZE));
            std::sort(cur.begin(), cur.end());
            auto [writtenDiskSize, realDiskSize] = storeBlock(getFlatPathToHashTableFile(0, i), cur);
            prevWrittenDiskSize += writtenDiskSize;
            prevRealDiskSize += realDiskSize;
        }
    } else {
        std::cerr << "Resuming computation from existing layer " << config.startFromLayer << std::endl;
        LayerMeta meta;
        if (config.hasExplicitMeta) {
            meta.O_size = config.explicitOSize;
            meta.tableSize = config.explicitTableSize;
            meta.expansionRatio = config.explicitExpRatio;
            meta.avgBytesPerHash = config.explicitAvgBytesPerHash;
            meta.writtenDiskSize = 1;
            meta.realDiskSize = 1;
        } else if (!loadLayerMeta(config.startFromLayer, meta)) {
            meta = scanLayerDirectory(config.startFromLayer, 370.30);
        }
        O_size = meta.O_size;
        prevTableSize = meta.tableSize;
        prevExpansionRatio = meta.expansionRatio;
        averageBytesPerHash = meta.avgBytesPerHash;
        prevWrittenDiskSize = std::max<size_t>(1, meta.writtenDiskSize);
        prevRealDiskSize = std::max<size_t>(1, meta.realDiskSize);
        std::cerr << "Loaded state for layer " << config.startFromLayer
                  << ": O_size=" << O_size
                  << " prevTableSize=" << prevTableSize
                  << " prevExpRatio=" << prevExpansionRatio
                  << " avgBytesPerHash=" << averageBytesPerHash << std::endl << std::endl;
    }

    auto timeStart = Clock::now();
    double timeOnPrevLayer = 0;
    for (int32_t t = config.startFromLayer; t < n; t++) {
        std::cerr << "\e[32m" << "Step " << t + 1 << "\e[0m" << std::endl;
        std::string curLayerDir = config.directoryPath + "/" + std::to_string(t + 1);
        if (std::filesystem::exists(curLayerDir) && config.startFromLayer > 0 && t == config.startFromLayer && config.overwriteMainDirectory) {
            std::cerr << "Removing incomplete target layer directory: " << curLayerDir << std::endl;
            std::filesystem::remove_all(curLayerDir);
        }
        if (!std::filesystem::exists(curLayerDir)) {
            std::filesystem::create_directory(curLayerDir);
            std::cerr << "Created directory: " << t + 1 << std::endl;
        }

        auto timeLayerStart = Clock::now();
        size_t tableSize = std::max((size_t)1, (size_t)std::ceil(prevExpansionRatio * O_size));
#ifdef DEBUG
        std::cerr << "[DIAG] layer=" << t + 1
                  << " O_size=" << O_size
                  << " prevExpRatio=" << std::fixed << std::setprecision(6) << prevExpansionRatio
                  << " tableSize=" << tableSize
                  << " avgBytesPerHash=" << std::setprecision(2) << averageBytesPerHash
                  << std::endl;
#endif

        auto safeSubtract = [](size_t x, size_t y) {
            return (x >= y) ? (x - y) : 0;
        };
        static_assert(sizeof(Node) >= sizeof(Expression));
        size_t memoryUsagePerOldExpression = safeSubtract(averageBytesPerHash, (sizeof(Node) - sizeof(Expression)) + sizeof(std::mutex) / 16);
        size_t fixedMemoryUsage = std::thread::hardware_concurrency() * (BLOCK_SIZE * memoryUsagePerOldExpression + 131'072);

        size_t cur_O_size = 0;
        size_t curWrittenDiskSize = 0;
        size_t curRealDiskSize = 0;
        size_t sumOfMemoryUsages = 0;
        size_t windowIndex = 0;
        for (size_t i = 0; i < tableSize;) {
            size_t maxNumHashes = safeSubtract(config.memoryLimitInBytes, fixedMemoryUsage) / averageBytesPerHash;
            size_t numHashes = std::min(tableSize - i, std::max((size_t)BLOCK_SIZE, maxNumHashes - maxNumHashes % BLOCK_SIZE));

#ifdef DEBUG
            std::cerr << "[DIAG]   window=" << windowIndex
                      << " slots=[" << i << ", " << i + numHashes << ")"
                      << " maxNumHashes=" << maxNumHashes
                      << std::endl;
#endif

            ComputationResult res = computePartOfNextLayer(H, prevTableSize, t + 1, tableSize, i, i + numHashes);

            cur_O_size += res.O_size;
            curWrittenDiskSize += res.writtenDiskSize;
            curRealDiskSize += res.realDiskSize;
            sumOfMemoryUsages += res.maxMemoryUsed;
            averageBytesPerHash = std::max((double)sumOfMemoryUsages / (i + numHashes), (double)(sizeof(Node) + sizeof(Node*)));

#ifdef DEBUG
            std::cerr << "[DIAG]   window=" << windowIndex
                      << " O_size_so_far=" << cur_O_size
                      << " rawMemUsed=" << res.maxMemoryUsed / 1'000'000 << " MB"
                      << " avgBytesPerHash=" << std::setprecision(2) << averageBytesPerHash
                      << std::endl;
#endif

            i += numHashes;
            windowIndex++;
        }
        curRealDiskSize += getLogicalFileSize(curLayerDir);

        prevExpansionRatio = (double)cur_O_size / O_size;
        O_size = cur_O_size;
        prevTableSize = tableSize;

        saveLayerMeta(t + 1, {O_size, prevTableSize, prevExpansionRatio, averageBytesPerHash, curWrittenDiskSize, curRealDiskSize});

        auto timeLayerFinish = Clock::now();
        std::cerr << "Layer " << t + 1 << " computed; total terms: " << O_size << " (x" << prevExpansionRatio << " from previous layer)" << std::endl;
        std::cerr << "Written to disk: ~" << curWrittenDiskSize / 1'000'000 << " MB (x" << (double)curWrittenDiskSize / prevWrittenDiskSize << " from previous layer)" << std::endl;
        std::cerr << "Actual disk usage: ~" << curRealDiskSize / 1'000'000 << " MB (x" << (double)curRealDiskSize / prevRealDiskSize << " from previous layer)" << std::endl;
        double timeOnCurLayer = std::chrono::duration_cast<std::chrono::microseconds>(timeLayerFinish - timeLayerStart).count() / 1000000.0;
        std::cerr << "Total time on layer: " << timeOnCurLayer << " sec";
        if (t > config.startFromLayer)
            std::cerr << " (x" << timeOnCurLayer / timeOnPrevLayer << " from previous layer)" << std::endl;
        else
            std::cerr << std::endl;
        timeOnPrevLayer = timeOnCurLayer;
        prevWrittenDiskSize = curWrittenDiskSize;
        prevRealDiskSize = curRealDiskSize;
        std::cerr << std::endl;
    }

    auto timeEnd = Clock::now();
    std::cerr << "Computation took " << std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - timeStart).count() / 1000000.0 << " sec" << std::endl;

    if (config.textOutput) {
        for (size_t i = 0; i < prevTableSize; i += BLOCK_SIZE)
            for (const auto& expr : loadAndDeleteBlock(resolvePathToHashTableFileForRead(n, i)))
                expr.print();

        auto timeFinish = Clock::now();
        std::cerr << "Text output took " << std::chrono::duration_cast<std::chrono::microseconds>(timeFinish - timeEnd).count() / 1000000.0 << " sec" << std::endl;
    }

    return 0;
}
