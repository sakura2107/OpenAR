#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Log/MiniLog.h>

void ar::miniLogInit(ar::level lv) {
    //default logger
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::sink_ptr sink_p = { console_sink };
    auto logger = std::make_shared<spdlog::logger>("console sink", sink_p);
    spdlog::set_default_logger(logger);
    //default level
    if (lv == ar::level::trace) spdlog::set_level(spdlog::level::trace);
    if (lv == ar::level::debug) spdlog::set_level(spdlog::level::debug);
    if (lv == ar::level::info) spdlog::set_level(spdlog::level::info);
    if (lv == ar::level::warn) spdlog::set_level(spdlog::level::warn);
    if (lv == ar::level::err) spdlog::set_level(spdlog::level::err);
    if (lv == ar::level::critical) spdlog::set_level(spdlog::level::critical);
}

void ar::setMiniLogFileSink(const std::string& path, ar::level lv, const size_t& milliseconds) {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
    spdlog::sink_ptr sink_p = { file_sink };
    auto logger = std::make_shared<spdlog::logger>("file sink", sink_p);
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::milliseconds(milliseconds));
    if (lv == ar::level::trace) spdlog::set_level(spdlog::level::trace);
    if (lv == ar::level::debug) spdlog::set_level(spdlog::level::debug);
    if (lv == ar::level::info) spdlog::set_level(spdlog::level::info);
    if (lv == ar::level::warn) spdlog::set_level(spdlog::level::warn);
    if (lv == ar::level::err) spdlog::set_level(spdlog::level::err);
    if (lv == ar::level::critical) spdlog::set_level(spdlog::level::critical);
}

void ar::setMiniLogConsoleSink(const std::string& path, ar::level lv) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::sink_ptr sink_p = { console_sink };
    auto logger = std::make_shared<spdlog::logger>("console sink", sink_p);
    spdlog::set_default_logger(logger);
    if (lv == ar::level::trace) spdlog::set_level(spdlog::level::trace);
    if (lv == ar::level::debug) spdlog::set_level(spdlog::level::debug);
    if (lv == ar::level::info) spdlog::set_level(spdlog::level::info);
    if (lv == ar::level::warn) spdlog::set_level(spdlog::level::warn);
    if (lv == ar::level::err) spdlog::set_level(spdlog::level::err);
    if (lv == ar::level::critical) spdlog::set_level(spdlog::level::critical);
}

void ar::setMiniLogMultiSinks(const std::string& path, level lv, const size_t& milliseconds) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
    spdlog::sinks_init_list multi_sinks = { console_sink, file_sink };
    auto logger = std::make_shared<spdlog::logger>("multi sinks", multi_sinks);
    spdlog::set_default_logger(logger);
    if (lv == ar::level::trace) spdlog::set_level(spdlog::level::trace);
    if (lv == ar::level::debug) spdlog::set_level(spdlog::level::debug);
    if (lv == ar::level::info) spdlog::set_level(spdlog::level::info);
    if (lv == ar::level::warn) spdlog::set_level(spdlog::level::warn);
    if (lv == ar::level::err) spdlog::set_level(spdlog::level::err);
    if (lv == ar::level::critical) spdlog::set_level(spdlog::level::critical);
}

void ar::setMiniLogLevel(ar::level lv) {
    if (lv == ar::level::trace) spdlog::set_level(spdlog::level::trace);
    if (lv == ar::level::debug) spdlog::set_level(spdlog::level::debug);
    if (lv == ar::level::info) spdlog::set_level(spdlog::level::info);
    if (lv == ar::level::warn) spdlog::set_level(spdlog::level::warn);
    if (lv == ar::level::err) spdlog::set_level(spdlog::level::err);
    if (lv == ar::level::critical) spdlog::set_level(spdlog::level::critical);
}

void ar::setMiniLogFlushTime(const size_t& milliseconds) {
    spdlog::flush_every(std::chrono::milliseconds(milliseconds));
}

void ar::miniLogShutDown() {
    spdlog::shutdown();
}

void ar::detail::callMiniLog(const std::string& msg, ar::level lv) {
    if (lv == ar::level::trace) spdlog::trace(msg);
    if (lv == ar::level::debug) spdlog::debug(msg);
    if (lv == ar::level::info) spdlog::info(msg);
    if (lv == ar::level::warn) spdlog::warn(msg);
    if (lv == ar::level::err) spdlog::error(msg);
    if (lv == ar::level::critical) spdlog::critical(msg);
}