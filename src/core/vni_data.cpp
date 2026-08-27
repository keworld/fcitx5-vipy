#include "vicplex/vni_data.hpp"

namespace vicplex {

const VniData::ShapeRule VniData::kShapeRules[7] = {
    {'6', 'a', 0x00E2},
    {'8', 'a', 0x0103},
    {'6', 'e', 0x00EA},
    {'6', 'o', 0x00F4},
    {'7', 'o', 0x01A1},
    {'7', 'u', 0x01B0},
    {'9', 'd', 0x0111},
};

const VniData::ShapeRule *VniData::findShape(char key) {
    for (const auto &rule : kShapeRules) {
        if (rule.key == key) return &rule;
    }
    return nullptr;
}

int VniData::toneFromKey(char key) {
    switch (key) {
        case '1': return 2; // sắc
        case '2': return 1; // huyền
        case '3': return 3; // hỏi
        case '4': return 4; // ngã
        case '5': return 5; // nặng
        default: return 0;
    }
}

} // namespace vicplex
