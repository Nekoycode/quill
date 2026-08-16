#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <quill/detail/blocking_queue.h>
#include <quill/logger.h>
#include <quill/sink.h>

namespace quill {

// Message exchanged between the async frontend and the backend writer thread.
struct async_msg {
  enum class kind : std::uint8_t { log, flush };

  kind k{kind::log};
  std::size_t sink_index{0};
  log_msg msg;                  // used when k == log
  std::promise<void>* sync{nullptr}; // used when k == flush
};

// A logger whose sink writes are performed on a dedicated background thread.
// The frontend formats only the message text and enqueues the record; the
// backend applies each sink's formatter and writes. Shutdown (destruction)
// requests a stop and drains all pending messages, so no in-flight record is
// lost.
class async_logger final : public logger {
public:
  explicit async_logger(std::string name,
                        std::vector<std::shared_ptr<sinks::sink>> sinks,
                        std::size_t queue_size = 4096)
      : logger(std::move(name), std::move(sinks)), queue_(queue_size),
        backend_thread_([this](std::stop_token st) { backend_loop(st); }) {}

  ~async_logger() override {
    backend_thread_.request_stop();
    queue_.notify_all();
  }

  void flush() override {
    std::promise<void> p;
    auto f = p.get_future();
    queue_.enqueue(async_msg{async_msg::kind::flush, 0, log_msg{}, &p});
    f.wait();
  }

protected:
  void sink_it_(std::size_t sink_index, const log_msg& msg) override {
    queue_.enqueue(async_msg{async_msg::kind::log, sink_index, msg, nullptr});
  }

private:
  void process(async_msg& m) {
    if (m.k == async_msg::kind::log) {
      if (m.sink_index < sinks_.size()) {
        sinks_[m.sink_index]->write(m.msg);
      }
    } else {
      for (auto& s : sinks_) {
        s->flush();
      }
      if (m.sync != nullptr) {
        m.sync->set_value();
      }
    }
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

  detail::blocking_queue<async_msg> queue_;
  std::jthread backend_thread_;
};

} // namespace quill
