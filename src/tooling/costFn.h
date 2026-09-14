#pragma once

#include <functional>
#include <random>
#include <vector>

#include "../mlConfig.h"

using CostFn = std::function<double(const std::vector<double>& returnPath, double transactionCost)>;

inline double defaultCostFn(const std::vector<double>& /*returnPath*/, double transactionCost) {
    return transactionCost;
}

// Round-trip transaction cost, sampled once per trade at entry from
// kCostConfig's configured range
inline double generateTransactionCost() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(kCostConfig.min, kCostConfig.max);
    return dist(gen);
}
