#include "net/ProxyClient.h"
#include "net/Http.h"
#include "control/AdbTransport.h"
#include "core/Process.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <fstream>
#include <atomic>
#include <mutex>
#include <set>
#include <optional>
namespace ar::net {
namespace {
std::atomic<uint64_t> counter{1};
std::mutex owners_mutex;
std::set<std::string> owners;
std::filesystem::path executable_dir() {
    std::wstring value(32768, L'\0');
    value.resize(GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size())));
    return std::filesystem::path(value).parent_path();
}
std::string trim(std::string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
        text.pop_back();
    return text;
}
} // namespace
struct ProxyClient::State {
    ProxyConfig config;
    ar::log::ILogger *logger;
    ar::control::AdbClient adb;
    ar::core::UniqueHandle job, process;
    bool ready = false, enabled = false, reverse_owned = false, claimed = false;
    std::optional<std::string> previous;
    State(ProxyConfig c, ar::log::ILogger *l)
        : config(std::move(c)), logger(l),
          adb(config.adb_path.empty() ? executable_dir() / L"adb.exe" : config.adb_path, l) {
        if (!config.mixed_port || !config.api_port || config.mixed_port == config.api_port ||
            !config.timeout_ms || config.serial.empty())
            throw std::invalid_argument("Invalid proxy configuration");
        if (config.api_url.empty())
            config.api_url = "http://127.0.0.1:" + std::to_string(config.api_port);
        if (config.kernel.empty())
            config.kernel = executable_dir() / L"mihomo/mihomo.exe";
        config.kernel = std::filesystem::absolute(config.kernel);
        if (config.working_directory.empty())
            config.working_directory = std::filesystem::temp_directory_path() / L"OpenAR-proxy" /
                                       ar::core::utf8_to_wide(std::to_string(GetCurrentProcessId()) + "-" +
                                                              std::to_string(counter.fetch_add(1)));
        config.working_directory = std::filesystem::absolute(config.working_directory);
        adb.set_context({}, config.timeout_ms);
    }
    void log(std::string_view text) {
        if (logger)
            logger->log(ar::log::Level::error, text);
    }
    bool api(const char *method, const std::string &endpoint, std::string &output,
             const std::string &body = {}, uint32_t timeout = 0) {
        HttpOptions options;
        options.timeout_ms = timeout ? timeout : config.timeout_ms;
        auto response = request_http(method, config.api_url + endpoint, body, options);
        if (!response)
            return false;
        output = std::move(response.value().body);
        return response.value().status >= 200 && response.value().status < 300;
    }
    bool command(const std::string &args, std::string &output) {
        return adb.run("-s " + ar::core::quote_arg(config.serial) + " " + args, output);
    }
    void stop() {
        if (process.get()) {
            TerminateProcess(process.get(), 0);
            WaitForSingleObject(process.get(), 1000);
            process.reset();
        }
        job.reset();
        ready = false;
    }
    void release() {
        std::lock_guard lock(owners_mutex);
        if (claimed)
            owners.erase(config.serial);
        claimed = false;
    }
};
ProxyClient::ProxyClient(ProxyConfig config, ar::log::ILogger *logger)
    : state_(std::make_unique<State>(std::move(config), logger)) {}
ProxyClient::ProxyClient(const std::string &url, int32_t port, ar::log::ILogger *logger, std::string adb_path)
    : ProxyClient(
          [&] {
              if (port <= 0 || port > 65535)
                  throw std::invalid_argument("Invalid adb port");
              ProxyConfig config;
              config.subscription_url = url;
              config.serial = "127.0.0.1:" + std::to_string(port);
              config.adb_path = ar::core::utf8_to_wide(adb_path);
              return config;
          }(),
          logger) {}
