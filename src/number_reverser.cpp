#include "number_reverser.h"

std::vector<int> NumberReverser::reverse(const std::vector<int>& nums) {
    std::vector<int> out;
    out.reserve(nums.size());

    for (int i = (int)nums.size() - 1; i >= 0; i--) {
        out.push_back(nums[i]);
    }

    return out;
}
