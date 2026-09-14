#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "../vendor/azbacktest.h"

struct MLConfig {
    float tickSize;
    float tickValue;
};

inline MLConfig kMLConfig{0.25f, 0.50f};

struct WeightRange {
    float min;
    float max;
};

// Weight ranges for costFactorization.h's optional RiskCostModel plug-in, dead code if you dont use it
struct RiskCostWeightRanges {
    WeightRange adverseExcursion{0.30f, 0.70f};
    WeightRange drawdown{0.10f, 0.40f};
    WeightRange volatility{0.05f, 0.20f};
    WeightRange giveback{0.10f, 0.30f};
    WeightRange underwater{0.05f, 0.20f};
    WeightRange timeToProfit{0.02f, 0.10f};
};

inline RiskCostWeightRanges kRiskCostWeightRanges{};


struct CostConfig {
    double min;
    double max;
};

inline CostConfig kCostConfig{1.25, 3.25};

namespace mlCfgDetail { inline bool loaded = false; }

inline void generateDefaultMLConfig(const char* tomlPath) {
    std::ofstream out(tomlPath);
    out <<
R"(# ML Trading Architecture configuration

# instrument tick size/value
tickSize  = 0.25
tickValue = 0.50

# A random value between these two numbers is sampled per trade for the
# round-trip transaction cost (core - used regardless of which cost model, if
# any, you plug in). If you dont want the randomization, just make them equal.
[transactionCost]
min = 1.25
max = 3.25

# Weight ranges for costFactorization.h's optional RiskCostModel plug-in -
# only relevant if you use it. Editing any of these is optional - only what
# you change overrides the built-in defaults.
[riskCost.adverseExcursion]
min = 0.30
max = 0.70

[riskCost.drawdown]
min = 0.10
max = 0.40

[riskCost.volatility]
min = 0.05
max = 0.20

[riskCost.giveback]
min = 0.10
max = 0.30

[riskCost.underwater]
min = 0.05
max = 0.20

[riskCost.timeToProfit]
min = 0.02
max = 0.10
)";
}

inline WeightRange loadWeightRange(const toml::Table& cfg, const std::string& section, WeightRange def) {
    WeightRange r;
    r.min = static_cast<float>(toml::getFloat(cfg, section, "min", def.min));
    r.max = static_cast<float>(toml::getFloat(cfg, section, "max", def.max));
    return r;
}

inline void loadMLConfig(const char* tomlPath = "MLConfig.toml") {
    if (mlCfgDetail::loaded) return;
    mlCfgDetail::loaded = true;

    {
        std::ifstream check(tomlPath);
        if (!check.is_open()) {
            generateDefaultMLConfig(tomlPath);
            std::cout << "generated " << tomlPath << " with default values" << std::endl;
        }
    }

    auto cfg = toml::parse(tomlPath);

    kMLConfig.tickSize  = toml::getFloat(cfg, "", "tickSize", kMLConfig.tickSize);
    kMLConfig.tickValue = toml::getFloat(cfg, "", "tickValue", kMLConfig.tickValue);

    kCostConfig.min = toml::getFloat(cfg, "transactionCost", "min", static_cast<float>(kCostConfig.min));
    kCostConfig.max = toml::getFloat(cfg, "transactionCost", "max", static_cast<float>(kCostConfig.max));

    kRiskCostWeightRanges.adverseExcursion = loadWeightRange(cfg, "riskCost.adverseExcursion", kRiskCostWeightRanges.adverseExcursion);
    kRiskCostWeightRanges.drawdown = loadWeightRange(cfg, "riskCost.drawdown", kRiskCostWeightRanges.drawdown);
    kRiskCostWeightRanges.volatility = loadWeightRange(cfg, "riskCost.volatility", kRiskCostWeightRanges.volatility);
    kRiskCostWeightRanges.giveback = loadWeightRange(cfg, "riskCost.giveback", kRiskCostWeightRanges.giveback);
    kRiskCostWeightRanges.underwater = loadWeightRange(cfg, "riskCost.underwater", kRiskCostWeightRanges.underwater);
    kRiskCostWeightRanges.timeToProfit = loadWeightRange(cfg, "riskCost.timeToProfit", kRiskCostWeightRanges.timeToProfit);
}
