#include "model/Analyser.h"

#include "model/ImageSize.h"
#include "model/LibraryText.h"
#include "model/Utf8Path.h"

#include <chrono>
#include <optional>
#include <thread>
#include <utility>

namespace {

struct Job {
    std::uint64_t id {0};
    std::filesystem::path photo;
    Vision::LensCamera camera;
    double min_area_mm2 {0};
    std::filesystem::path model_path;
};

}  // namespace

/// Everything the Analyser owns. The worker never touches it: it holds a
/// weak_ptr in the task it posts, so that task finds nothing after the
/// Analyser is gone.
struct Analyser::State : std::enable_shared_from_this<Analyser::State> {
    Post post;
    std::function<void(AnalysisDone const&)> on_done;
    std::unique_ptr<std::jthread> worker;
    std::optional<Job> waiting;
    std::uint64_t last_id {0};
    Vision::ObjectOptions last_options {};
    Vision::PixelScale last_scale {};

    void start(Job job);
    /// On the GUI thread; `error` is the library's problem when the analysis failed, made a message here.
    void finished(std::uint64_t id, std::shared_ptr<Vision::ObjectsAnalysis const> analysis,
                  std::optional<Core::Problem> error, double seconds);
};

void Analyser::State::start(Job job) {
    // outline tolerance fixed (outline_tolerance_mm), threshold at the default; the minimum
    // area in mm2 becomes a percentage of the photo as Vision reads it, with
    // its EXIF rotation (header read only)
    auto const size = shown_size(job.photo);
    bool const rotated = shown_rotated(job.photo);
    auto const scale = Vision::lens_scale(job.camera, rotated);
    Vision::ObjectOptions options {utf8(job.model_path), outline_tolerance_mm};
    if (size) options.min_area_pct = Vision::min_area_pct(job.min_area_mm2, size->width, size->height, scale);
    last_options = options;
    last_scale = scale;
    worker = std::make_unique<std::jthread>(
        [weak = weak_from_this(), post = post, job = std::move(job), scale, options] {
            auto const begin = std::chrono::steady_clock::now();
            auto result = Vision::find_objects(utf8(job.photo), scale, options);
            double const seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
            std::shared_ptr<Vision::ObjectsAnalysis const> analysis;
            std::optional<Core::Problem> error;
            if (result.isError()) error = std::move(result).error();
            else analysis = std::make_shared<Vision::ObjectsAnalysis const>(std::move(result)());
            post([weak, id = job.id, analysis, error, seconds] {
                if (auto state = weak.lock()) state->finished(id, analysis, error, seconds);
            });
        });
}

void Analyser::State::finished(std::uint64_t id, std::shared_ptr<Vision::ObjectsAnalysis const> analysis,
                               std::optional<Core::Problem> error, double seconds) {
    worker->join();
    worker.reset();
    if (waiting) {
        // a newer request makes this result stale
        Job next = std::move(*waiting);
        waiting.reset();
        start(std::move(next));
        return;
    }
    AnalysisDone done;
    done.id = id;
    done.seconds = seconds;
    if (!analysis && error) done.error = from_library(*error);
    done.analysis = std::move(analysis);
    on_done(done);
}

Analyser::Analyser(Post post, std::function<void(AnalysisDone const&)> on_done)
    : state_(std::make_shared<State>()) {
    state_->post = std::move(post);
    state_->on_done = std::move(on_done);
}

Analyser::~Analyser() {
    state_->waiting.reset();
    if (state_->worker) state_->worker->join();
}

std::uint64_t Analyser::request(std::filesystem::path const& photo, Vision::LensCamera const& camera,
                                double min_area_mm2, std::filesystem::path const& model_path) {
    Job job {++state_->last_id, photo, camera, min_area_mm2, model_path};
    if (state_->worker) {
        state_->waiting = std::move(job);
    } else {
        state_->start(std::move(job));
    }
    return state_->last_id;
}

Vision::ObjectOptions Analyser::last_options() const { return state_->last_options; }

Vision::PixelScale Analyser::last_scale() const { return state_->last_scale; }

bool Analyser::busy() const { return state_->worker != nullptr; }
