#pragma once

#include <vector>
#include <string>
#include <set>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    std::string tmp;
    for (const auto& str : strings) {
        tmp = str;
        if (!tmp.empty()) {
            non_empty_strings.insert(tmp);
        }
    }
    return non_empty_strings;
}