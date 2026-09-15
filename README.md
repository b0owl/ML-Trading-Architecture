Machine learning pipeline on top of AZBacktest (https://github.com/b0owl/AZBacktest/).  
Includes a built in QModel  

Heavily in beta, designed around the QModel implementation. A generic approach is planned.  

Architecture includes a request/response system inside of `dataRequestHandler.h` and a cost factorization system to train the models. Custom close conditions (i.e a stop loss) can be added through the CloseConditionPiepline struct, holds a vector of functions that return true/false values.  

To use this project, you'll need to generate the vendor folder:  
`./build.sh` to build mlTradingArchitectureCore.h and mlTradingArchitecture.h  
the core version excludes QModel and RiskCostModel, the vanilla version does not.  
This command will also create mlTradingArchitecture/ (where both headers live, alongside GLFW headers)

As for the actual models, you start by defining some feature and returning its output as a double.  
An example of a feature is the change in delta over two arbitrary points.  

You then declare the following:  
```cpp
loadConfig(); // load configs
loadMLConfig(); 
const float kBucketWidth = f; // how big should the buckets be for the Q table
QModel model(kBucketWidth); // see above
AgentParameter someAgentControlledInformation(min, default, average);
RiskCostModel riskCostModel; // see comment below
CostFn costFn = std::ref(riskCostModel); // riskCostModel, defined in costFactorization.h is the built in cost model
CloseConditionPipeline = ;
const unsigned int kTrainingSteps = ;
```

Implement the actual loop and trading logic...  
```cpp
TradeState state;
unsigned int stepsInPosition = 0;

// reward earned since the last time the model retuned its parameters
double rewardSinceAdjust = 0.0;
double rawReturnSinceAdjust = 0.0; // pnl

for (unsigned int step = 0; step < kTrainingSteps; ++step) {
    std::vector<double> features = { yourFeature(state.idx, someAgentControlledInformation) };

    TradeAction action = model.selectAction(features, state);

    if (state.isLong || state.isShort) {
        CloseRequest closeRequest{state, features, stepsInPosition};
        CloseResponse closeResponse = closeConditions.evaluate(closeRequest);
        if (closeResponse.forceClose) action = TradeAction::Close;
    }

    // periodically retune reward
    if (step % kAdjustInterval == 0) {
        model.adjustParameters(features, rewardSinceAdjust, {&someAgentControlledInformation});
        model.adjustParameters(features, rawReturnSinceAdjust, riskCostModel.weights.asList());
        rewardSinceAdjust = 0.0;
        rawReturnSinceAdjust = 0.0;
    }

    TradeStep result = simulateTradeStep(state, action, costFn);

    const bool outOfData = result.done && result.state.idx == state.idx;
    if (outOfData) break;

    std::vector<double> nextFeatures = { yourFeature(result.state.idx, someAgentControlledInformation) };
    model.update(features, action, result.reward, nextFeatures, result.done);

    rewardSinceAdjust += result.reward;
    rawReturnSinceAdjust += result.rawReturn;

    stepsInPosition = (result.state.isLong || result.state.isShort) ? stepsInPosition + 1 : 0;
    state = result.state;
}
```