#pragma once

// QModel designed to take generic features and conditions

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <vector>

#include "../pipeline.h"
#include "../tooling/agentParameter.h"

constexpr int kActionCount = 4;
constexpr int kWeightLevels = 5;

inline int actionIndex(TradeAction action) { return static_cast<int>(action); }

struct WeightBandit {
    double rangeMin = 0.0;
    double rangeMax = 0.0;
    std::map<std::vector<int64_t>, std::array<double, kWeightLevels>> table;

    std::vector<int64_t> lastKey;
    int lastLevel = -1;
    bool hasPending = false;

    double levelValue(int level) const {
        return rangeMin + (rangeMax - rangeMin) * (static_cast<double>(level) / (kWeightLevels - 1));
    }
};

struct QModel {
    float learningRate = 0.1f;
    float discountFactor = 0.95f;
    float explorationRate = 0.1f;

    float weightLearningRate = 0.2f;
    float weightExplorationRate = 0.2f;

    float bucketWidth;

    explicit QModel(float bucketWidth = 1.0f) : bucketWidth(bucketWidth) {}

    std::mt19937 rng{std::random_device{}()};

    int selectBanditLevel(const std::array<double, kWeightLevels>& row) {
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        if (unit(rng) < weightExplorationRate) {
            std::uniform_int_distribution<int> pick(0, kWeightLevels - 1);
            return pick(rng);
        }
        int best = 0;
        for (int i = 1; i < kWeightLevels; ++i) {
            if (row[i] > row[best]) best = i;
        }
        return best;
    }

    double updateAndSelectWeight(WeightBandit& bandit, const std::vector<int64_t>& stateKey, double rewardSinceLastAdjust) {
        if (bandit.hasPending) {
            double& q = bandit.table[bandit.lastKey][bandit.lastLevel];
            q += weightLearningRate * (rewardSinceLastAdjust - q);
        }

        int level = selectBanditLevel(bandit.table[stateKey]);

        bandit.lastKey = stateKey;
        bandit.lastLevel = level;
        bandit.hasPending = true;

        return bandit.levelValue(level);
    }

    std::map<AgentParameter*, WeightBandit> parameterBandits;

    void adjustParameters(
        const std::vector<double>& features,
        double rewardSinceLastAdjust,
        const std::vector<AgentParameter*>& params
    ) {
        std::vector<int64_t> stateKey = discretize(features);

        for (AgentParameter* param : params) {
            auto it = parameterBandits.find(param);
            if (it == parameterBandits.end()) {
                WeightBandit bandit;
                bandit.rangeMin = param->min;
                bandit.rangeMax = param->max;
                it = parameterBandits.emplace(param, std::move(bandit)).first;
            }

            param->setValue(updateAndSelectWeight(it->second, stateKey, rewardSinceLastAdjust));
        }
    }

    std::map<std::vector<int64_t>, std::array<double, kActionCount>> table;

    std::vector<int64_t> discretize(const std::vector<double>& features) const {
        std::vector<int64_t> key;
        key.reserve(features.size());
        for (double f : features) {
            key.push_back(static_cast<int64_t>(std::floor(f / bucketWidth)));
        }
        return key;
    }

    std::array<double, kActionCount>& qRow(const std::vector<double>& features) {
        return table[discretize(features)];
    }

    std::vector<TradeAction> legalActions(const TradeState& state) const {
        if (state.isLong || state.isShort) return {TradeAction::Hold, TradeAction::Close};
        return {TradeAction::Hold, TradeAction::Long, TradeAction::Short};
    }

    TradeAction selectAction(const std::vector<double>& features, const TradeState& state) {
        std::vector<TradeAction> legal = legalActions(state);

        std::uniform_real_distribution<double> unit(0.0, 1.0);
        if (unit(rng) < explorationRate) {
            std::uniform_int_distribution<size_t> pick(0, legal.size() - 1);
            return legal[pick(rng)];
        }

        auto& row = qRow(features);
        TradeAction best = legal[0];
        double bestValue = row[actionIndex(best)];
        for (TradeAction a : legal) {
            if (row[actionIndex(a)] > bestValue) {
                bestValue = row[actionIndex(a)];
                best = a;
            }
        }
        return best;
    }

    void update(
        const std::vector<double>& features,
        TradeAction action,
        double reward,
        const std::vector<double>& nextFeatures,
        bool done
    ) {
        auto& row = qRow(features);

        double target = reward;
        if (!done) {
            auto& nextRow = qRow(nextFeatures);
            target += discountFactor * *std::max_element(nextRow.begin(), nextRow.end());
        }

        int a = actionIndex(action);
        row[a] += learningRate * (target - row[a]);
    }
};
