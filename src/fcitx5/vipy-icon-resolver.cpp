#include "vipy-icon-resolver.hpp"

#include <unistd.h>

namespace vipy::fcitx_wrapper {

namespace {
bool isReadable(const std::string &path) {
    return !path.empty() && access(path.c_str(), R_OK) == 0;
}

std::string joinPath(const std::string &directory, const std::string &name) {
    if (directory.empty()) {
        return name;
    }
    return directory.back() == '/' ? directory + name
                                  : directory + "/" + name;
}
} // namespace

std::string resolveIconPath(const std::vector<std::string> &names,
                            const IconSearchPaths &paths) {
    if (names.empty()) {
        return {};
    }

    for (const auto &name : names) {
        for (const auto &directory : paths.systemDirectories) {
            for (const char *extension : {".svg", ".png"}) {
                const auto path = joinPath(directory, name + extension);
                if (isReadable(path)) {
                    return path;
                }
            }
        }
    }

    return joinPath(paths.fallbackDirectory, names.front() + ".svg");
}

} // namespace vipy::fcitx_wrapper
