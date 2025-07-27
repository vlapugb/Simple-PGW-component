#include "BlackListStorer.h"

#include <functional>

BlackListStorer::BlackListStorer (const std::size_t bloom_size, const std::shared_ptr<spdlog::logger>& logger)
: _bloom (bloom_size, false), _logger (logger) {
}

void BlackListStorer::store (const std::unordered_set<std::string>& black_list) {
    _true_set = black_list;
    for (const auto& s : black_list)
        bloom_add (s);
    _logger->info ("Blacklist loaded ({} entries)", black_list.size ());
}

bool BlackListStorer::is_in_blacklist (const std::string& imsi) const {
    if (!bloom_test (imsi)) {
        return false;
    }
    const bool hit = _true_set.contains (imsi);
    _logger->debug ("IMSI {} {}", imsi, hit ? "in blacklist" : "allowed");
    return hit;
}


void BlackListStorer::bloom_add (const std::string& s) {
    const std::size_t h1 = hash1 (s);
    const std::size_t h2 = hash2 (s) | 1;
    for (std::size_t i = 0; i < K; ++i)
        _bloom[(h1 + i * h2) % _bloom.size ()] = true;
}

bool BlackListStorer::bloom_test (const std::string& s) const {
    const std::size_t h1 = hash1 (s);
    const std::size_t h2 = hash2 (s) | 1;
    for (std::size_t i = 0; i < K; ++i)
        if (!_bloom[(h1 + i * h2) % _bloom.size ()])
            return false;
    return true;
}

std::size_t BlackListStorer::hash1 (const std::string& s) {
    return std::hash<std::string>{}(s);
}

std::size_t BlackListStorer::hash2 (const std::string& s) {
    constexpr std::size_t mod = 7ULL * 10000000000ULL;
    std::size_t hash          = 0;
    std::size_t pow           = 1;

    for (const char ch : s) {
        constexpr std::size_t primary = 43;
        hash                          = (hash + (ch - '0') * pow) % mod;
        pow                           = (pow * primary) % mod;
    }
    return hash;
}
