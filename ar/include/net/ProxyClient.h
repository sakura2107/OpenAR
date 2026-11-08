#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "log/Logger.h"
namespace ar::net {
enum class ProxyOwnership { owned, external };
struct ProxyConfig {
    ProxyOwnership ownership = ProxyOwnership::owned;
    std::filesystem::path kernel, working_directory, adb_path;
    std::string subscription_url, serial = "127.0.0.1:16384", api_url;
    uint16_t mixed_port = 7890, api_port = 9090;
    uint32_t timeout_ms = 15000;
    bool allow_invalid_subscription_certificate = false;
};
// Optional host service. External kernels are never stopped; selecting their
// node affects every client sharing that kernel. Use distinct ports for isolation.
class ProxyClient {
  public:
    explicit ProxyClient(ProxyConfig config, ar::log::ILogger *logger = nullptr);
    explicit ProxyClient(const std::string &subscribe_url, int32_t adb_port = 16384,
                         ar::log::ILogger *logger = nullptr, std::string adb_path = ".\\adb.exe");
    ~ProxyClient();
    ProxyClient(const ProxyClient &) = delete;
    ProxyClient &operator=(const ProxyClient &) = delete;
    bool Initialize();
    std::vector<std::string> GetNodeNames();
    bool EnableProxy(const std::string &node_name = {});
    bool DisableProxy();
    bool IsEnabled();
    bool IsReady() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace ar::net
