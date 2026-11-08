#include "loop/Loop.h"
#include "loop/Simulator.h"
#include "core/Json.h"
#include <fstream>
#include <iostream>
namespace {
using namespace ar::loop;
using ar::runtime::ManualClock;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
cv::Mat patch(int seed) {
    cv::Mat m(24, 24, CV_8UC3);
    cv::RNG rng(seed);
    rng.fill(m, cv::RNG::UNIFORM, 30, 220);
    return m;
}
const cv::Rect A(20, 20, 24, 24), D(130, 20, 24, 24), H(220, 20, 24, 24), P(50, 90, 24, 24),
    E(130, 90, 24, 24);
cv::Mat screen(std::initializer_list<std::pair<cv::Rect, int>> values) {
    cv::Mat im(180, 320, CV_8UC3, cv::Scalar(40, 50, 60));
    for (auto &[r, n] : values)
        patch(n).copyTo(im(r));
    return im;
}
Roi at(cv::Rect r) {
    return roi(r.x, r.y, r.width, r.height);
}
struct World {
    std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
    std::shared_ptr<Simulator> sim;
    std::unique_ptr<ar::runtime::DeviceSession> session;
    std::unique_ptr<Context> ctx;
    World(const std::filesystem::path &out, std::string initial = "home", bool full = true) {
        std::map<std::string, SimState> states;
        states["home"] = {screen({{A, 1}, {H, 3}}), {{A, 0, "done", "go"}, {{}, 4, "home", "back"}}};
        states["done"] = {screen({{D, 2}, {A, 1}}),
                          {{A, 0, "home", "wrong-after-goal"}, {{}, 4, "home", "back"}}};
        states["network"] = {screen({{P, 4}, {A, 1}}), {{P, 0, "network", "network-ack"}}};
        states["popup"] = {screen({{P, 4}, {A, 1}}), {{P, 0, "$return", "popup-close"}}};
        states["loading"] = {screen({{E, 5}}), {}};
        states["unknown"] = {screen({{A, 1}}), {{{}, 4, "home", "back"}}};
        states["done_home"] = {screen({{D, 2}, {H, 3}}), {}};
        states["done_e"] = {screen({{D, 2}, {E, 5}}), {}};
        states["hidden"] = {screen({}), {{{270, 140, 20, 20}, 0, "home", "wake"}}};
        states["timed"] = {screen({{P, 4}}), {}, "hidden", 3000};
        states["busy_popup"] = {screen({{P, 4}, {E, 5}, {A, 1}}), {}};
        sim = std::make_shared<Simulator>(std::move(states), initial, clock);
        session = std::make_unique<ar::runtime::DeviceSession>(
            sim,
            ar::runtime::SessionConfig{"loop-test-" + std::to_string(reinterpret_cast<uintptr_t>(this)),
                                       {320, 180}},
            clock);
        Options options;
        options.trace_root = out;
        options.full_frames = full;
        options.poll = 10ms;
        options.stable_max = 30ms;
        options.operation_timeout = 100ms;
        ctx = std::make_unique<Context>(*session, out, "test", options);
        for (auto [n, seed] : std::initializer_list<std::pair<const char *, int>>{
                 {"A", 1}, {"D", 2}, {"H", 3}, {"P", 4}, {"E", 5}})
            ctx->asset(n, patch(seed));
    }
    Phase step() {
        return Phase("step")
            .goal(see("D", at(D)))
            .on("go", see("A", at(A)), tap().settle(40ms))
            .timeout(300ms)
            .max_frames(70);
    }
    void recovery() {
        ctx->recover(Phase("recover")
                         .goal(see("H", at(H)))
                         .on("back", see("A", at(A)), key(4))
                         .timeout(150ms)
                         .max_frames(30));
    }
    Task task() { return Task("test-task").done(see("D", at(D))).phase(step()).timeout(1500ms).retries(2); }
};
} // namespace
void loop_contract_tests(const std::filesystem::path &root, const std::filesystem::path &out,
                         const std::function<void(const char *, const std::function<void()> &)> &test) {
    test("loop global retry survives more than five notices and throttles local input", [&] {
        World w(out, "network");
        std::vector<int64_t> times;
        w.ctx->global("network-ack", see("P", at(P)), tap().retry_while_present(500ms));
        w.sim->after_action = [&](Simulator &s) {
            times.push_back(w.clock->now_ms());
            s.state(times.size() < 8 ? "network" : "done");
        };
        auto task = Task("repeated-network")
                        .done(see("D", at(D)))
                        .phase(Phase("network-resume")
                                   .goal(see("D", at(D)))
                                   .on("must-not-click-under-notice", see("A", at(A)),
                                       tap().effect().expect(see("D", at(D))))
                                   .timeout(10s)
                                   .max_frames(2000))
                        .timeout(15s);
        auto result = task.run(*w.ctx);
        require(result.status == Status::success, "repeated network notice failed");
        require(times.size() == 8 && w.sim->applied["go"] == 0,
                "local resource action ran during retry cooldown");
        for (size_t k = 1; k < times.size(); ++k)
            require(times[k] - times[k - 1] >= 500, "network retry was not throttled");
        World replay(out, "network");
        replay.ctx->global("network-ack", see("P", at(P)), tap().retry_while_present(500ms));
        auto exact = task.replay(*replay.ctx, result.trace_directory);
        require(static_cast<bool>(exact) && replay.sim->actions == 0, "transient retry exact replay failed");
    });
    test("loop global retry remains cancellable", [&] {
        World w(out, "network");
        std::stop_source source;
        w.ctx->global("network-ack", see("P", at(P)), tap().retry_while_present(500ms));
        w.sim->after_action = [&](Simulator &s) {
            s.state("network");
            if (s.actions == 3)
                source.request_stop();
        };
        auto result = Phase("network-cancel")
                          .goal(see("D", at(D)))
                          .timeout(10s)
                          .max_frames(2000)
                          .run(*w.ctx, source.get_token());
        require(result.status == Status::cancelled && w.sim->actions == 3,
                "cancel did not stop network retry");
    });
    test("loop global retry rejects resource actions and unsafe intervals", [&] {
        for (bool effect : {false, true}) {
            World w(out, "network");
            auto action = tap().retry_while_present(effect ? 500ms : 10ms);
            if (effect)
                action.effect().expect(see("D", at(D)));
            bool rejected = false;
            try {
                w.ctx->global("invalid-retry", see("P", at(P)), action);
                auto result = w.step().run(*w.ctx);
                rejected = result.status == Status::failed;
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            require(rejected && w.sim->actions == 0, "unsafe transient retry accepted");
        }
    });
    test("loop goal precedes all rules and performs zero actions", [&] {
        World w(out, "done");
        auto r = w.step().run(*w.ctx);
        require(r.status == Status::success, "goal failed");
        require(w.sim->actions == 0, "clicked after goal");
    });
    test("loop one input per frame and global priority with seeded popups", [&] {
        World w(out);
        w.sim->faults.popup_state = "popup";
        w.sim->faults.popup_probability = .12;
        w.sim->faults.popup_on_capture = {1};
        w.sim->faults.seed = 7;
        w.ctx->global("close", see("P", at(P)), tap());
        std::map<uint64_t, int> inputs;
        w.sim->after_action = [&](Simulator &s) {
            require(++inputs[s.captures] == 1, "multiple actions per frame");
        };
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::success, "popup task failed");
        require(w.sim->applied["popup-close"] > 0, "popup not tested");
    });
    test("loop side-effect confirmation timeout never repeats after recovery", [&] {
        World w(out);
        w.sim->after_action = [](Simulator &s) {
            if (s.applied["go"] && s.state() == "done")
                s.state("unknown");
        };
        w.recovery();
        auto p = Phase("effect")
                     .goal(see("D", at(D)))
                     .on("once", see("A", at(A)), tap().effect().expect(see("D", at(D)), 40ms))
                     .timeout(250ms);
        auto r = Task("effect-task").done(see("D", at(D))).phase(p).timeout(1500ms).retries(2).run(*w.ctx);
        require(r.status == Status::outcome_unknown, "outcome not unknown");
        require(w.sim->applied["go"] == 1, "effect repeated");
        require(r.recoveries == 2, "recovery not exercised");
    });
    test("loop global effect and missing expect rejected before device access", [&] {
        World w(out);
        bool threw = false;
        try {
            w.ctx->global("bad", see("A", at(A)), tap().effect().expect(see("D", at(D))));
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "global effect accepted");
        auto r = Phase("bad").goal(see("D", at(D))).on(see("A", at(A)), tap().effect()).run(*w.ctx);
        require(r.status == Status::failed && w.sim->captures == 0, "missing expect reached device");
    });
    test("loop unchanged rule stuck detection triggers bounded recovery", [&] {
        World w(out);
        w.sim->faults.ignored_actions = 100;
        w.recovery();
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::failed && r.recoveries == 2, "stuck recovery missing");
        require(w.sim->attempted["go"] == 15 && w.sim->actions == 15,
                "five attempts per phase/retry not bounded");
    });
    test("loop recovery task restart observes already completed result", [&] {
        World w(out);
        w.recovery();
        w.sim->after_action = [](Simulator &s) {
            if (s.applied["go"] && s.state() == "home")
                s.state("done_home");
        };
        auto p = Phase("effect-different-confirm")
                     .goal(see("E", at(E)))
                     .on("once", see("A", at(A)), tap().effect().expect(see("E", at(E)), 40ms))
                     .timeout(250ms);
        auto r = Task("completed-after-recovery").done(see("D", at(D))).phase(p).timeout(1200ms).run(*w.ctx);
        require(r.status == Status::already_done, "completion check did not skip");
        require(r.recoveries == 1 && w.sim->applied["go"] == 1, "completed effect repeated");
    });
    test("loop phase checkpoints skip completed work after recovery", [&] {
        World w(out);
        w.recovery();
        bool injected = false;
        int skips = 0;
        w.ctx->events([&](const std::string &line) {
            rapidjson::Document d;
            d.Parse(line.c_str());
            std::string type = d["type"].GetString(), phase = d["phase"].GetString();
            if (type == "loop.phase.start" && phase == "second" && !injected) {
                injected = true;
                w.sim->faults.ignored_actions = 100;
            }
            if (type == "loop.recovery")
                w.sim->faults.ignored_actions = 0;
            if (type == "loop.phase.skip")
                ++skips;
        });
        w.sim->after_action = [](Simulator &s) {
            if (s.applied["go"] >= 2 && s.state() == "done")
                s.state("done_e");
        };
        auto second = Phase("second").goal(see("E", at(E))).on(see("A", at(A)), tap()).timeout(300ms);
        auto r = Task("checkpoints")
                     .done(completed("second"))
                     .phase(w.step().checkpoint())
                     .phase(second)
                     .timeout(1500ms)
                     .run(*w.ctx);
        require(r.status == Status::success && r.recoveries == 1 && skips == 1, "checkpoint restart failed");
        require(w.sim->applied["go"] == 2, "first phase repeated");
    });
    test("loop recovery reruns navigation prerequisites by default", [&] {
        World w(out);
        w.recovery();
        bool injected = false;
        int starts = 0, skips = 0;
        w.ctx->events([&](const std::string &line) {
            rapidjson::Document d;
            d.Parse(line.c_str());
            std::string type = d["type"].GetString(), phase = d["phase"].GetString();
            if (type == "loop.phase.start" && phase == "step")
                ++starts;
            if (type == "loop.phase.start" && phase == "second" && !injected) {
                injected = true;
                w.sim->faults.ignored_actions = 100;
            }
            if (type == "loop.recovery")
                w.sim->faults.ignored_actions = 0;
            if (type == "loop.phase.skip")
                ++skips;
        });
        w.sim->after_action = [](Simulator &s) {
            if (s.applied["go"] >= 2 && s.state() == "done")
                s.state("done_e");
        };
        auto second = Phase("second").goal(see("E", at(E))).on(see("A", at(A)), tap()).timeout(300ms);
        auto r = Task("restart-prerequisites")
                     .done(completed("second"))
                     .phase(w.step())
                     .phase(second)
                     .timeout(1500ms)
                     .run(*w.ctx);
        require(r.status == Status::success && r.recoveries == 1, "restart did not succeed");
        require(starts == 2 && skips == 0, "navigation was incorrectly skipped after recovery");
    });
    test("loop cancellation completes within one capture and without input", [&] {
        World w(out);
        std::stop_source stop;
        w.sim->before_capture = [&](Simulator &) { stop.request_stop(); };
        auto r = w.task().run(*w.ctx, stop.get_token());
        require(r.status == Status::cancelled, "not cancelled");
        require(w.sim->captures == 1 && w.sim->actions == 0, "cancellation late");
    });
    test("loop moving background stability wait capped and still recognizes", [&] {
        World w(out);
        w.sim->faults.animate = true;
        auto r = w.step().run(*w.ctx);
        require(r.status == Status::success, "animated screen never recognized");
        require(w.clock->now_ms() < 200, "unbounded stability wait");
        std::ifstream f(r.trace_directory / L"events.jsonl");
        std::string data((std::istreambuf_iterator<char>(f)), {});
        require(data.find("stable_capped\\\":true") != std::string::npos, "cap not recorded");
    });
    test("loop missing ROI rejected at startup before capture", [&] {
        World w(out);
        auto r = Phase("missing-roi").goal(see("D")).on(see("A", at(A)), tap()).run(*w.ctx);
        require(r.status == Status::failed && w.sim->captures == 0, "missing ROI reached device");
        require(r.reason.find("ROI") != std::string::npos, "wrong validation error");
    });
    test("loop full-screen matching requires an explicit reason", [&] {
        World w(out, "done");
        auto invalid = Phase("whole").goal(see("D", roi(0, 0, 320, 180))).run(*w.ctx);
        require(invalid.status == Status::failed && w.sim->captures == 0,
                "whole screen accepted without reason");
        auto allowed = Phase("whole-reason")
                           .goal(see("D", full_screen("Synthetic fixture exercises explicit override")))
                           .run(*w.ctx);
        require(allowed.status == Status::success, "explicit full-screen reason rejected");
    });
    test("loop decision replay is exact, offline and detects recognition changes", [&] {
        World w(out);
        auto task = w.task();
        auto r = task.run(*w.ctx);
        require(r.status == Status::success, "recording failed");
        World replay(out);
        uint32_t replay_decisions = 0;
        auto replayed = task.replay(*replay.ctx, r.trace_directory, [&](const ReplayDecision &d) {
            require(d.equal, "replay rule differs");
            ++replay_decisions;
        });
        require(static_cast<bool>(replayed), replayed ? "" : replayed.error().message.c_str());
        require(replayed.value() == r.frames, "frame count mismatch");
        require(replay_decisions == r.frames, "replay did not expose every frame decision");
        require(replay.sim->captures == 0 && replay.sim->actions == 0, "replay touched device");
        replay.ctx->asset("A", patch(999));
        auto changed = task.replay(*replay.ctx, r.trace_directory);
        require(!changed, "parameter change was hidden");
    });
    test("loop effect outcome and recovery replay preserve the one-shot ledger", [&] {
        World w(out);
        w.recovery();
        w.sim->after_action = [](Simulator &s) {
            if (s.applied["go"] && s.state() == "done")
                s.state("unknown");
        };
        auto p = Phase("effect-replay")
                     .goal(see("D", at(D)))
                     .on("once", see("A", at(A)),
                         tap().effect().expect(see("D", at(D)), 40ms).task_budget("effect-budget", 1))
                     .timeout(250ms);
        auto task = Task("effect-replay-task").done(see("D", at(D))).phase(p).timeout(1500ms);
        auto r = task.run(*w.ctx);
        require(r.status == Status::outcome_unknown, "expected unknown");
        World replay(out);
        replay.recovery();
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(static_cast<bool>(checked), checked ? "" : checked.error().message.c_str());
        require(replay.sim->actions == 0 && replay.sim->captures == 0, "effect replay touched controller");
    });
    test("loop thumbnails reject exact replay rather than comparing altered pixels", [&] {
        World w(out, "home", false);
        auto task = w.task();
        auto r = task.run(*w.ctx);
        World replay(out);
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(!checked && checked.error().code == ar::core::ErrorCode::unsupported,
                "thumbnail replay was silently allowed");
    });
    test("loop pure ROI predicate exceptions trigger normal bounded recovery", [&] {
        World w(out);
        w.recovery();
        auto bad = custom("throws", at(A), [](const cv::Mat &) -> bool {
            throw std::runtime_error("Injected recognition exception");
        });
        auto task =
            Task("throwing").done(see("D", at(D))).phase(Phase("throws").goal(bad)).timeout(500ms).retries(1);
        auto r = task.run(*w.ctx);
        require(r.status == Status::failed && r.recoveries == 1, "custom exception bypassed recovery");
        require(w.sim->actions == 0, "exception caused action");
    });
    test("loop delayed transition and lost input acknowledgement confirmed by fresh frame", [&] {
        World w(out);
        w.sim->faults.delayed_frames = 4;
        w.sim->faults.loading_state = "loading";
        w.sim->faults.lost_acknowledgements = 1;
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::success, "lost acknowledgement failed");
        require(w.sim->applied["go"] == 1, "repeated delayed click");
    });
    test("loop ignored input retries navigation only", [&] {
        World w(out);
        w.sim->faults.ignored_actions = 1;
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::success, "did not retry navigation");
        require(w.sim->attempted["go"] == 2 && w.sim->applied["go"] == 1, "wrong retry count");
    });
    test("loop permanently loading screen terminates within frame and task budgets", [&] {
        World w(out);
        w.sim->faults.stuck_loading = true;
        w.sim->faults.loading_state = "loading";
        w.recovery();
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::failed, "loading did not fail");
        require(w.clock->now_ms() <= 1500 && r.frames <= 100, "loading was unbounded");
    });
    test("loop explicit forbidden modal guard stops even over visible goal", [&] {
        World w(out, "done");
        w.ctx->guard("forbidden", see("A", at(A)));
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::blocked && w.sim->actions == 0, "forbidden modal was bypassed");
    });
    test("loop device mutual exclusion shared with existing runtime", [&] {
        World w(out);
        auto lock = w.session->acquire_run();
        require(static_cast<bool>(lock), "lock failed");
        auto r = w.task().run(*w.ctx);
        require(r.status == Status::failed && w.sim->captures == 0, "concurrent run accepted");
    });
    test("loop B2b old page with loading overlay for 3s clicks exactly once", [&] {
        World w(out);
        w.sim->faults.transition_delay_ms = 3000;
        w.sim->faults.loading_overlay = patch(5);
        w.sim->faults.overlay_origin = E.tl();
        w.ctx->busy(see("E", at(E)));
        auto task = Task("overlay-delay")
                        .done(completed("enter"))
                        .phase(Phase("enter")
                                   .goal(see("D", at(D)))
                                   .on("entry", see("A", at(A)), tap())
                                   .timeout(10s)
                                   .max_frames(2000))
                        .timeout(12s);
        auto r = task.run(*w.ctx);
        require(r.status == Status::success && r.recoveries == 0, "loading overlay caused recovery");
        require(w.sim->attempted["go"] == 1 && w.clock->now_ms() >= 3000,
                "navigation repeated during loading");
        World replay(out);
        replay.ctx->busy(see("E", at(E)));
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(bool(checked), checked ? "" : checked.error().message.c_str());
        require(replay.sim->captures == 0 && replay.sim->actions == 0, "busy replay used device");
    });
    test("loop B2b black loading 6s does not consume unknown time", [&] {
        World w(out);
        w.sim->faults.black_loading_ms = 6000;
        w.ctx->busy(custom("black", full_screen("Synthetic black loading fault"),
                           [](const cv::Mat &m) { return cv::mean(m)[0] < 1; }));
        auto p = Phase("black-load")
                     .goal(see("D", at(D)))
                     .on("entry", see("A", at(A)), tap())
                     .unknown_after(100ms)
                     .timeout(10s)
                     .max_frames(2000);
        auto r = p.run(*w.ctx);
        require(r.status == Status::success && r.recoveries == 0 && w.sim->attempted["go"] == 1,
                "black load treated as unknown");
        require(w.clock->now_ms() >= 6000, "six second fault not injected");
    });
    test("loop B2b delayed confirmed fixed fallback wakes hidden UI once", [&] {
        World w(out);
        w.sim->faults.idle_state = "hidden";
        w.sim->faults.hide_ui_on_capture = 1;
        auto p = Phase("wake")
                     .goal(see("H", at(H)))
                     .on("neutral-wake",
                         custom("safe-blank", roi(270, 140, 20, 20),
                                [](const cv::Mat &m) { return cv::mean(m)[0] < 100; }),
                         tap_at({280, 150}, "Blank synthetic recovery point")
                             .after_unknown(50ms)
                             .expect(see("H", at(H)), 100ms)
                             .max_attempts(2))
                     .unknown_after(200ms)
                     .timeout(1s);
        auto r = p.run(*w.ctx);
        require(r.status == Status::success && w.sim->attempted["wake"] == 1, "wake was repeated or absent");
        require(w.clock->now_ms() >= 50, "wake ignored unknown delay");
    });
    test("loop B2b busy suppresses even global popup actions", [&] {
        World w(out, "busy_popup");
        w.ctx->global("close", see("P", at(P)), tap()).busy(see("E", at(E)));
        auto r = Phase("busy-popup")
                     .goal(see("D", at(D)))
                     .on(see("A", at(A)), tap())
                     .unknown_after(5ms)
                     .timeout(100ms)
                     .run(*w.ctx);
        require(r.status == Status::failed && w.sim->actions == 0, "busy allowed global input");
        require(r.reason.find("unknown") == std::string::npos, "busy consumed unknown budget");
    });
    test("loop B2b goal during busy completes but next phase input waits", [&] {
        World w(out);
        int64_t busy_until = -1;
        bool injected = false;
        uint64_t second_at = 0;
        w.sim->before_capture = [&](Simulator &s) {
            if (s.state() == "done" && !injected) {
                injected = true;
                busy_until = w.clock->now_ms() + 120;
                s.state("done_e");
            }
            if (s.state() == "done_e" && w.clock->now_ms() >= busy_until)
                s.state("done");
        };
        w.sim->after_action = [&](Simulator &s) {
            if (s.applied["back"]) {
                second_at = w.clock->now_ms();
                require(second_at >= busy_until, "next phase acted while busy");
            }
        };
        w.ctx->busy(see("E", at(E)));
        auto r = Task("busy-carry")
                     .done(completed("return"))
                     .phase(Phase("enter").goal(see("D", at(D))).on(see("A", at(A)), tap()))
                     .phase(Phase("return").goal(see("H", at(H))).on(see("A", at(A)), key(4)))
                     .timeout(1s)
                     .run(*w.ctx);
        require(r.status == Status::success && second_at && w.sim->actions == 2,
                "busy boundary navigation failed");
    });
    test("loop B2b phase attempt cap ignores animated image differences", [&] {
        World w(out);
        w.sim->faults.ignored_actions = 100;
        w.sim->faults.animate = true;
        auto r = Phase("cap")
                     .goal(see("D", at(D)))
                     .on("go", see("A", at(A)), tap().settle(40ms))
                     .timeout(1s)
                     .run(*w.ctx);
        require(r.status == Status::failed && w.sim->attempted["go"] == 5,
                "phase cap is not exactly five attempts");
        require(r.reason.find("attempt limit") != std::string::npos, "image differences bypassed cap");
    });
    test("loop B2b default navigation confirmation window is two seconds", [&] {
        World w(out);
        w.sim->faults.ignored_actions = 1;
        auto r = Phase("default-settle")
                     .goal(see("D", at(D)))
                     .on("go", see("A", at(A)), tap())
                     .timeout(5s)
                     .max_frames(2000)
                     .run(*w.ctx);
        require(r.status == Status::success && w.sim->attempted["go"] == 2,
                "navigation timeout did not retry");
        require(w.clock->now_ms() >= 2000 && w.clock->now_ms() < 2300, "default settle is not 2s");
    });
    test("loop B2b globals may close popup during phase settle", [&] {
        World w(out);
        w.sim->faults.ignored_actions = 1;
        w.sim->faults.popup_state = "popup";
        w.sim->faults.popup_on_capture = {4};
        w.ctx->global("close", see("P", at(P)), tap());
        auto r = Phase("settle-popup")
                     .goal(see("D", at(D)))
                     .on("go", see("A", at(A)), tap().settle(120ms))
                     .timeout(1s)
                     .run(*w.ctx);
        require(r.status == Status::success && w.sim->applied["popup-close"] == 1,
                "global was blocked by settle");
        require(w.sim->attempted["go"] == 2, "phase input escaped confirmation window");
    });
    test("loop B2b unknown milliseconds independent of frame rate", [&] {
        for (auto ms : {2u, 20u}) {
            World w(out, "loading");
            w.sim->capture_ms = ms;
            auto r = Phase("unknown-ms").goal(see("D", at(D))).unknown_after(80ms).timeout(1s).run(*w.ctx);
            require(r.status == Status::failed && r.reason.find("unknown-time") != std::string::npos,
                    "unknown timer not reached");
            require(w.clock->now_ms() >= 80 && w.clock->now_ms() < 180, "unknown budget varies with FPS");
        }
    });
    test("loop B2b unsafe fixed fallback rejected before capture", [&] {
        World w(out);
        auto r = Phase("unsafe-point")
                     .goal(see("D", at(D)))
                     .on(see("A", at(A)), tap_at({280, 150}, "test"))
                     .run(*w.ctx);
        require(r.status == Status::failed && w.sim->captures == 0, "unconfirmed fixed point accepted");
    });
    test("loop B2b replay checks new busy and settle metadata", [&] {
        World w(out);
        auto task = w.task();
        auto r = task.run(*w.ctx);
        require(r.status == Status::success, "record failed");
        auto copy = r.trace_directory;
        copy += L"-tampered";
        std::filesystem::copy(r.trace_directory, copy, std::filesystem::copy_options::recursive);
        std::ifstream in(copy / L"events.jsonl");
        std::string s((std::istreambuf_iterator<char>(in)), {});
        in.close();
        auto pos = s.find("\\\"busy\\\":false");
        require(pos != std::string::npos, "busy field absent");
        auto original = s;
        s.replace(pos, 14, "\\\"busy\\\":true");
        std::ofstream file(copy / L"events.jsonl");
        file << s;
        file.close();
        World replay(out);
        require(!task.replay(*replay.ctx, copy), "busy metadata change hidden");
        original.replace(pos, 14, "\\\"busy\\\":\\\"invalid\\\"");
        std::ofstream malformed(copy / L"events.jsonl");
        malformed << original;
        malformed.close();
        require(!task.replay(*replay.ctx, copy), "malformed busy field accepted");
    });
    test("loop B2b busy occlusion cannot end settle through false gone", [&] {
        World w(out);
        int64_t first = -1, second = -1;
        w.sim->after_action = [&](Simulator &s) {
            if (s.applied["go"] == 1) {
                first = w.clock->now_ms();
                s.state("loading");
            } else if (s.applied["go"] == 2) {
                second = w.clock->now_ms();
                s.state("popup");
            }
        };
        w.sim->before_capture = [&](Simulator &s) {
            if (s.applied["go"] == 1)
                s.state(w.clock->now_ms() - first < 120 ? "loading" : "home");
        };
        w.ctx->busy(see("E", at(E)));
        auto r = Phase("busy-occlusion")
                     .goal(see("P", at(P)))
                     .on("entry", see("A", at(A)), tap().settle(250ms))
                     .timeout(1s)
                     .run(*w.ctx);
        require(r.status == Status::success && second - first >= 250,
                "busy overlay falsely ended navigation hold");
        require(w.sim->attempted["go"] == 2, "wrong retry count");
    });
    test("loop B2b navigation gone and expect reasons are recorded", [&] {
        for (bool expect : {false, true}) {
            World w(out);
            bool observed = false;
            w.sim->after_action = [&](Simulator &s) { s.state(expect ? "done_e" : "loading"); };
            w.ctx->events([&](const std::string &line) {
                rapidjson::Document d;
                d.Parse(line.c_str());
                if (std::string(d["type"].GetString()) == "loop.frame" &&
                    std::string(d["data"]["settle_reason"].GetString()) == (expect ? "expect" : "gone")) {
                    observed = true;
                    w.sim->state("popup");
                }
            });
            auto action = tap().settle(250ms);
            if (expect)
                action.expect(see("E", at(E)));
            auto r = Phase("reason")
                         .goal(see("P", at(P)))
                         .on("entry", see("A", at(A)), action)
                         .timeout(1s)
                         .run(*w.ctx);
            require(r.status == Status::success && observed && w.sim->actions == 1,
                    "settle exit reason missing");
        }
    });
    test("loop B2b known button prevents delayed fallback wake", [&] {
        World w(out);
        auto r = Phase("no-wake-on-button")
                     .goal(see("P", at(P)))
                     .on("wake",
                         all(custom("blank", roi(270, 140, 20, 20), [](const cv::Mat &) { return true; }),
                             not_(see("A", at(A)))),
                         tap_at({280, 150}, "Synthetic neutral fallback")
                             .after_unknown(20ms)
                             .expect(see("P", at(P)), 50ms)
                             .max_attempts(2))
                     .unknown_after(80ms)
                     .timeout(200ms)
                     .run(*w.ctx);
        require(r.status == Status::failed && w.sim->actions == 0, "known button allowed fallback wake");
    });
    test("loop B2b counterfactual includes initial task check and uses no device", [&] {
        World w(out);
        auto task = w.task();
        auto recorded = task.run(*w.ctx);
        require(recorded.status == Status::success, "record failed");
        uint64_t recorded_action = 0, projected_action = 0;
        std::ifstream file(recorded.trace_directory / L"events.jsonl");
        std::string line;
        while (std::getline(file, line)) {
            rapidjson::Document outer;
            outer.Parse(line.c_str());
            if (std::string(outer["type"].GetString()) != "loop.frame")
                continue;
            rapidjson::Document d;
            d.Parse(outer["details"].GetString());
            if (d.HasMember("action"))
                recorded_action = outer["frame_id"].GetUint64();
        }
        World projected(out);
        auto result = task.project(*projected.ctx, recorded.trace_directory, [&](const std::string &row) {
            rapidjson::Document d;
            d.Parse(row.c_str());
            if (d["action"].GetBool())
                projected_action = d["frame_id"].GetUint64();
        });
        require(result && result.value().complete && result.value().actions == 1, "counterfactual failed");
        require(recorded_action == projected_action && recorded_action != 0,
                "projection skipped initial task check");
        require(projected.sim->captures == 0 && projected.sim->actions == 0, "projection used device");
    });
    test("loop task action budget persists across phases and stops before third input", [&] {
        World w(out);
        w.recovery();
        auto task = Task("shared-navigation-budget")
                        .done(completed("third"))
                        .phase(Phase("first")
                                   .goal(see("D", at(D)))
                                   .on("go", see("A", at(A)), tap().task_budget("shared", 2)))
                        .phase(Phase("second")
                                   .goal(see("H", at(H)))
                                   .on("back", see("A", at(A)), key(4).task_budget("shared", 2)))
                        .phase(Phase("third")
                                   .goal(see("D", at(D)))
                                   .on("go", see("A", at(A)), tap().task_budget("shared", 2)))
                        .timeout(2s);
        auto r = task.run(*w.ctx);
        require(r.status == Status::blocked && r.actions == 2 && r.recoveries == 0,
                "task budget reset between phases/recovery");
        World replay(out);
        replay.recovery();
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(bool(checked), checked ? "" : checked.error().message.c_str());
        require(replay.sim->captures == 0 && replay.sim->actions == 0, "budget replay used device");
    });
    test("loop unresponsive wake task budget cannot double through recovery", [&] {
        World w(out, "hidden");
        w.sim->faults.ignored_actions = 100;
        auto phase = [](std::string name) {
            return Phase(name)
                .goal(see("H", at(H)))
                .on("wake", custom("blank", roi(270, 140, 20, 20), [](const cv::Mat &) { return true; }),
                    tap_at({280, 150}, "Synthetic wake")
                        .after_unknown(20ms)
                        .settle(30ms)
                        .expect(see("H", at(H)), 30ms)
                        .max_attempts(2)
                        .task_budget("wake-task", 2))
                .unknown_after(150ms)
                .timeout(500ms);
        };
        w.ctx->recover(phase("recover"));
        auto task = Task("unresponsive-wake").done(completed("home")).phase(phase("home")).timeout(2s);
        auto r = task.run(*w.ctx);
        require(r.status == Status::blocked && r.actions == 2 && r.recoveries == 0 &&
                    w.sim->attempted["wake"] == 2,
                "wake retried in recovery after exhausting task budget");
        World replay(out, "hidden");
        replay.ctx->recover(phase("recover"));
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(bool(checked), checked ? "" : checked.error().message.c_str());
    });
    test("loop B2c strict navigation stops after missing confirmation even when trigger disappears", [&] {
        World w(out);
        w.recovery();
        w.sim->after_action = [](Simulator &s) { s.state("loading"); };
        auto task =
            Task("strict-navigation")
                .done(completed("strict"))
                .phase(Phase("strict")
                           .goal(see("P", at(P)))
                           .on("exit", see("A", at(A)),
                               tap().expect(see("P", at(P)), 50ms).settle(50ms).require_confirmation()))
                .timeout(1s);
        auto r = task.run(*w.ctx);
        require(r.status == Status::blocked && r.actions == 1 && r.recoveries == 0 &&
                    r.reason == "$nav.unconfirmed/exit",
                "strict exit retried or recovered");
        World replay(out);
        replay.recovery();
        auto checked = task.replay(*replay.ctx, r.trace_directory);
        require(bool(checked) && replay.sim->captures == 0 && replay.sim->actions == 0,
                "strict failure replay mismatch");
        auto projected = task.project(*replay.ctx, r.trace_directory);
        require(projected && !projected.value().complete && projected.value().actions == 1 &&
                    projected.value().reason.find("$nav.unconfirmed/exit") != std::string::npos,
                "strict failure projection ignored stop");
    });
    test("loop B2c strict navigation accepts visible success after lost acknowledgement", [&] {
        World w(out);
        w.sim->faults.lost_acknowledgements = 1;
        auto r = Phase("strict-success")
                     .goal(see("D", at(D)))
                     .on("go", see("A", at(A)),
                         tap().expect(see("D", at(D)), 50ms).settle(50ms).require_confirmation())
                     .run(*w.ctx);
        require(r.status == Status::success && w.sim->actions == 1, "visible success lost after lost ack");
    });
    test("loop B2c timed simulator state expires and reentry restarts its lifetime", [&] {
        World w(out, "timed");
        require(w.sim->connect(), "sim connect failed");
        w.clock->sleep_ms(2900, {});
        w.sim->screenshot();
        require(w.sim->state() == "timed", "button expired early");
        w.clock->sleep_ms(100, {});
        w.sim->screenshot();
        require(w.sim->state() == "hidden", "button did not expire after three seconds");
        w.sim->state("timed");
        w.clock->sleep_ms(2000, {});
        w.sim->state("timed");
        w.clock->sleep_ms(1500, {});
        w.sim->screenshot();
        require(w.sim->state() == "timed", "reentry did not restart button lifetime");
        w.clock->sleep_ms(1500, {});
        w.sim->screenshot();
        require(w.sim->state() == "hidden", "reset button never expired");
    });
    test("loop real native OCR crop and semantic numeric condition", [&] {
        auto image = ar::core::read_image(root / L"tests/ocr_corpus/ja/regression/home.png");
        require(static_cast<bool>(image), "missing OCR fixture");
        auto clock = std::make_shared<ManualClock>();
        auto sim = std::make_shared<Simulator>(std::map<std::string, SimState>{{"home", {image.value(), {}}}},
                                               "home", clock);
        ar::runtime::DeviceSession session(sim, {"loop-ocr", {1280, 720}}, clock);
        Options o;
        o.trace_root = out;
        o.stable_max = 0ms;
        Context ctx(session, out, "test", o);
        ar::vision::OcrParams params;
        params.profile = ar::core::wide_to_utf8((root / L"models/ocr/ja-b1d-v2.json").wstring());
        ctx.ocr(params);
        auto r = Phase("numeric").goal(ocr_number("789333348", roi(697, 23, 106, 25))).run(ctx);
        require(r.status == Status::success && sim->actions == 0, "numeric crop condition failed");
        auto mismatch = Phase("numeric-operator-mismatch")
                            .goal(ocr_number("789333348/789333348", roi(697, 23, 106, 25)))
                            .max_frames(1)
                            .timeout(30ms)
                            .run(ctx);
        require(mismatch.status == Status::failed && sim->actions == 0,
                "missing fraction operator was silently repaired");
        std::ifstream file(mismatch.trace_directory / L"events.jsonl");
        std::string trace((std::istreambuf_iterator<char>(file)), {});
        require(trace.find("raw_ocr") != std::string::npos && trace.find("789,333,348") != std::string::npos,
                "unmatched OCR output was not recorded");
    });
}
