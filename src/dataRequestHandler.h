#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "../vendor/azbacktest.h"
#include "tooling/dollarDeltaToPoints.h"
#include "mlConfig.h"

struct Request {
   unsigned int idx = 0;
   unsigned int horizon = 0;
};

struct Response {
   std::vector<double> returnPath;
};

struct Engine {
    std::vector<double> prices;
    MarketData md;
    Handling handler;
    DataWindow window;

    // contract the trade opened on
    std::string trackedContract;

    Engine(const std::string& path, double tickSize = 0.25, double tickValue = 0.50)
       : prices{},
         md(path),
         handler(prices, tickSize, tickValue),
         window{}

   {}
};

double requestIdx(Engine& engine, unsigned int idx) {
   engine.md.setCursor(idx);
   DataWindow window = engine.handler.requestDataWindow(engine.md, 1);
   return window.prices[0];
}

std::string_view activeContract(Engine& engine) {
    if (engine.trackedContract.empty()) {
        engine.trackedContract = std::string(engine.md.contractAt(0));
    }
    return engine.trackedContract;
}

std::optional<unsigned int> nextMatchingIdx(Engine& engine, unsigned int from, std::string_view contract) {
    unsigned int idx = from;
    std::string_view sym = engine.md.contractAt(static_cast<int>(idx));
    while (!sym.empty() && sym != contract) {
        ++idx;
        sym = engine.md.contractAt(static_cast<int>(idx));
    }
    if (sym.empty()) return std::nullopt;
    return idx;
}

std::optional<unsigned int> prevMatchingIdx(Engine& engine, unsigned int from, std::string_view contract) {
    unsigned int idx = from;
    while (idx > 0) {
        --idx;
        if (engine.md.contractAt(static_cast<int>(idx)) == contract) return idx;
    }
    return std::nullopt;
}

Response sendRequest(Engine& engine, const Request& request) {
    Response response;

    engine.md.setCursor(request.idx);
    DataWindow window = engine.handler.requestDataWindow(engine.md, request.horizon + 1);

    if (!window.prices.empty()) {
        const double requestPrice = window.prices[0];
        for (double price : window.prices) { // push back price data from t to t+h
            response.returnPath.push_back(
                toPoints(kMLConfig.tickSize, kMLConfig.tickValue, price - requestPrice)
            );
        }
    }

    return response;
}
