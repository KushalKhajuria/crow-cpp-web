#include "number_reverser.h"

using namespace std;

vector<int> NumberReverser::reverse(const vector<int>& nums) {
    vector<int> out;
    out.reserve(nums.size());

    for (int i = (int)nums.size() - 1; i >= 0; i--) {
        out.push_back(nums[i]);
    }

    return out;
}
