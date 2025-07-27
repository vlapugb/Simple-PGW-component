#pragma once
#include "spdlog/logger.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class BlackListStorer {
    public:
    explicit BlackListStorer (std::size_t bloom_size, const std::shared_ptr<spdlog::logger>& logger);

    void store (const std::unordered_set<std::string>& black_list);

    bool is_in_blacklist (const std::string& imsi) const;

    private:
    void bloom_add (const std::string& s);

    bool bloom_test (const std::string& s) const;

    static std::size_t hash1 (const std::string& s);

    static std::size_t hash2 (const std::string& s);

    static constexpr std::size_t K = 7;

    std::vector<bool> _bloom;
    std::unordered_set<std::string> _true_set;
    std::shared_ptr<spdlog::logger> _logger;
};
