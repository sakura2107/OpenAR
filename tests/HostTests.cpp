#include "host/JobHost.h"
#include <condition_variable>
#include <fstream>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <Windows.h>
namespace {
using namespace ar::host;
using namespace std::chrono_literals;
void require(bool b, const char *m) {
    if (!b)
        throw std::runtime_error(m);
}
JobSnapshot wait(JobHost &h, const std::string &id) {
    auto end = std::chrono::steady_clock::now() + 3s;
    do {
        auto j = h.get(id);
        require(bool(j), "job vanished");
        if (terminal(j.value().status))
            return j.value();
        std::this_thread::sleep_for(1ms);
    } while (std::chrono::steady_clock::now() < end);
    throw std::runtime_error("job did not finish");
}
std::vector<DeviceInfo> devices() {
    return {{"a", "A", "test", "", true, {"go"}}, {"b", "B", "test", "", true, {"go"}}};
}
HostOptions options(const std::filesystem::path &p) {
    HostOptions o;
    o.state_directory = p;
    o.trace_root = p / L"traces";
    return o;
}
struct Gate {
    std::mutex m;
    std::condition_variable_any cv;
    int entered = 0;
    bool release = false;
    JobResult run(const JobRequest &, RunContext &ctx) {
        std::unique_lock lock(m);
        ++entered;
        cv.notify_all();
        cv.wait(lock, ctx.stop, [&] { return release; });
        return {ctx.stop.stop_requested() ? JobStatus::cancelled : JobStatus::succeeded, "", "{}", {}};
    }
    bool starts(int n) {
        std::unique_lock lock(m);
        return cv.wait_for(lock, 2s, [&] { return entered >= n; });
    }
    void open() {
        std::lock_guard lock(m);
        release = true;
        cv.notify_all();
    }
};
} // namespace
void host_contract_tests(const std::filesystem::path &root,
                         const std::function<void(const char *, const std::function<void()> &)> &test) {
    test("host FIFO serializes one device while other devices progress", [&] {
        Gate gate;
        std::mutex mutex;
        std::vector<std::string> order;
        JobHost h(
            devices(), {{"go", "Go"}},
            [&](auto &r, auto &ctx) {
                {
                    std::lock_guard lock(mutex);
                    order.push_back(r.request_key);
                }
                return gate.run(r, ctx);
            },
            options(root / L"parallel"));
        auto a = h.submit({"a", "go", "{}", "a1"}).value();
        require(gate.starts(1), "first device did not start");
        auto a2 = h.submit({"a", "go", "{}", "a2"}).value();
        auto b = h.submit({"b", "go", "{}", "b1"}).value();
        require(gate.starts(2), "second device was blocked by first");
        require(h.get(a2.id).value().status == JobStatus::queued, "same device ran concurrently");
        gate.open();
        require(wait(h, a.id).status == JobStatus::succeeded &&
                    wait(h, a2.id).status == JobStatus::succeeded &&
                    wait(h, b.id).status == JobStatus::succeeded,
                "jobs failed");
        std::lock_guard lock(mutex);
        require(std::find(order.begin(), order.end(), "a1") < std::find(order.begin(), order.end(), "a2"),
                "FIFO order lost");
    });
    test("host queued and running cancellation does not start cancelled queued work", [&] {
        Gate gate;
        JobHost h(
            devices(), {{"go", "Go"}}, [&](auto &r, auto &c) { return gate.run(r, c); },
            options(root / L"cancel"));
        auto first = h.submit({"a", "go"}).value();
        require(gate.starts(1), "runner not started");
        auto second = h.submit({"a", "go"}).value();
        require(h.cancel(second.id).value().status == JobStatus::cancelled, "queued cancel failed");
        require(bool(h.cancel(first.id)), "running cancel rejected");
        require(wait(h, first.id).status == JobStatus::cancelled, "running cancel ignored");
        require(gate.entered == 1, "queued cancelled task executed");
        require(h.cancel(second.id).value().status == JobStatus::cancelled, "cancel not idempotent");
    });
    test("host idempotency keys reject changed invocations and survive restart", [&] {
        auto o = options(root / L"idempotent");
        std::string id;
        {
            JobHost h(
                devices(), {{"go", "Go"}}, [](auto &, auto &) { return JobResult{JobStatus::succeeded}; }, o);
            auto j = h.submit({"a", "go", "{}", "key"}).value();
            id = j.id;
            require(h.submit({"a", "go", "{}", "key"}).value().id == id, "duplicate created another job");
            auto changed = h.submit({"b", "go", "{}", "key"});
            require(!changed && changed.error().code == ar::core::ErrorCode::conflict,
                    "changed invocation accepted");
            wait(h, id);
        }
        std::atomic<int> calls = 0;
        JobHost h(
            devices(), {{"go", "Go"}},
            [&](auto &, auto &) {
                ++calls;
                return JobResult{JobStatus::succeeded};
            },
            o);
        require(h.submit({"a", "go", "{}", "key"}).value().id == id && calls == 0,
                "restart repeated existing job");
    });
    test("host bounded queues reject excess and terminal metadata can be forgotten", [&] {
        Gate gate;
        auto o = options(root / L"capacity");
        o.max_queued_per_device = 1;
        o.max_jobs = 2;
        JobHost h(devices(), {{"go", "Go"}}, [&](auto &r, auto &c) { return gate.run(r, c); }, o);
        auto a = h.submit({"a", "go"}).value();
        require(gate.starts(1), "runner missing");
        auto b = h.submit({"a", "go"}).value();
        require(!h.submit({"a", "go"}), "queue grew beyond cap");
        require(!h.forget(a.id), "forgot active job");
        h.cancel(b.id);
        require(bool(h.forget(b.id)), "forget failed");
        require(!h.get(b.id), "forgotten metadata retained");
        gate.open();
        wait(h, a.id);
    });
    test("host event gaps are explicit and cursors paginate in order", [&] {
        auto o = options(root / L"events");
        o.max_events = 3;
        JobHost h(
            devices(), {{"go", "Go"}},
            [](auto &, auto &c) {
                for (int n = 0; n < 8; ++n)
                    c.publish("{\"frame\":1}");
                return JobResult{JobStatus::succeeded};
            },
            o);
        auto id = h.submit({"a", "go"}).value().id;
        wait(h, id);
        auto p = h.events(0, 2);
        require(p.gap && p.events.size() == 2 && p.first_sequence > 1, "missing gap/pagination");
        auto next = h.events(p.next_sequence, 2);
        require(!next.gap && next.events.size() == 1 && next.events[0].sequence > p.events.back().sequence,
                "cursor skipped/repeated event");
        require(h.events(UINT64_MAX).gap && !h.events(UINT64_MAX).events.empty(),
                "invalid future cursor cannot recover");
    });
    test("host executor exceptions do not kill the device worker", [&] {
        std::atomic<int> n = 0;
        JobHost h(
            devices(), {{"go", "Go"}},
            [&](auto &, auto &) {
                if (++n == 1)
                    throw std::runtime_error("injected");
                return JobResult{JobStatus::succeeded};
            },
            options(root / L"throw"));
        auto a = h.submit({"a", "go"}).value();
        auto b = h.submit({"a", "go"}).value();
        require(wait(h, a.id).status == JobStatus::failed && wait(h, b.id).status == JobStatus::succeeded,
                "worker lost after exception");
    });
    test("host unfinished stored jobs become interrupted without execution", [&] {
        auto o = options(root / L"interrupted");
        std::filesystem::create_directories(o.state_directory / L"jobs");
        JobSnapshot old;
        old.id = "old-running";
        old.request = {"a", "go"};
        old.status = JobStatus::running;
        std::ofstream(o.state_directory / L"jobs/old-running.json") << snapshot_json(old);
        old.id = "old-queued";
        old.status = JobStatus::queued;
        std::ofstream(o.state_directory / L"jobs/old-queued.json") << snapshot_json(old);
        std::atomic<int> n = 0;
        JobHost h(
            devices(), {{"go", "Go"}},
            [&](auto &, auto &) {
                ++n;
                return JobResult{JobStatus::succeeded};
            },
            o);
        require(h.get("old-running").value().status == JobStatus::interrupted &&
                    h.get("old-queued").value().status == JobStatus::interrupted && n == 0,
                "crash history auto-resumed");
    });
    test("host state directories cannot be opened by two owners", [&] {
        auto o = options(root / L"lock");
        JobHost h(
            devices(), {{"go", "Go"}}, [](auto &, auto &) { return JobResult{JobStatus::succeeded}; }, o);
        bool refused = false;
        try {
            JobHost other(
                devices(), {{"go", "Go"}}, [](auto &, auto &) { return JobResult{JobStatus::succeeded}; }, o);
        } catch (...) {
            refused = true;
        }
        require(refused, "state directory lock was bypassed");
    });
    test("host named device lease prevents other host owners from executing", [&] {
        Gate gate;
        auto d = devices();
        d[0].lock_key = "unit-device-" + std::to_string(GetCurrentProcessId());
        JobHost a(
            d, {{"go", "Go"}}, [&](auto &r, auto &c) { return gate.run(r, c); }, options(root / L"lease-a"));
        auto first = a.submit({"a", "go"}).value();
        require(gate.starts(1), "lease holder missing");
        JobHost b(
            d, {{"go", "Go"}},
            [](auto &, auto &) {
                throw std::runtime_error("must not run");
                return JobResult{};
            },
            options(root / L"lease-b"));
        auto second = b.submit({"a", "go"}).value();
        require(wait(b, second.id).status == JobStatus::blocked, "other owner executed leased device");
        gate.open();
        wait(a, first.id);
    });
    test("host trace confinement and registry validation reject invalid work", [&] {
        auto o = options(root / L"paths");
        JobHost h(
            devices(), {{"go", "Go"}},
            [&](auto &, auto &c) {
                c.trace(root / L"outside");
                return JobResult{JobStatus::succeeded};
            },
            o);
        require(!h.submit({"missing", "go"}) && !h.submit({"a", "unknown"}) && !h.submit({"a", "go", "[]"}),
                "invalid invocation accepted");
        auto a = h.submit({"a", "go"}).value();
        require(wait(h, a.id).status == JobStatus::failed, "escaped trace accepted");
    });
    test("host shutdown cooperatively cancels active work and retains terminal status", [&] {
        Gate g;
        JobHost h(
            devices(), {{"go", "Go"}}, [&](auto &r, auto &c) { return g.run(r, c); },
            options(root / L"shutdown"));
        auto a = h.submit({"a", "go"}).value();
        require(g.starts(1), "runner missing");
        auto b = h.submit({"a", "go"}).value();
        h.shutdown();
        require(h.get(a.id).value().status == JobStatus::cancelled &&
                    h.get(b.id).value().status == JobStatus::cancelled,
                "shutdown left active work");
        require(!h.submit({"a", "go"}), "stopped host accepted work");
    });
}
