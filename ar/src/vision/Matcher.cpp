#include "vision/Matcher.h"

#include <format>
#include <string>

namespace ar::vision {

std::unique_ptr<IImageMatcher> create_matcher(std::string_view backend, const MatcherConfig &config) {
    auto matcher = MatcherRegistry::instance().create(backend, config);
    if (!matcher && config.logger) {
        std::string available;
        MatcherRegistry::instance().for_each_name([&available](std::string_view name) {
            available += (available.empty() ? "" : ", ");
            available += name;
        });
        if (MatcherRegistry::instance().contains(backend)) {
            config.logger->log(ar::log::Level::error,
                               "Matcher initialization failed: " + std::string(backend));
            return nullptr;
        }
        config.logger->log(ar::log::Level::error,
                           std::format("Unknown matcher backend '{}' (available: {})", backend, available));
    }
    return matcher;
}

std::vector<std::string_view> matcher_backends() {
    std::vector<std::string_view> names;
    MatcherRegistry::instance().for_each_name([&names](std::string_view name) { names.push_back(name); });
    return names;
}

} // namespace ar::vision
