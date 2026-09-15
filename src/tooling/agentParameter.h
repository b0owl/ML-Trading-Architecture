#pragma once

#include <algorithm>

// A tunable numeric value with bounds, usable for anything the model might want
// to steer at runtime (i.e sl/tp)
struct AgentParameter {
    double defaultValue;
    double min;
    double max;
    double value;
    bool enabled;

    AgentParameter(double defaultValue, double min, double max, bool enabled = true)
        : defaultValue(defaultValue), min(min), max(max), value(defaultValue), enabled(enabled) {}

    void setValue(double newValue) {
        value = std::clamp(newValue, min, max);
    }
};
