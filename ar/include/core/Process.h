#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>
#include "core/Str.hpp"
namespace ar::core {
inline std::string quote_arg(const std::string &arg) {
    if (arg.empty())
        return "\"\"";
    if (arg.find_first_of(" \t\"") == std::string::npos)
        return arg;
    std::string out = "\"";
    size_t slashes = 0;
    for (char c : arg) {
        if (c == '\\')
            ++slashes;
        else {
            out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
            out += c;
            slashes = 0;
        }
    }
    out.append(slashes * 2, '\\');
    return out + '"';
}
enum class ProcessState { exited, start_failed, timed_out, cancelled, output_limit, io_failed };
struct ProcessOptions {
    std::chrono::milliseconds timeout{30000};
    size_t max_output_bytes = 32 * 1024 * 1024;
    std::filesystem::path working_directory;
    std::stop_token stop;
    std::function<bool()> cancelled;
};
struct ProcessResult {
    ProcessState state = ProcessState::start_failed;
    uint32_t exit_code = UINT32_MAX;
    uint32_t native_error = 0;
    std::vector<uint8_t> stdout_bytes;
    std::string stderr_text;
    std::chrono::milliseconds elapsed{};
    bool success() const noexcept { return state == ProcessState::exited && exit_code == 0; }
    std::string stdout_text() const { return {stdout_bytes.begin(), stdout_bytes.end()}; }
};
#ifdef _WIN32
class UniqueHandle {
  public:
    explicit UniqueHandle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle &) = delete;
    UniqueHandle &operator=(const UniqueHandle &) = delete;
    HANDLE get() const noexcept { return value_; }
    void reset(HANDLE value = nullptr) noexcept {
        if (value_ && value_ != INVALID_HANDLE_VALUE)
            CloseHandle(value_);
        value_ = value;
    }

  private:
    HANDLE value_;
};
// PeekNamedPipe prevents an idle child from blocking deadline/cancellation checks.
inline ProcessResult run_process_command(const std::filesystem::path &program, const std::string &arguments,
                                         const ProcessOptions &options = {}) {
    ProcessResult result;
    const auto start = std::chrono::steady_clock::now();
    const auto finish = [&] {
        result.elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        return result;
    };
    const auto cancelled = [&] {
        return options.stop.stop_requested() || (options.cancelled && options.cancelled());
    };
    if (cancelled()) {
        result.state = ProcessState::cancelled;
        return finish();
    }
    if (program.empty() || options.timeout.count() <= 0 || options.max_output_bytes == 0)
        return finish();
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE out_read = nullptr, out_write = nullptr, err_read = nullptr, err_write = nullptr;
    if (!CreatePipe(&out_read, &out_write, &sa, 0)) {
        result.native_error = GetLastError();
        return finish();
    }
    UniqueHandle stdout_read(out_read), stdout_write(out_write);
    if (!CreatePipe(&err_read, &err_write, &sa, 0)) {
        result.native_error = GetLastError();
        return finish();
    }
    UniqueHandle stderr_read(err_read), stderr_write(err_write);
    if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0)) {
        result.native_error = GetLastError();
        return finish();
    }
    UniqueHandle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (input.get() == INVALID_HANDLE_VALUE) {
        result.native_error = GetLastError();
        return finish();
    }
    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    std::vector<uint8_t> attributes(attribute_size);
    auto *list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &attribute_size)) {
        result.native_error = GetLastError();
        return finish();
    }
    struct AttributeGuard {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeGuard() { DeleteProcThreadAttributeList(list); }
    } guard{list};
    HANDLE inherited[]{out_write, err_write, input.get()};
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited),
                                   nullptr, nullptr)) {
        result.native_error = GetLastError();
        return finish();
    }
    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.get() ||
        !SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        result.native_error = GetLastError();
        return finish();
    }
    auto command =
        utf8_to_wide(quote_arg(wide_to_utf8(program.wstring())) + (arguments.empty() ? "" : " " + arguments));
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = input.get();
    si.StartupInfo.hStdOutput = out_write;
    si.StartupInfo.hStdError = err_write;
    si.lpAttributeList = list;
    PROCESS_INFORMATION pi{};
    const auto cwd = options.working_directory.wstring();
    if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                        cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi)) {
        result.native_error = GetLastError();
        return finish();
    }
    UniqueHandle process(pi.hProcess), thread(pi.hThread);
    stdout_write.reset();
    stderr_write.reset();
    input.reset();
    if (!AssignProcessToJobObject(job.get(), process.get()) || ResumeThread(thread.get()) == DWORD(-1)) {
        result.native_error = GetLastError();
        TerminateProcess(process.get(), UINT32_MAX);
        WaitForSingleObject(process.get(), 1000);
        return finish();
    }
    const auto deadline = start + options.timeout;
    auto drain = [&](HANDLE pipe, bool stderr_stream) {
        for (int round = 0; round < 16; ++round) {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
                const auto error = GetLastError();
                if (error == ERROR_BROKEN_PIPE)
                    return true;
                result.native_error = error;
                result.state = ProcessState::io_failed;
                return false;
            }
            if (!available)
                return true;
            char buffer[4096];
            DWORD read = 0;
            if (!ReadFile(pipe, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr)) {
                result.native_error = GetLastError();
                result.state = ProcessState::io_failed;
                return false;
            }
            if (result.stdout_bytes.size() + result.stderr_text.size() + read > options.max_output_bytes) {
                result.state = ProcessState::output_limit;
                return false;
            }
            if (stderr_stream)
                result.stderr_text.append(buffer, read);
            else
                result.stdout_bytes.insert(result.stdout_bytes.end(), buffer, buffer + read);
        }
        return true;
    };
    for (;;) {
        if (cancelled()) {
            result.state = ProcessState::cancelled;
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.state = ProcessState::timed_out;
            break;
        }
        if (!drain(out_read, false) || !drain(err_read, true))
            break;
        const DWORD wait = WaitForSingleObject(process.get(), 5);
        if (wait == WAIT_OBJECT_0) {
            result.state = ProcessState::exited;
            DWORD exit_code = 0;
            GetExitCodeProcess(process.get(), &exit_code);
            result.exit_code = exit_code;
            TerminateJobObject(job.get(), 0);
            for (int i = 0; i < 512; ++i) {
                DWORD a = 0, b = 0;
                PeekNamedPipe(out_read, nullptr, 0, nullptr, &a, nullptr);
                PeekNamedPipe(err_read, nullptr, 0, nullptr, &b, nullptr);
                if (!a && !b)
                    break;
                if (!drain(out_read, false) || !drain(err_read, true))
                    break;
            }
            return finish();
        }
        if (wait == WAIT_FAILED) {
            result.native_error = GetLastError();
            result.state = ProcessState::io_failed;
            break;
        }
    }
    TerminateJobObject(job.get(), UINT32_MAX);
    WaitForSingleObject(process.get(), 1000);
    DWORD exit_code = 0;
    GetExitCodeProcess(process.get(), &exit_code);
    result.exit_code = exit_code;
    return finish();
}
#else
inline ProcessResult run_process_command(const std::filesystem::path &, const std::string &,
                                         const ProcessOptions & = {}) {
    return {};
}
#endif
inline ProcessResult execute_process(const std::filesystem::path &program,
                                     const std::vector<std::string> &arguments,
                                     const ProcessOptions &options = {}) {
    std::string command;
    for (const auto &arg : arguments) {
        if (!command.empty())
            command += ' ';
        command += quote_arg(arg);
    }
    return run_process_command(program, command, options);
}
} // namespace ar::core
