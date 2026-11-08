#pragma once
#include <fstream>
#include "runtime/Tasker.h"
namespace ar::runtime {
class TraceWriter final : public IEventSink {
  public:
    explicit TraceWriter(std::filesystem::path directory);
    void emit(const Event &event) noexcept override;
    ar::core::Result<void> snapshot(const Frame &frame, const std::optional<RecognitionMatch> &match);
    // Additive frame recording; existing JSON pipeline snapshot semantics unchanged.
    ar::core::Result<std::string> record_frame(const Frame &frame, bool full_resolution=false,
                                               bool failure=false);
    const std::optional<ar::core::Error> &error() const { return error_; }

  private:
    std::filesystem::path directory_;
    std::ofstream file_;
    std::optional<ar::core::Error> error_;
};
} // namespace ar::runtime
