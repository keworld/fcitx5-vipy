#ifndef FCITX5_VIPY_ICON_RESOLVER_HPP
#define FCITX5_VIPY_ICON_RESOLVER_HPP

#include <string>
#include <vector>

namespace vipy::fcitx_wrapper {

struct IconSearchPaths {
    std::vector<std::string> systemDirectories;
    std::string fallbackDirectory;
};

std::string resolveIconPath(const std::vector<std::string> &names,
                            const IconSearchPaths &paths);

} // namespace vipy::fcitx_wrapper

#endif
