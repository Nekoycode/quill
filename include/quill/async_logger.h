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
#include <quill/detail/deferred_message.h>
#include <quill/logger.h>
#include <quill/sink.h>

namespace quill {

// Message exchanged between the async frontend and the backend writer thread.
struct async_msg {
  enum class kind : std::uint8_t { log, flush };

  kind k{kind::log};
  log_msg msg;                       // metadata (+ payload when not deferred)
  detail::deferred_message deferred; // set when formatting is deferred
  std::promise<void>* sync{nullptr}; // used by flush to signal completion
};

// A logger whose sink writes are performed on a dedicated background thread.
// The frontend only captures the message (metadata + arguments); the backend
// formats the `%v` text and applies each sink's formatter before writing.
// Shutdown requests a stop and drains all pending messages.
class async_logger final : public logger {
public:
  explicit async_logger(std::string name, std::vector<std::shared_ptr<sinks::sink>> sinks,
                        std::size_t queue_size = 4096)
      : logger(std::move(name), std::move(sinks), /*defers_formatting=*/true), queue_(queue_size),
        backend_thread_([this](std::stop_token st) { backend_loop(st); }) {}

  ~async_logger() override {
    backend_thread_.request_stop();
    queue_.notify_all();
  }

  void flush() override {
    std::promise<void> p;
    auto f = p.get_future();
    queue_.enqueue(async_msg{async_msg::kind::flush, log_msg{}, detail::deferred_message{}, &p});
    f.wait();
  }

protected:
  void log_meta(log_msg&& meta, detail::deferred_message&& deferred) override {
    queue_.enqueue(async_msg{async_msg::kind::log, std::move(meta), std::move(deferred), nullptr});
  }

private:
  void process(async_msg& m) {
    if (m.k == async_msg::kind::flush) {
      for (auto& s : sinks_) {
        s->flush();
      }
      if (m.sync != nullptr) {
        m.sync->set_value();
      }
      return;
    }

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
