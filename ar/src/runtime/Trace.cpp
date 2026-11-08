#include "runtime/Trace.h"
#include "core/Str.hpp"
#include "core/IO.h"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
namespace ar::runtime {
ar::core::Result<std::string> TraceWriter::record_frame(const Frame &frame,bool full,bool failure) {
    try {
        auto relative=std::filesystem::path(failure?L"failures":L"frames") /
            (std::to_wstring(frame.id)+(full||failure?L".png":L".thumb.png"));
        std::filesystem::create_directories((directory_/relative).parent_path());
        cv::Mat image=frame.image;
        if (!full && !failure && image.cols>640) cv::resize(image,image,{640,static_cast<int>(640.0*image.rows/image.cols)});
        auto saved=ar::core::write_image(directory_/relative,image);
        if (!saved) return saved.error();
        if (failure && !frame.original.empty()) {
            auto original=ar::core::write_image(directory_/L"failures"/(std::to_wstring(frame.id)+L".original.png"),frame.original);
            if (!original) return original.error();
        }
        return relative.generic_string();
    } catch(const std::exception &e) {
        return ar::core::failure(ar::core::ErrorCode::io_failed,"trace.frame",e.what());
    }
}
TraceWriter::TraceWriter(std::filesystem::path directory) : directory_(std::move(directory)) {
    try {
        std::filesystem::create_directories(directory_);
        file_.open(directory_ / L"events.jsonl", std::ios::binary);
        if (!file_)
            error_ = ar::core::failure(ar::core::ErrorCode::io_failed, "trace", "Cannot open event trace");
    } catch (const std::exception &e) {
        error_ = ar::core::failure(ar::core::ErrorCode::io_failed, "trace", e.what());
    }
}
void TraceWriter::emit(const Event &event) noexcept {
    if (error_)
        return;
    try {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        w.StartObject();
        auto text = [&](const char *key, const std::string &value) {
            w.Key(key);
            w.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
        };
        text("run_id", event.run_id);
        text("device_id", event.device_id);
        text("resource", event.resource);
        text("version", event.version);
        text("node", event.node);
        text("type", event.type);
        text("details", event.details);
        w.Key("frame_id");
        w.Uint64(event.frame_id);
        w.Key("time_ms");
        w.Int64(event.time_ms);
        if (event.match) {
            w.Key("recognition");
            w.StartObject();
            w.Key("found");
            w.Bool(event.match->found);
            w.Key("score");
            w.Double(event.match->score);
            text("backend", event.match->backend);
            text("text", event.match->text);
            w.Key("box");
            w.StartArray();
            for (int v :
                 {event.match->box.x, event.match->box.y, event.match->box.width, event.match->box.height})
                w.Int(v);
            w.EndArray();
            w.EndObject();
        }
        if (event.error) {
            w.Key("error");
            w.StartObject();
            text("code", ar::core::error_name(event.error->code));
            text("operation", event.error->operation);
            text("message", event.error->message);
            text("backend", event.error->backend);
            w.Key("native_code");
            w.Int64(event.error->native_code);
            w.EndObject();
        }
        w.EndObject();
        file_.write(buffer.GetString(), buffer.GetSize());
        file_.put('\n');
        file_.flush();
        if (!file_)
            error_ = ar::core::failure(ar::core::ErrorCode::io_failed, "trace", "Writing event trace failed");
    } catch (const std::exception &e) {
        error_ = ar::core::failure(ar::core::ErrorCode::io_failed, "trace", e.what());
    } catch (...) {
    }
}
ar::core::Result<void> TraceWriter::snapshot(const Frame &frame,
                                             const std::optional<RecognitionMatch> &match) {
    try {
        auto overlay = frame.image.clone();
        if (match && !match->box.empty())
            cv::rectangle(overlay, match->box, cv::Scalar(0, 0, 255), 2);
        auto canonical = ar::core::write_image(directory_ / L"failure.png", overlay);
        if (!canonical)
            return canonical;
        return ar::core::write_image(directory_ / L"failure-original.png", frame.original);
    } catch (const std::exception &e) {
        return ar::core::failure(ar::core::ErrorCode::io_failed, "snapshot", e.what());
    }
}
} // namespace ar::runtime
