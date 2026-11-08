#pragma once
#include "core/Str.hpp"
#include "core/Result.h"
#include <winhttp.h>
#include <chrono>
#include <stop_token>
#pragma comment(lib, "winhttp.lib")
namespace ar::net {
struct HttpOptions {
    uint32_t timeout_ms = 15000;
    size_t max_response_bytes = 16 * 1024 * 1024;
    bool allow_invalid_certificate = false;
    std::stop_token stop;
};
struct HttpResponse {
    int status = 0;
    std::string body, headers;
};
class HttpHandle {
  public:
    explicit HttpHandle(HINTERNET handle) : handle_(handle) {}
    ~HttpHandle() {
        if (handle_)
            WinHttpCloseHandle(handle_);
    }
    operator HINTERNET() const { return handle_; }
    HttpHandle(const HttpHandle &) = delete;
    HttpHandle &operator=(const HttpHandle &) = delete;

  private:
    HINTERNET handle_;
};
inline ar::core::Result<HttpResponse> request_http(const std::string &method, const std::string &url,
                                                   const std::string &body, const HttpOptions &options = {}) {
    using ar::core::ErrorCode;
    using ar::core::failure;
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::milliseconds(options.timeout_ms);
    auto error = [&] {
        return failure(ErrorCode::io_failed, "http", "WinHTTP operation failed", {}, GetLastError());
    };
    auto check = [&]() -> ar::core::Result<void> {
        if (options.stop.stop_requested())
            return failure(ErrorCode::cancelled, "http", "Request cancelled");
        if (std::chrono::steady_clock::now() >= deadline)
            return failure(ErrorCode::timeout, "http", "Request deadline reached");
        return {};
    };
    if (!options.timeout_ms || !options.max_response_bytes || body.size() > UINT32_MAX)
        return failure(ErrorCode::invalid_argument, "http", "Invalid timeout/byte budget");
    auto checked = check();
    if (!checked)
        return checked.error();
    const auto wurl = ar::core::utf8_to_wide(url), wmethod = ar::core::utf8_to_wide(method);
    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = components.dwHostNameLength = components.dwUrlPathLength =
        components.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &components) ||
        (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS))
        return failure(ErrorCode::invalid_argument, "http", "Invalid HTTP URL");
    const auto host = std::wstring(components.lpszHostName, components.dwHostNameLength);
    const auto path = std::wstring(components.lpszUrlPath, components.dwUrlPathLength) +
                      std::wstring(components.lpszExtraInfo, components.dwExtraInfoLength);
    HttpHandle session(WinHttpOpen(L"OpenAR/2", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
        return error();
    if (!WinHttpSetTimeouts(session, options.timeout_ms, options.timeout_ms, options.timeout_ms,
                            options.timeout_ms))
        return error();
    HttpHandle connection(WinHttpConnect(session, host.c_str(), components.nPort, 0));
    if (!connection)
        return error();
    const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    HttpHandle request(WinHttpOpenRequest(connection, wmethod.c_str(), path.empty() ? L"/" : path.c_str(),
                                          nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          secure ? WINHTTP_FLAG_SECURE : 0));
    if (!request)
        return error();
    if (secure && options.allow_invalid_certificate) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        if (!WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags)))
            return error();
    }
    const wchar_t *headers =
        body.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(request, headers, body.empty() ? 0 : DWORD(-1),
                            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(body.data()),
                            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
        !WinHttpReceiveResponse(request, nullptr))
        return error();
    checked = check();
    if (!checked)
        return checked.error();
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX))
        return error();
    HttpResponse response;
    response.status = static_cast<int>(status);
    DWORD header_size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
                        &header_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::wstring raw(header_size / sizeof(wchar_t), L'\0');
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                                 raw.data(), &header_size, WINHTTP_NO_HEADER_INDEX))
            return error();
        while (!raw.empty() && raw.back() == 0)
            raw.pop_back();
        response.headers = ar::core::wide_to_utf8(raw);
    }
    DWORD content_length = 0, content_size = sizeof(content_length);
    const bool known_length =
        WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &content_length, &content_size,
                            WINHTTP_NO_HEADER_INDEX) != FALSE;
    if (known_length && content_length > options.max_response_bytes)
        return failure(ErrorCode::io_failed, "http", "Response exceeds byte budget");
    for (;;) {
        checked = check();
        if (!checked)
            return checked.error();
        const auto remaining =
            static_cast<int>(std::max<int64_t>(1, std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      deadline - std::chrono::steady_clock::now())
                                                      .count()));
        WinHttpSetTimeouts(request, remaining, remaining, remaining, remaining);
        char chunk[65536];
        DWORD received = 0;
        if (!WinHttpReadData(request, chunk, sizeof(chunk), &received))
            return error();
        if (!received)
            break;
        if (response.body.size() + received > options.max_response_bytes)
            return failure(ErrorCode::io_failed, "http", "Response exceeds byte budget");
        response.body.append(chunk, received);
    }
    if (known_length && response.body.size() != content_length)
        return failure(ErrorCode::io_failed, "http", "Incomplete HTTP response");
    checked = check();
    if (!checked)
        return checked.error();
    return response;
}
// Legacy convenience API; certificate validation is now enabled by default.
inline bool http_request(const std::string &method, const std::string &url, const std::string &body,
                         std::string &response, int *status = nullptr, std::string *headers = nullptr,
                         int timeout_ms = 15000, bool insecure = false) {
    response.clear();
    if (status)
        *status = 0;
    if (headers)
        headers->clear();
    if (timeout_ms <= 0)
        return false;
    HttpOptions options;
    options.timeout_ms = timeout_ms;
    options.allow_invalid_certificate = insecure;
    auto result = request_http(method, url, body, options);
    if (!result)
        return false;
    response = std::move(result.value().body);
    if (status)
        *status = result.value().status;
    if (headers)
        *headers = std::move(result.value().headers);
    return result.value().status >= 200 && result.value().status < 300;
}
inline bool http_get(const std::string &url, std::string &response, int *status = nullptr,
                     std::string *headers = nullptr) {
    return http_request("GET", url, {}, response, status, headers);
}
} // namespace ar::net
