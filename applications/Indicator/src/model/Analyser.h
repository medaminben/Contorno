#ifndef INDICATOR_ANALYSER_H
#define INDICATOR_ANALYSER_H
#include "model/Message.h"

#include <Vision/Objects.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

/// The outcome of one analysis.
struct AnalysisDone {
    std::uint64_t id {0};
    /// Null when the analysis failed; then `error` says why.
    std::shared_ptr<Vision::ObjectsAnalysis const> analysis;
    Message error;
    double seconds {0};
};

/// Runs find_objects in a worker thread, one analysis at a time. A request
/// made while one runs waits; a newer request replaces a waiting one, and
/// the result of a run that a waiting request has made stale is dropped.
/// `on_done` runs inside a task handed to `Post`, which must run it on the
/// thread that owns the Analyser (the GUI thread); a task that arrives after
/// the Analyser is destroyed does nothing. Every member but the worker's own
/// body is for that one thread.
class Analyser {
public:
    /// Runs a task on the GUI thread. Called from the worker thread. The task must run
    /// asynchronously, never inside the call: it joins the worker that is calling.
    using Post = std::function<void(std::function<void()>)>;

    /// The outline tolerance of every analysis (also written into an inspection's parameters).
    static constexpr double outline_tolerance_mm = 0.5;

    Analyser(Post post, std::function<void(AnalysisDone const&)> on_done);
    /// Drops the waiting request and joins the worker. A run in progress cannot
    /// be interrupted, so this waits for it; its result is dropped.
    ~Analyser();
    Analyser(Analyser const&) = delete;
    Analyser& operator=(Analyser const&) = delete;

    /// The request's number (increasing).
    /// `min_area_mm2` becomes a percentage of the photo (read with the EXIF
    /// rotation applied); `model_path` is the IS-Net file.
    std::uint64_t request(std::filesystem::path const& photo, Vision::LensCamera const& camera,
                          double min_area_mm2, std::filesystem::path const& model_path);
    /// The options of the last run started (for tests).
    Vision::ObjectOptions last_options() const;
    /// The scale (mm per pixel) the last started analysis ran with.
    Vision::PixelScale last_scale() const;
    /// A run is going or waiting.
    bool busy() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};

#endif  // INDICATOR_ANALYSER_H
