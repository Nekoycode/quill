#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <zest/detail/blocking_queue.h>
#include <zest/detail/deferred_message.h>
#include <zest/logger.h>
#include <zest/sink.h>

namespace zest {

// Message exchanged between the async frontend and the backend writer threads.
struct async_msg {
  log_msg msg;                       // metadata (+ payload when not deferred)
  detail::deferred_message deferred; // set when formatting is deferred
};

// What the async frontend does when the bounded queue is full.
enum class overflow_policy {
  block,       // producer waits for space (never drops records) — default
  drop_oldest, // evict the oldest pending record to make room for the new one
  drop_newest, // discard the incoming record
};

// A logger whose sink writes are performed on a pool of background threads.
// The frontend only captures the message (metadata + arguments, allocation-free
// for the common case); the backend threads format the `%v` text and apply each
// sink's formatter before writing.
//
// With `backend_threads > 1`, records are no longer strictly FIFO ordered, but
// the frontend stays off the I/O path and aggregate throughput scales with the
// pool size. Shutdown requests a stop and drains all pending messages.
class async_logger final : public logger {
public:
  explicit async_logger(std::string name, std::vector<std::shared_ptr<sinks::sink>> sinks,
                        std::size_t queue_size = 4096, std::size_t backend_threads = 1,
                        overflow_policy policy = overflow_policy::block)
      : logger(std::move(name), std::move(sinks), /*defers_formatting=*/true), queue_(queue_size),
        policy_(policy) {
    backend_threads_.reserve(backend_threads);
    for (std::size_t i = 0; i < backend_threads; ++i) {
      backend_threads_.emplace_back([this](std::stop_token st) { backend_loop(st); });
    }
  }

  ~async_logger() override {
    for (auto& t : backend_threads_) {
      t.request_stop();
    }
    queue_.notify_all();
  }

  void flush() override {
    // Wait until every enqueued record has been written to its sink, then flush.
    // (A pending-record counter makes this correct across multiple backends.)
    std::size_t spins = 0;
    while (pending_.load(std::memory_order_acquire) != 0) {
      if (++spins < 1000) {
        std::this_thread::yield();
      } else {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }
    for (auto& s : sinks_) {
      s->flush();
    }
  }

protected:
  void log_meta(log_msg&& meta, detail::deferred_message&& deferred) override {
    async_msg m{std::move(meta), std::move(deferred)};
    switch (policy_) {
    case overflow_policy::block:
      // Count before enqueue so flush() never returns while a record is in flight.
      pending_.fetch_add(1, std::memory_order_relaxed);
      queue_.enqueue(std::move(m));
      break;
    case overflow_policy::drop_newest:
      // Optimistically count, then undo if the record was dropped — counting
      // first keeps flush() on the safe (wait-longer) side and avoids a
      // transient negative count if a backend drains it concurrently.
      pending_.fetch_add(1, std::memory_order_relaxed);
      if (!queue_.try_enqueue_drop_newest(std::move(m))) {
        pending_.fetch_sub(1, std::memory_order_relaxed); // dropped: never pending
      }
      break;
    case overflow_policy::drop_oldest: {
      pending_.fetch_add(1, std::memory_order_relaxed);
      const std::size_t evicted = queue_.enqueue_drop_oldest(std::move(m));
      if (evicted != 0) {
        // Evicted records were counted when enqueued but will never be
        // processed, so the backend's decrement in process() never comes.
        pending_.fetch_sub(evicted, std::memory_order_relaxed);
      }
      break;
    }
    }
  }

private:
  void process(async_msg& m) {
    if (!m.deferred.empty()) {
      std::string text;
      m.deferred.format_into(text);
      m.msg.payload = std::move(text);
    }
    for (auto& s : sinks_) {
      if (s->should_log(m.msg.lvl)) {
        s->write(m.msg);
      }
    }
    pending_.fetch_sub(1, std::memory_order_release);
  }

  void backend_loop(std::stop_token st) {
    async_msg m;
    while (queue_.dequeue(m, st)) {
      process(m);
    }
    // Drain whatever arrived before stop was observed.
    while (queue_.try_dequeue(m)) {
      process(m);
    }
    for (auto& s : sinks_) {
      s->flush();
    }
  }

  // Member order matters: `backend_threads_` must be declared LAST so that on
  // destruction it is joined FIRST (reverse order), while `pending_` and
  // `queue_` are still alive for the backend threads to use during the drain.
  std::atomic<std::size_t> pending_{0};
  detail::blocking_queue<async_msg> queue_;
  overflow_policy policy_;
  std::vector<std::jthread> backend_threads_;
};

} // namespace zest
