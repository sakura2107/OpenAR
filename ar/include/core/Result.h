#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
namespace ar::core {
enum class ErrorCode {
    invalid_argument,
    unsupported,
    offline,
    timeout,
    cancelled,
    dependency_missing,
    model_invalid,
    backend_failed,
    io_failed,
    resource_invalid,
    conflict,
    stale_frame,
    budget_exceeded
};
inline const char *error_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::invalid_argument:
        return "invalid_argument";
    case ErrorCode::unsupported:
        return "unsupported";
    case ErrorCode::offline:
        return "offline";
    case ErrorCode::timeout:
        return "timeout";
    case ErrorCode::cancelled:
        return "cancelled";
    case ErrorCode::dependency_missing:
        return "dependency_missing";
    case ErrorCode::model_invalid:
        return "model_invalid";
    case ErrorCode::backend_failed:
        return "backend_failed";
    case ErrorCode::io_failed:
        return "io_failed";
    case ErrorCode::resource_invalid:
        return "resource_invalid";
    case ErrorCode::conflict:
        return "conflict";
    case ErrorCode::stale_frame:
        return "stale_frame";
    case ErrorCode::budget_exceeded:
        return "budget_exceeded";
    }
    return "unknown";
}
struct Error {
    ErrorCode code = ErrorCode::backend_failed;
    std::string operation;
    std::string message;
    std::string backend;
    int64_t native_code = 0;
};
template <class T> class Result {
  public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}
    explicit operator bool() const noexcept { return std::holds_alternative<T>(data_); }
    T &value() { return std::get<T>(data_); }
    const T &value() const { return std::get<T>(data_); }
    const Error &error() const { return std::get<Error>(data_); }

  private:
    std::variant<T, Error> data_;
};
template <> class Result<void> {
  public:
    Result() = default;
    Result(Error error) : data_(std::move(error)) {}
    explicit operator bool() const noexcept { return std::holds_alternative<std::monostate>(data_); }
    const Error &error() const { return std::get<Error>(data_); }

  private:
    std::variant<std::monostate, Error> data_;
};
inline Error failure(ErrorCode code, std::string operation, std::string message, std::string backend = {},
                     int64_t native_code = 0) {
    return {code, std::move(operation), std::move(message), std::move(backend), native_code};
}
} // namespace ar::core
