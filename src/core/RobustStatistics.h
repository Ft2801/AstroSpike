#ifndef ROBUSTSTATISTICS_H
#define ROBUSTSTATISTICS_H

#include <vector>
#include <cstddef>
#include <cstdint>

namespace RobustStatistics {

    // O(N) Robust Statistics Finder
    // Finds the value at minPrct and maxPrct (0.0 - 1.0)
    // Use minPrct=0.5 for robust Median.
    // Handles full dynamic range (including negatives).
    void findMinMaxPercentile(const float* data, size_t size, float minPrct, float* minOut, float maxPrct, float* maxOut, int threads);

    // Helper for Median
    float getMedian(const std::vector<float>& data);
    
    // Helper for MAD
    float getMAD(const std::vector<float>& data, float median);


}

#endif // ROBUSTSTATISTICS_H