ProxyClient::~ProxyClient() {
    try {
        if (state_->previous) {
            state_->adb.set_context({}, 1000);
            DisableProxy();
        }
        state_->stop();
        state_->release();
    } catch (...) {
    }
}
bool ProxyClient::Initialize() {
    auto &s = *state_;
    if (s.ready)
        return true;
    std::string response;
    if (s.config.ownership == ProxyOwnership::external) {
        s.ready = s.api("GET", "/version", response);
        return s.ready;
    }
    // Never adopt a process just because a configured port happens to answer.
    if (s.api("GET", "/version", response, {}, 500)) {
        s.log("Owned proxy port already in use; configure isolated ports or external ownership");
        return false;
    }
    if (!std::filesystem::is_regular_file(s.config.kernel) || s.config.subscription_url.empty())
        return false;
    HttpOptions options;
    options.timeout_ms = s.config.timeout_ms;
    options.allow_invalid_certificate = s.config.allow_invalid_subscription_certificate;
    auto url = s.config.subscription_url;
    url += (url.find('?') == std::string::npos ? "?" : "&");
    url += "flag=clash";
    auto subscription = request_http("GET", url, {}, options);
    if (!subscription || subscription.value().status < 200 || subscription.value().status >= 300 ||
        subscription.value().body.find("proxies:") == std::string::npos)
        return false;
    std::filesystem::create_directories(s.config.working_directory);
    {
        std::ofstream file(s.config.working_directory / L"sub.yaml", std::ios::binary);
        if (!(file << subscription.value().body))
            return false;
    }
    {
        std::ofstream file(s.config.working_directory / L"config.yaml", std::ios::binary);
        file << "mixed-port: " << s.config.mixed_port
             << "\nexternal-controller: 127.0.0.1:" << s.config.api_port
             << "\nmode: rule\nlog-level: warning\nallow-lan: false\nprofile:\n  store-selected: "
                "false\nproxy-providers:\n  sub:\n    type: file\n    path: ./sub.yaml\nproxy-groups:\n  - "
                "name: PROXY\n    type: select\n    use: [sub]\nrules:\n  - MATCH,PROXY\n";
        if (!file)
            return false;
    }
    s.job.reset(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!s.job.get() ||
        !SetInformationJobObject(s.job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        return false;
    auto command = ar::core::utf8_to_wide(
        ar::core::quote_arg(ar::core::wide_to_utf8(s.config.kernel.wstring())) + " -d " +
        ar::core::quote_arg(ar::core::wide_to_utf8(s.config.working_directory.wstring())) + " -f " +
        ar::core::quote_arg(ar::core::wide_to_utf8((s.config.working_directory / L"config.yaml").wstring())));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(s.config.kernel.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, s.config.working_directory.c_str(),
                        &startup, &process))
        return false;
    s.process.reset(process.hProcess);
    ar::core::UniqueHandle thread(process.hThread);
    if (!AssignProcessToJobObject(s.job.get(), s.process.get()) || ResumeThread(thread.get()) == DWORD(-1)) {
        s.stop();
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(s.config.timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (WaitForSingleObject(s.process.get(), 100) == WAIT_OBJECT_0)
            break;
        if (s.api("GET", "/version", response, {}, 200)) {
            s.ready = true;
            return true;
        }
    }
    s.stop();
    return false;
}
std::vector<std::string> ProxyClient::GetNodeNames() {
    auto &s = *state_;
    std::vector<std::string> result;
    if (!Initialize())
        return result;
    std::string response;
    if (!s.api("GET", "/providers/proxies/sub", response))
        return result;
    rapidjson::Document doc;
    doc.Parse(response.c_str());
    if (!doc.IsObject() || !doc.HasMember("proxies") || !doc["proxies"].IsArray())
        return result;
    for (const auto &item : doc["proxies"].GetArray())
        if (item.IsObject() && item.HasMember("name") && item["name"].IsString())
            result.emplace_back(item["name"].GetString());
    return result;
}
bool ProxyClient::EnableProxy(const std::string &node) {
    auto &s = *state_;
    if (!Initialize())
        return false;
    {
        std::lock_guard lock(owners_mutex);
        if (!s.claimed) {
            if (!owners.insert(s.config.serial).second) {
                s.log("Another proxy client owns this device");
                return false;
            }
            s.claimed = true;
        }
    }
    std::string response;
    if (!s.adb.connect(s.config.serial, response)) {
        s.release();
        return false;
    }
    if (!s.previous) {
        if (!s.command("shell settings get global http_proxy", response)) {
            s.release();
            return false;
        }
        s.previous = trim(response);
    }
    if (!node.empty()) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject();
        writer.Key("name");
        writer.String(node.data(), static_cast<rapidjson::SizeType>(node.size()));
        writer.EndObject();
        if (!s.api("PUT", "/proxies/PROXY", response, buffer.GetString()))
            return false;
    }
    const auto port = std::to_string(s.config.mixed_port);
    if (!s.reverse_owned) {
        if (!s.command("reverse --list", response))
            return false;
        if (response.find("tcp:" + port) != std::string::npos) {
            s.log("ADB reverse port already in use");
            return false;
        }
        if (!s.command("reverse tcp:" + port + " tcp:" + port, response))
            return false;
        s.reverse_owned = true;
    }
    if (!s.command("shell settings put global http_proxy 127.0.0.1:" + port, response))
        return false;
    s.enabled = IsEnabled();
    return s.enabled;
}
bool ProxyClient::DisableProxy() {
    auto &s = *state_;
    if (!s.previous)
        return true;
    std::string response;
    if (!s.adb.connect(s.config.serial, response))
        return false;
    bool restored = s.command(*s.previous == "null" ? "shell settings delete global http_proxy"
                                                    : "shell settings put global http_proxy " +
                                                          ar::core::quote_arg(*s.previous),
                              response);
    if (restored)
        restored =
            s.command("shell settings get global http_proxy", response) && trim(response) == *s.previous;
    if (!restored) {
        s.log("Could not restore device proxy settings");
        return false;
    }
    if (s.reverse_owned &&
        !s.command("reverse --remove tcp:" + std::to_string(s.config.mixed_port), response))
        return false;
    s.previous.reset();
    s.reverse_owned = false;
    s.enabled = false;
    s.release();
    return true;
}
bool ProxyClient::IsEnabled() {
    auto &s = *state_;
    std::string response;
    return s.command("shell settings get global http_proxy", response) &&
           trim(response) == "127.0.0.1:" + std::to_string(s.config.mixed_port);
}
bool ProxyClient::IsReady() const {
    return state_->ready;
}
} // namespace ar::net
