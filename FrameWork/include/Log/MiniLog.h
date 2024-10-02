#pragma once
#include <format>
#include <string>
#include <memory>

namespace ar{
enum class level {
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    };
namespace detail {

void callMiniLog(const std::string& msg, level lv);

}

void miniLogInit(level lv = level::debug);

void setMiniLogFileSink(const std::string& path,level lv = level::debug, const size_t& milliseconds = 1000);

void setMiniLogConsoleSink(const std::string& path,level lv = level::debug);

void setMiniLogMultiSinks(const std::string& path,level lv = level::debug, const size_t& milliseconds = 1000);

void setMiniLogLevel(level lv);

void setMiniLogFlushTime(const size_t& milliseconds);

void miniLogShutDown();

template <typename... _Types>
void trace(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::trace);
}

template <typename... _Types>
void debug(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::debug);
}

template <typename... _Types>
void info(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::info);
}

template <typename... _Types>
void warn(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::warn);
}

template <typename... _Types>
void error(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::err);
}


template <typename... _Types>
void critical(const std::format_string<_Types...> _Fmt, _Types&&... _Args) {
    std::string msg = std::vformat(_Fmt.get(), std::make_format_args(_Args...));
    detail::callMiniLog(msg, level::critical);
}

}