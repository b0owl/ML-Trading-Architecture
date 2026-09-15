#pragma once

#include <any>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "../vendor/azbacktest.h"
#include "tooling/dollarDeltaToPoints.h"
#include "mlConfig.h"

struct Request {
   unsigned int idx = 0;
   unsigned int horizon = 0;
   std::function<std::any(int)> feature =
       [](int idx) -> std::any {
           return {};
       };

   bool isLong = false;
   bool isShort = false;
};

struct Response {
   Request request = {};
   unsigned int idx = request.idx + request.horizon;
   std::vector<double> returnPath;

   std::any featureOut;
   std::any feature() {
       return request.feature(this->idx);
   }

   bool isLong = false;
   bool isShort = false;
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

enum class RequestAction {
   RequestState
};

// The output of a feature from Response or Request is the output that feature generates
// at the respective index.

std::any RequestResponseIO(
   RequestAction action,
   Engine& engine,
   const Request& request
) {

    auto RequestState = [&engine, request]() -> Response {
        Response response;
        response.request = request;
        response.idx = request.idx + request.horizon;

        response.isLong = request.isLong;
        response.isShort = request.isShort;

        // run feature()
        std::any featureOut = response.feature();
        // set featureOut
        response.featureOut = featureOut;

        engine.md.setCursor(request.idx);
        DataWindow window = engine.handler.requestDataWindow(engine.md, request.horizon + 1);

        if (!window.prices.empty()) {
            const double requestPrice = window.prices[0];
            for (double price : window.prices) {
                response.returnPath.push_back(
                    toPoints(kMLConfig.tickSize, kMLConfig.tickValue, price - requestPrice)
                );
            }
        }

        return response;
    };

   switch (action) {
       case RequestAction::RequestState:
           return RequestState();
   }

   return {};
}

Response sendRequest(Engine& engine, const Request& request) {
    return std::any_cast<Response>(RequestResponseIO(RequestAction::RequestState, engine, request));
}
