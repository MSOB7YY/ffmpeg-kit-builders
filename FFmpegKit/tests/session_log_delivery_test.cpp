// Regression tests for how session logs reach their sessions:
//  - a drain waiter must not miss the wakeup and sleep for its whole timeout
//  - a session that left the history keeps the logs still queued for it
//  - a session's -loglevel applies to that session only
#include "ffmpegkit_wrapper.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <thread>
#endif

namespace {

#ifdef _WIN32
class TestThread {
public:
  template <typename Callable> explicit TestThread(Callable &&callable) {
    using Task = std::function<void()>;
    auto *task = new Task(std::forward<Callable>(callable));
    unsigned thread_id = 0;
    handle_ = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, &TestThread::run, task, 0, &thread_id));
  }

  TestThread(const TestThread &) = delete;
  TestThread &operator=(const TestThread &) = delete;
  TestThread(TestThread &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  ~TestThread() { join(); }

  void join() {
    if (!handle_) {
      return;
    }
    WaitForSingleObject(handle_, INFINITE);
    CloseHandle(handle_);
    handle_ = nullptr;
  }

private:
  static unsigned __stdcall run(void *arg) {
    using Task = std::function<void()>;
    std::unique_ptr<Task> task(static_cast<Task *>(arg));
    (*task)();
    return 0;
  }

  HANDLE handle_{nullptr};
};
#else
using TestThread = std::thread;
#endif

const char *const kQuietProbeArgs[] = {
    "-v", "quiet", "-show_entries", "stream=width", "-of",
    "default=noprint_wrappers=1", "-f", "lavfi", "-i",
    "testsrc=size=137x79:rate=1:duration=1"};
const char *const kInfoProbeArgs[] = {
    "-v", "info", "-show_entries", "stream=width", "-of",
    "default=noprint_wrappers=1", "-f", "lavfi", "-i",
    "testsrc=size=137x79:rate=1:duration=1"};
constexpr int kProbeArgc = 10;
const char *const kProbeOutput = "width=137";

struct ProbeResult {
  int64_t returnCode = -1;
  std::string output;
  std::chrono::milliseconds elapsed{0};
};

ProbeResult runProbe(const char *const *args) {
  ProbeResult result;
  const auto start = std::chrono::steady_clock::now();
  FFprobeSessionHandle session =
      ffprobe_kit_create_session_from_argv(kProbeArgc, const_cast<const char **>(args));
  if (session == nullptr) {
    return result;
  }
  ffprobe_kit_session_execute(session);
  result.returnCode = ffmpeg_kit_session_get_return_code(session);
  char *output = ffmpeg_kit_session_get_output(session);
  result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  if (output != nullptr) {
    result.output = output;
    ffmpeg_kit_free(output);
  }
  ffmpeg_kit_handle_release(session);
  return result;
}

class SessionLogDeliveryTest : public ::testing::Test {
protected:
  void SetUp() override {
    previousHistorySize_ = ffmpeg_kit_get_session_history_size();
  }

  void TearDown() override {
    ffmpeg_kit_set_session_history_size(previousHistorySize_);
    ffmpeg_kit_config_clear_sessions();
  }

private:
  int64_t previousHistorySize_ = 10;
};

} // namespace

// Before the fix, the drain notify could land between a waiter's check and its
// wait, so getting a session's output took the full 5 second drain timeout
// every so often.
TEST_F(SessionLogDeliveryTest, SequentialSessionsNeverWaitForTheDrainTimeout) {
  const int iterations = 200;
  for (int i = 0; i < iterations; ++i) {
    const ProbeResult result = runProbe(kQuietProbeArgs);
    ASSERT_EQ(result.returnCode, 0) << "iteration " << i;
    ASSERT_NE(result.output.find(kProbeOutput), std::string::npos)
        << "iteration " << i << " output: " << result.output;
    ASSERT_LT(result.elapsed.count(), 2000)
        << "iteration " << i << " waited for the drain timeout";
  }
}

