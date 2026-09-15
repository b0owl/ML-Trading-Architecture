#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../pipeline.h"
#include "../tooling/agentParameter.h"

struct CloseRequest {
    TradeState state;
    std::vector<double> features;
    unsigned int stepsInPosition = 0;
};

struct CloseResponse {
    bool forceClose = false;
    int triggeredIndex = -1; // which registered condition fired, -1 if none
};

using CloseCondition = std::function<bool(const CloseRequest&, const AgentParameter&)>;

struct CloseConditionEntry {
    std::string name;
    std::shared_ptr<AgentParameter> params;
    CloseCondition condition;
};

struct CloseConditionPipeline {
    std::vector<CloseConditionEntry> conditions;

    std::shared_ptr<AgentParameter> addCondition(std::string name, AgentParameter params, CloseCondition condition) {
        auto sharedParams = std::make_shared<AgentParameter>(params);
        conditions.push_back({std::move(name), sharedParams, std::move(condition)});
        return sharedParams;
    }

    AgentParameter* findParams(const std::string& name) {
        for (auto& entry : conditions) {
            if (entry.name == name) return entry.params.get();
        }
        return nullptr;
    }

    CloseResponse evaluate(const CloseRequest& request) const {
        CloseResponse response;

        for (size_t i = 0; i < conditions.size(); ++i) {
            const auto& entry = conditions[i];
            if (!entry.params->enabled) continue;
            if (entry.condition && entry.condition(request, *entry.params)) {
                response.forceClose = true;
                response.triggeredIndex = static_cast<int>(i);
                return response;
            }
        }

        return response;
    }
};
