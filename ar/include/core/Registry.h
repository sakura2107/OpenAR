#pragma once
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

namespace ar::core {

// Name-keyed factory registry; one instance exists per interface family
// (MatcherRegistry, OcrRegistry, and later a detector registry for YOLO).
//
// Backends self-register from their own translation unit via the
// ARFW_REGISTER_BACKEND macro, so adding/removing a backend is a purely
// mechanical change to the project's source list: no framework code and no
// other backend ever references a backend by symbol. Dynamically loaded
// plugins (see ar/vision/PluginLoader.h) register capturing factories at
// runtime, which is why the factory is a std::function.
template <class Interface, class... Args> class Registry {
  public:
    using Factory = std::function<std::unique_ptr<Interface>(Args...)>;

    static Registry &instance() {
        static Registry registry;
        return registry;
    }

    // Owns the name. Registration is restricted to initialization before freeze().
    bool add(std::string_view name, Factory factory) {
        std::lock_guard lock(mutex_);
        return !frozen_ && !name.empty() && factory &&
               factories_.emplace(std::string(name), std::move(factory)).second;
    }

    std::unique_ptr<Interface> create(std::string_view name, Args... args) const {
        Factory factory;
        {
            std::lock_guard lock(mutex_);
            const auto it = factories_.find(std::string(name));
            if (it == factories_.end())
                return nullptr;
            factory = it->second;
        }
        return factory(args...);
    }

    template <class Visitor> void for_each_name(Visitor &&visitor) const {
        std::vector<std::string_view> names;
        {
            std::lock_guard lock(mutex_);
            for (const auto &entry : factories_)
                names.emplace_back(entry.first);
        }
        std::sort(names.begin(), names.end());
        for (const auto name : names)
            visitor(name);
    }
    void freeze() {
        std::lock_guard lock(mutex_);
        frozen_ = true;
    }
    bool contains(std::string_view name) const {
        std::lock_guard lock(mutex_);
        return factories_.contains(std::string(name));
    }

  private:
    Registry() = default;

    mutable std::mutex mutex_;
    bool frozen_ = false;
    std::unordered_map<std::string, Factory> factories_;
};

// Registration macro: place next to the backend class, in the backend's own
// .cpp (usage style matches the START_REGISTER macro of the Blueprint
// project: one line next to the class is enough). Runs before main(), so the
// backend is available to any later caller.//
//   ARFW_REGISTER_BACKEND(ar::vision::MatcherRegistry, "ssim-cuda",
//       [](const ar::vision::MatcherConfig& config)
//           -> std::unique_ptr<ar::vision::IImageMatcher> {
//           return std::make_unique<ar::SsimCudaMatcher>(config);
//       })
//
// NAME must be a string literal; FACTORY must be a capture-less lambda or a
// plain function convertible to the registry's factory signature.
#define ARFW_JOIN_IMPL(A, B) A##B
#define ARFW_JOIN(A, B) ARFW_JOIN_IMPL(A, B)
#define ARFW_REGISTER_BACKEND(REGISTRY, NAME, FACTORY)                                                       \
    namespace {                                                                                              \
    const bool ARFW_JOIN(k_arfw_registered_, __COUNTER__) = [] {                                             \
        return REGISTRY::instance().add(NAME, FACTORY);                                                      \
    }();                                                                                                     \
    }

} // namespace ar::core