// Before the fix, logs were matched to sessions through the history when the
// callback thread processed them, so a session that had already been evicted
// lost the output still queued for it.
TEST_F(SessionLogDeliveryTest, EvictedSessionsKeepTheirQueuedOutput) {
  ffmpeg_kit_set_session_history_size(1);
  const int threadCount = 8;
  const int iterationsPerThread = 15;
  std::mutex resultsMutex;
  std::vector<ProbeResult> results;
  {
    std::vector<TestThread> threads;
    for (int t = 0; t < threadCount; ++t) {
      threads.emplace_back([&] {
        for (int i = 0; i < iterationsPerThread; ++i) {
          ProbeResult result = runProbe(kQuietProbeArgs);
          std::lock_guard<std::mutex> lock(resultsMutex);
          results.push_back(std::move(result));
        }
      });
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }

  ASSERT_EQ(results.size(), static_cast<size_t>(threadCount * iterationsPerThread));
  for (const ProbeResult &result : results) {
    EXPECT_EQ(result.returnCode, 0);
    EXPECT_NE(result.output.find(kProbeOutput), std::string::npos)
        << "output: '" << result.output << "'";
  }
}

TEST_F(SessionLogDeliveryTest, LogRedirectionStrategyRoundTrips) {
  const FFmpegKitLogRedirectionStrategy previous =
      ffmpeg_kit_config_get_log_redirection_strategy();
  ffmpeg_kit_config_set_log_redirection_strategy(
      FFMPEG_KIT_LOG_REDIRECTION_STRATEGY_NEVER_PRINT_LOGS);
  EXPECT_EQ(ffmpeg_kit_config_get_log_redirection_strategy(),
            FFMPEG_KIT_LOG_REDIRECTION_STRATEGY_NEVER_PRINT_LOGS);

  // the output still reaches the session, only the console printing stops
  const ProbeResult result = runProbe(kQuietProbeArgs);
  EXPECT_EQ(result.returnCode, 0);
  EXPECT_NE(result.output.find(kProbeOutput), std::string::npos);

  ffmpeg_kit_config_set_log_redirection_strategy(previous);
  EXPECT_EQ(ffmpeg_kit_config_get_log_redirection_strategy(), previous);
}

// Before the fix, -loglevel changed av_log's process wide level, so a quiet
// session got another session's info logs (banner, input dump) in its output
// and an info session could lose its own.
TEST_F(SessionLogDeliveryTest, LogLevelOptionOnlyAppliesToItsSession) {
  ffmpeg_kit_config_set_log_level(FFMPEG_KIT_LOG_LEVEL_INFO);
  const int threadsPerLevel = 4;
  const int iterationsPerThread = 15;
  std::mutex resultsMutex;
  std::vector<ProbeResult> quietResults;
  std::vector<ProbeResult> infoResults;
  {
    std::vector<TestThread> threads;
    for (int t = 0; t < threadsPerLevel * 2; ++t) {
      const bool quiet = (t % 2) == 0;
      threads.emplace_back([&, quiet] {
        for (int i = 0; i < iterationsPerThread; ++i) {
          ProbeResult result = runProbe(quiet ? kQuietProbeArgs : kInfoProbeArgs);
          std::lock_guard<std::mutex> lock(resultsMutex);
          (quiet ? quietResults : infoResults).push_back(std::move(result));
        }
      });
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }

  for (const ProbeResult &result : quietResults) {
    EXPECT_EQ(result.returnCode, 0);
    EXPECT_NE(result.output.find(kProbeOutput), std::string::npos);
    EXPECT_EQ(result.output.find("Input #0"), std::string::npos)
        << "quiet session got info logs: " << result.output;
    EXPECT_EQ(result.output.find("ffprobe version"), std::string::npos)
        << "quiet session got the banner: " << result.output;
  }
  for (const ProbeResult &result : infoResults) {
    EXPECT_EQ(result.returnCode, 0);
    EXPECT_NE(result.output.find(kProbeOutput), std::string::npos);
    EXPECT_NE(result.output.find("Input #0"), std::string::npos)
        << "info session lost its own logs: " << result.output;
  }
}
