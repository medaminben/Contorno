#include <gtest/gtest.h>

#include "TempDir.h"
#include "TestImage.h"
#include "model/Analyser.h"
#include "model/Utf8Path.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace {

std::filesystem::path const calf =
    std::filesystem::path(INDICATOR_SAMPLES_DIR) / "leather" / "calf_black.jpg";

/// Stands in for the GUI thread: the analyser's tasks are queued here from the
/// worker thread and run by whoever drains the queue (the test thread).
class TaskQueue {
public:
    Analyser::Post post() {
        return [this](std::function<void()> task) {
            {
                std::lock_guard lock(mutex_);
                tasks_.push_back(std::move(task));
            }
            ready_.notify_all();
        };
    }
    /// Runs queued tasks until `until` is true or `timeout` has passed.
    bool drain(std::function<bool()> const& until, std::chrono::milliseconds timeout) {
        auto const end = std::chrono::steady_clock::now() + timeout;
        while (!until()) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                if (!ready_.wait_until(lock, end, [this] { return !tasks_.empty(); }))
                    return until();
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
        return true;
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> tasks_;
};

/// The analyser's results, collected as they arrive.
struct Done {
    int count {0};
    std::shared_ptr<Vision::ObjectsAnalysis const> analysis;
    Message error;
    std::uint64_t id {0};

    std::function<void(AnalysisDone const&)> sink() {
        return [this](AnalysisDone const& done) {
            ++count;
            id = done.id;
            analysis = done.analysis;
            error = done.error;
        };
    }
    /// Waits for `count` to reach `n`.
    bool wait(TaskQueue& queue, int n, std::chrono::milliseconds timeout) {
        return queue.drain([this, n] { return count >= n; }, timeout);
    }
};

constexpr std::chrono::milliseconds limit {120000};

}  // namespace

TEST(WxAnalyser, MinAreaBecomesPercent) {
    TaskQueue queue;
    Done spy;
    Analyser analyser(queue.post(), spy.sink());
    Vision::LensCamera const lens;
    analyser.request(calf, lens, 100, {});
    ASSERT_TRUE(spy.wait(queue, 1, limit));
    EXPECT_FALSE(spy.error.key().empty());
    auto const s = Vision::lens_scale(lens);
    auto const options = analyser.last_options();
    EXPECT_NEAR(options.min_area_pct, Vision::min_area_pct(100, 1334, 1602, s), 1e-12);
    EXPECT_EQ(options.outline_tolerance_mm, 0.5);
    EXPECT_EQ(options.outline_tolerance_mm, Analyser::outline_tolerance_mm);
    EXPECT_FALSE(analyser.busy());
}

// a phone photo stored on its side: the percentage is of the photo as shown,
// whose width Vision's scale comes from
TEST(WxAnalyser, RotatedPhotoUsesShownWidth) {
    TempDir dir;
    auto const photo = dir.path / "rotated.jpg";
    ASSERT_TRUE(write_rotated_jpeg(photo));  // 40 x 20 stored, 20 x 40 shown
    TaskQueue queue;
    Done spy;
    Analyser analyser(queue.post(), spy.sink());
    Vision::LensCamera const lens;
    analyser.request(photo, lens, 100, {});
    ASSERT_TRUE(spy.wait(queue, 1, limit));
    auto const s = Vision::lens_scale(lens, true);
    EXPECT_NEAR(analyser.last_options().min_area_pct, Vision::min_area_pct(100, 20, 40, s), 1e-12);
}

TEST(WxAnalyser, ScaleAspectFollowsRotation) {
    TempDir dir;
    auto const rotated = dir.path / "rotated.jpg";
    ASSERT_TRUE(write_rotated_jpeg(rotated));
    auto const plain = dir.path / "plain.jpg";
    ASSERT_TRUE(write_plain_jpeg(plain));
    Vision::LensCamera const lens {1500, 13.02, 3.76, 4.20};
    TaskQueue queue;
    Done spy;
    Analyser analyser(queue.post(), spy.sink());
    analyser.request(rotated, lens, 100, {});
    ASSERT_TRUE(spy.wait(queue, 1, limit));
    EXPECT_NEAR(analyser.last_scale().y / analyser.last_scale().x, 3.76 / 4.20, 1e-12);
    analyser.request(plain, lens, 100, {});
    ASSERT_TRUE(spy.wait(queue, 2, limit));
    EXPECT_NEAR(analyser.last_scale().y / analyser.last_scale().x, 4.20 / 3.76, 1e-12);
}

TEST(WxAnalyser, FindsTheCalf) {
    if (!Vision::ai_available() || !std::filesystem::exists(INDICATOR_DEFAULT_MODEL))
        GTEST_SKIP();
    TaskQueue queue;
    Done spy;
    Analyser analyser(queue.post(), spy.sink());
    analyser.request(calf, Vision::LensCamera {}, 100, path_from_utf8(INDICATOR_DEFAULT_MODEL));
    ASSERT_TRUE(spy.wait(queue, 1, std::chrono::milliseconds(300000)));
    auto const& analysis = spy.analysis;
    ASSERT_TRUE(analysis) << spy.error.english();
    EXPECT_EQ(analysis->objects.size(), 1u);
    EXPECT_GT(analysis->area_mm2, 0);
    EXPECT_EQ(analysis->mask.width, 1334);
    EXPECT_EQ(analysis->mask.height, 1602);
}

// the destructor stops waiting work and joins the worker; the task the worker
// posted afterwards arrives at an Analyser that is gone and does nothing
TEST(WxAnalyser, DestroyedWhileRunningDropsResult) {
    TaskQueue queue;
    Done spy;
    {
        Analyser analyser(queue.post(), spy.sink());
        analyser.request(calf, Vision::LensCamera {}, 100, {});
        analyser.request(calf, Vision::LensCamera {}, 100, {});  // waits; dropped with the analyser
    }
    EXPECT_FALSE(queue.drain([&spy] { return spy.count > 0; }, std::chrono::milliseconds(2000)));
    EXPECT_EQ(spy.count, 0);
}
