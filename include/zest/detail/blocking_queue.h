#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <utility>
#include <vector>

namespace zest::detail {

// Bounded lock-free MPMC queue (Dmitry Vyukov's algorithm). Capacity is
// rounded up to a power of two. Only `try_*` operations are provided here;
// blocking backpressure lives in `blocking_queue`.
template <typename T> class mpmc_bounded_queue {
public:
  explicit mpmc_bounded_queue(std::size_t capacity)
      : capacity_(next_pow2(capacity == 0 ? 1 : capacity)), mask_(capacity_ - 1),
        buffer_(capacity_) {
    for (std::size_t i = 0; i < capacity_; ++i) {
      buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }
  }

  mpmc_bounded_queue(const mpmc_bounded_queue&) = delete;
  mpmc_bounded_queue& operator=(const mpmc_bounded_queue&) = delete;

  bool try_enqueue(T&& value) {
    cell* c = nullptr;
    std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
    for (;;) {
      c = &buffer_[pos & mask_];
      const std::size_t seq = c->sequence.load(std::memory_order_acquire);
      const std::intptr_t dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
      if (dif == 0) {
        if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (dif < 0) {
        return false; // full
      } else {
        pos = enqueue_pos_.load(std::memory_order_relaxed);
      }
    }
    c->data = std::move(value);
    c->sequence.store(pos + 1, std::memory_order_release);
    return true;
  }

  bool try_dequeue(T& out) {
    cell* c = nullptr;
    std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    for (;;) {
      c = &buffer_[pos & mask_];
      const std::size_t seq = c->sequence.load(std::memory_order_acquire);
      const std::intptr_t dif =
          static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
      if (dif == 0) {
        if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (dif < 0) {
        return false; // empty
      } else {
        pos = dequeue_pos_.load(std::memory_order_relaxed);
      }
    }
    out = std::move(c->data);
    c->sequence.store(pos + mask_ + 1, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept {
    return enqueue_pos_.load(std::memory_order_relaxed) ==
           dequeue_pos_.load(std::memory_order_relaxed);
  }

  bool full() const noexcept {
    return enqueue_pos_.load(std::memory_order_relaxed) -
               dequeue_pos_.load(std::memory_order_relaxed) >=
           capacity_;
  }

  std::size_t capacity() const noexcept { return capacity_; }

private:
  struct cell {
    std::atomic<std::size_t> sequence{0};
    T data{};
  };

  static std::size_t next_pow2(std::size_t v) {
    --v;
    for (std::size_t i = 1; i < sizeof(v) * 8; i <<= 1) {
      v |= v >> i;
    }
    return v + 1;
  }

  const std::size_t capacity_;
  const std::size_t mask_;
  std::vector<cell> buffer_;
  std::atomic<std::size_t> enqueue_pos_{0};
  std::atomic<std::size_t> dequeue_pos_{0};
};

// Blocking wrapper around `mpmc_bounded_queue`: producers block with
// backpressure when full, consumers block when empty. `dequeue` supports
// cooperative shutdown via `std::stop_token`.
template <typename T> class blocking_queue {
public:
  explicit blocking_queue(std::size_t capacity) : queue_(capacity) {}

  blocking_queue(const blocking_queue&) = delete;
  blocking_queue& operator=(const blocking_queue&) = delete;

  void enqueue(T&& item) {
    while (!queue_.try_enqueue(std::move(item))) {
      std::unique_lock<std::mutex> lock(mutex_);
      not_full_.wait(lock, [this] { return !queue_.full(); });
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      not_empty_.notify_one();
    }
  }

  // Overflow policy: drop-newest. If the queue is full the incoming item is
  // discarded instead of blocking the producer. Returns true if enqueued.
  bool try_enqueue_drop_newest(T&& item) {
    if (!queue_.try_enqueue(std::move(item))) {
      return false; // full: drop the incoming message
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      not_empty_.notify_one();
    }
    return true;
  }

  // Overflow policy: drop-oldest. If the queue is full, evict the oldest
  // pending item to make room for the incoming one (never blocks). Returns the
  // number of items evicted — the caller must account for them (they were
  // counted as pending when first enqueued but will never be processed).
  //
  // Under contention the eviction loop may evict more than one item (another
  // producer may grab a freed slot before our retry); the returned count is the
  // exact number evicted. `T` must be default-constructible.
  std::size_t enqueue_drop_oldest(T&& item) {
    std::size_t evicted = 0;
    while (!queue_.try_enqueue(std::move(item))) {
      // Full: evict the oldest pending item. If a consumer drained the queue in
      // the meantime (try_dequeue fails), just retry the enqueue.
      T evicted_item;
      if (queue_.try_dequeue(evicted_item)) {
        ++evicted;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      not_empty_.notify_one();
    }
    return evicted;
  }

  // Returns false when stop was requested and the queue is empty.
  bool dequeue(T& out, const std::stop_token& st) {
    for (;;) {
      if (queue_.try_dequeue(out)) {
        {
          std::lock_guard<std::mutex> lock(mutex_);
          not_full_.notify_one();
        }
        return true;
      }
      std::unique_lock<std::mutex> lock(mutex_);
      not_empty_.wait(lock, [this, &st] { return !queue_.empty() || st.stop_requested(); });
      if (st.stop_requested() && queue_.empty()) {
        return false;
      }
    }
  }

  bool try_dequeue(T& out) {
    if (queue_.try_dequeue(out)) {
      std::lock_guard<std::mutex> lock(mutex_);
      not_full_.notify_one();
      return true;
    }
    return false;
  }

  void notify_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    not_empty_.notify_all();
  }

  bool empty() const noexcept { return queue_.empty(); }

private:
  mpmc_bounded_queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
};

} // namespace zest::detail
