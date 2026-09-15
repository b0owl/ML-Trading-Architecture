#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "matrices.h"
#include "agentParameter.h"
#include "costFn.h"
#include "../mlConfig.h"

// costFn plug in. Dev-chosen penalty/reward system, you can design your own should you prefer

double parameterSensitivityPenalty(const SensitivityGrid& grid) {
    if (grid.empty()) return 0.0;

    int peakFlat = 0;
    for (size_t i = 1; i < grid.values.size(); ++i) {
        if (grid.values[i] > grid.values[peakFlat]) peakFlat = static_cast<int>(i);
    }

    const double peak = grid.values[peakFlat];
    if (peak <= 0.0) return 0.0;

    const std::vector<int> peakIdx = grid.multiIndex(peakFlat);

    double weightedPenalty = 0.0;
    double totalWeight = 0.0;

    for (size_t i = 0; i < grid.values.size(); ++i) {
        const std::vector<int> idx = grid.multiIndex(static_cast<int>(i));

        int distance = 0;
        for (size_t d = 0; d < idx.size(); ++d) {
            distance += std::abs(idx[d] - peakIdx[d]);
        }

        const double weight = 1.0 / (1.0 + distance);
        const double degradation = 1.0 - (grid.values[i] / peak);

        weightedPenalty += degradation * weight;
        totalWeight += weight;
    }

    return totalWeight > 0.0 ? (weightedPenalty / totalWeight) : 0.0;
}

SensitivityGrid sensitivityMatrix;

struct RiskCostWeights {
    AgentParameter adverseExcursion;
    AgentParameter drawdown;
    AgentParameter volatility;
    AgentParameter giveback;
    AgentParameter underwater;
    AgentParameter timeToProfit;

    RiskCostWeights()
        : adverseExcursion(midpoint(kRiskCostWeightRanges.adverseExcursion), kRiskCostWeightRanges.adverseExcursion.min, kRiskCostWeightRanges.adverseExcursion.max),
          drawdown(midpoint(kRiskCostWeightRanges.drawdown), kRiskCostWeightRanges.drawdown.min, kRiskCostWeightRanges.drawdown.max),
          volatility(midpoint(kRiskCostWeightRanges.volatility), kRiskCostWeightRanges.volatility.min, kRiskCostWeightRanges.volatility.max),
          giveback(midpoint(kRiskCostWeightRanges.giveback), kRiskCostWeightRanges.giveback.min, kRiskCostWeightRanges.giveback.max),
          underwater(midpoint(kRiskCostWeightRanges.underwater), kRiskCostWeightRanges.underwater.min, kRiskCostWeightRanges.underwater.max),
          timeToProfit(midpoint(kRiskCostWeightRanges.timeToProfit), kRiskCostWeightRanges.timeToProfit.min, kRiskCostWeightRanges.timeToProfit.max) {}

    static double midpoint(WeightRange range) { return (range.min + range.max) / 2.0; }

    std::vector<AgentParameter*> asList() {
        return {&adverseExcursion, &drawdown, &volatility, &giveback, &underwater, &timeToProfit};
    }
};

struct RiskCostModel {
    RiskCostWeights weights;

    double normalizationScale = 1.0;
    double normalizationRate = 0.05;

    // keep simulate but freeze learning
    bool frozen = false;

    double operator()(const std::vector<double>& returnPath, double transactionCost) {
        if (returnPath.size() <= 1) return transactionCost;

        const double scale = std::max(normalizationScale, 0.5);

        double peak = returnPath[0];
        double last = returnPath[0];
        double peakAbs = std::abs(returnPath[0]);
        double maxDrawdown = 0.0;
        double maxAdverseExcursion = 0.0;
        size_t underwaterCount = 0;
        int firstProfitStep = -1;
        double sumChange = 0.0;
        double sumSquaredChange = 0.0;

        for (size_t i = 1; i < returnPath.size(); ++i) {
            const double value = returnPath[i];
            const double change = value - last;
            sumChange += change;
            sumSquaredChange += change * change;

            peak = std::max(peak, value);
            peakAbs = std::max(peakAbs, std::abs(value));
            maxDrawdown = std::max(maxDrawdown, peak - value);

            if (value < 0.0) {
                maxAdverseExcursion = std::max(maxAdverseExcursion, -value);
                ++underwaterCount;
            }

            if (firstProfitStep < 0 && value > 0.0) {
                firstProfitStep = static_cast<int>(i);
            }

            last = value;
        }

        const size_t steps = returnPath.size() - 1;

        double volatility = 0.0;
        if (steps > 1) {
            const double mean = sumChange / static_cast<double>(steps);
            const double variance = (sumSquaredChange / static_cast<double>(steps)) - (mean * mean);
            volatility = std::sqrt(std::max(0.0, variance));
        }

        const double giveback = std::max(0.0, peak - last);
        const double underwaterFraction = static_cast<double>(underwaterCount) / static_cast<double>(steps);
        const double timeToProfitFraction = firstProfitStep >= 0
            ? static_cast<double>(firstProfitStep) / static_cast<double>(steps)
            : 1.0;

        double penalty = 0.0;
        penalty += (maxAdverseExcursion / scale) * weights.adverseExcursion.value;
        penalty += (maxDrawdown / scale) * weights.drawdown.value;
        penalty += (volatility / scale) * weights.volatility.value;
        penalty += (giveback / scale) * weights.giveback.value;
        penalty += underwaterFraction * weights.underwater.value;
        penalty += timeToProfitFraction * weights.timeToProfit.value;

        if (!frozen) {
            normalizationScale += normalizationRate * (peakAbs - normalizationScale);
        }

        return transactionCost + penalty;
    }
};
