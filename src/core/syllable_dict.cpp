//
// Created by keworld on 8/26/26.
//
#include "vipy/syllable_dict.hpp"
#include "syllable_hash.hpp"

namespace vipy {

    bool SyllableDict::contains(const std::string &low) {
        if (low.empty()) {
            return false;
        }
        return Perfect_Hash::in_word_set(low.c_str(), static_cast<unsigned int>(low.length())) != nullptr;
    }

} // namespace vipy