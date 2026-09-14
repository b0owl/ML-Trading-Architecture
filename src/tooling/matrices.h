#pragma once

#include <functional>
#include <vector>

struct SensitivityGrid {
    std::vector<int> shape;
    std::vector<double> values;

    SensitivityGrid() = default;
    explicit SensitivityGrid(std::vector<int> dimShape) : shape(std::move(dimShape)) {
        int total = 1;
        for (int s : shape) total *= s;
        values.assign(total, 0.0);
    }

    bool empty() const { return values.empty(); }

    int flatIndex(const std::vector<int>& idx) const {
        int flat = 0;
        for (size_t d = 0; d < shape.size(); ++d) {
            flat = flat * shape[d] + idx[d];
        }
        return flat;
    }

    std::vector<int> multiIndex(int flat) const {
        std::vector<int> idx(shape.size());
        for (size_t d = shape.size(); d-- > 0;) {
            idx[d] = flat % shape[d];
            flat /= shape[d];
        }
        return idx;
    }
};

// One swept parameter: `steps` evenly spaced values across [min, max].
struct SweepParameter {
    double min;
    double max;
    unsigned int steps;

    double valueAt(unsigned int step) const {
        if (steps <= 1) return min;
        return min + (static_cast<double>(step) / (steps - 1)) * (max - min);
    }
};

inline SensitivityGrid populateSensitivityMatrix(
    const std::vector<SweepParameter>& parameters,
    const std::function<double(const std::vector<double>&)>& evaluator
) {
    std::vector<int> shape;
    shape.reserve(parameters.size());
    for (const auto& p : parameters) shape.push_back(static_cast<int>(p.steps));

    SensitivityGrid grid(shape);

    for (int flat = 0; flat < static_cast<int>(grid.values.size()); ++flat) {
        const std::vector<int> idx = grid.multiIndex(flat);

        std::vector<double> paramValues(parameters.size());
        for (size_t d = 0; d < parameters.size(); ++d) {
            paramValues[d] = parameters[d].valueAt(static_cast<unsigned int>(idx[d]));
        }

        grid.values[flat] = evaluator(paramValues);
    }

    return grid;
}
