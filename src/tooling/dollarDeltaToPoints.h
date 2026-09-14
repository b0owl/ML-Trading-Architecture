#pragma once

double toPoints(float tickSize, float tickValue, double delta) { // where delta is the raw price difference
    return (delta / tickSize) * tickValue;
}