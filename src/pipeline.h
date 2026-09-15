#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "../vendor/azbacktest.h"

#include "tooling/costFn.h"
#include "dataRequestHandler.h"

namespace pipelineDetail { inline bool configLoaded = (loadConfig(), true); }

Engine engine(kCSVMapping.path);

Response sendRequest(Request request) {
    return sendRequest(engine, request);
}

enum class TradeAction {
    Hold,
    Long,
    Short,
    Close
};

void simulateAction(
    Engine& engine,
    unsigned int idx,
    TradeAction action,
    bool& opened,
    bool& closed
) {
    opened = false;
    closed = false;

    const bool inLong = engine.handler.inLong;
    const bool inShort = engine.handler.inShort;

    if (action == TradeAction::Long && !inLong && !inShort) {
        engine.handler.openLong(idx);
        opened = true;
        return;
    }

    if (action == TradeAction::Short && !inLong && !inShort) {
        engine.handler.openShort(idx);
        opened = true;
        return;
    }

    if (action == TradeAction::Close && (inLong || inShort)) {
        engine.handler.closeTrade();
        closed = true;
        return;
    }
}

struct TradeState {
    unsigned int idx = 0;

    bool isLong = false;
    bool isShort = false;

    double entryPrice = 0.0;

    std::vector<double> returnPath;

    double transactionCost = 0.0;
};

struct TradeStep {
    TradeState state;

    TradeAction action = TradeAction::Hold;

    double incrementalReturn = 0.0;

    double reward = 0.0;

    double rawReturn = 0.0;

    bool done = false;
};

TradeStep simulateTradeStep(
    const TradeState& current,
    TradeAction action,
    const CostFn& costFn = defaultCostFn
) {
    TradeStep result;

    result.state = current;
    result.action = action;

    const std::string_view contract = activeContract(engine);
    const std::optional<unsigned int> nextIdxOpt = nextMatchingIdx(engine, current.idx + 1, contract);
    if (!nextIdxOpt) {
        result.done = true;
        return result;
    }
    const unsigned int nextIdx = *nextIdxOpt;

    if (action == TradeAction::Hold && !current.isLong && !current.isShort) {
        result.state.idx = nextIdx;
        return result;
    }

    if (action == TradeAction::Long && !current.isLong && !current.isShort) {
        Request request;

        request.idx = current.idx;
        request.horizon = 1;

        Response response = sendRequest(request);

        if (response.returnPath.size() < 2) {
            result.done = true;
            return result;
        }

        result.state.idx = nextIdx;
        result.state.isLong = true;
        result.state.isShort = false;

        result.state.entryPrice = requestIdx(engine, current.idx);
        result.state.transactionCost = generateTransactionCost();

        result.state.returnPath.clear();
        result.state.returnPath.push_back(0.0);

        double stepReturn = response.returnPath.back();

        result.state.returnPath.push_back(stepReturn);

        result.incrementalReturn = stepReturn;

        result.reward = stepReturn;
        result.rawReturn = stepReturn;

        return result;
    }

    if (action == TradeAction::Short && !current.isLong && !current.isShort) {
        Request request;

        request.idx = current.idx;
        request.horizon = 1;

        Response response = sendRequest(request);

        if (response.returnPath.size() < 2) {
            result.done = true;
            return result;
        }

        result.state.idx = nextIdx;
        result.state.isLong = false;
        result.state.isShort = true;

        result.state.entryPrice = requestIdx(engine, current.idx);
        result.state.transactionCost = generateTransactionCost();

        result.state.returnPath.clear();
        result.state.returnPath.push_back(0.0);

        double stepReturn = -response.returnPath.back();

        result.state.returnPath.push_back(stepReturn);

        result.incrementalReturn = stepReturn;

        result.reward = stepReturn;
        result.rawReturn = stepReturn;

        return result;
    }

    if (current.isLong) {
        Request request;

        request.idx = current.idx;
        request.horizon = 1;

        Response response = sendRequest(request);

        if (response.returnPath.size() < 2) {
            result.done = true;
            return result;
        }

        double currentReturn = current.returnPath.empty() ? 0.0 : current.returnPath.back();

        double localReturn = response.returnPath.back();

        double nextReturn = currentReturn + localReturn;

        double stepReturn = localReturn;

        result.state.idx = nextIdx;
        result.state.returnPath.push_back(nextReturn);

        result.incrementalReturn = stepReturn;

        if (action == TradeAction::Close) {
            result.done = true;

            const double totalCost = costFn(result.state.returnPath, current.transactionCost);

            result.reward = stepReturn - totalCost;
            result.rawReturn = stepReturn - current.transactionCost;

            result.state.isLong = false;

            return result;
        }

        result.reward = stepReturn;
        result.rawReturn = stepReturn;

        return result;
    }

    if (current.isShort) {
        Request request;

        request.idx = current.idx;
        request.horizon = 1;

        Response response = sendRequest(request);

        if (response.returnPath.size() < 2) {
            result.done = true;
            return result;
        }

        double currentReturn = current.returnPath.empty() ? 0.0 : current.returnPath.back();

        double localReturn = -response.returnPath.back();

        double nextReturn = currentReturn + localReturn;

        double stepReturn = localReturn;

        result.state.idx = nextIdx;
        result.state.returnPath.push_back(nextReturn);

        result.incrementalReturn = stepReturn;

        if (action == TradeAction::Close) {
            result.done = true;

            const double totalCost = costFn(result.state.returnPath, current.transactionCost);

            result.reward = stepReturn - totalCost;
            result.rawReturn = stepReturn - current.transactionCost;

            result.state.isShort = false;

            return result;
        }

        result.reward = stepReturn;
        result.rawReturn = stepReturn;

        return result;
    }

    result.done = true;

    return result;
}
