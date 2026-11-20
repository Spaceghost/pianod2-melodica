///
/// Cross-thread/thread-safe circular queue.
/// @file       threadsafecircularqueue.h - pianod
/// @author     Perette Barella
/// @date       2020-04-22
/// @copyright  Copyright (c) 2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include <cassert>
#include <cstdint>
#include <ctime>

#include <type_traits>

/** An circular queue meant for sharing data across threads.
    @tparam ValueType Type of objects being passed around.
    @tparam capacity The maximum capacity of the queue. */
template <typename ValueType, uint32_t capacity>
class ThreadSafeCircularQueue {
    using index_type = uint32_t;
    struct StorageType {
        int overflows {0}; ///< Number of messages lost due to queue overflow.
        time_t time; ///< Time at which an item was queued.
        ValueType value;
    };
    StorageType data[capacity];
    std::atomic<index_type> read_index;
    std::mutex read_mutex;
    std::condition_variable read_bell;

    std::atomic<index_type> write_index;
    std::mutex write_mutex;
    std::condition_variable write_bell;

    /// Increment an index, wrapping around buffer size.
    inline index_type next (index_type index) {
        return (index + 1) % capacity;
    }

    /// Perform actions common to the various forms of push_back.
    inline void write_common() {
        data[write_index].time = time (nullptr);
        write_index = next (write_index);
        data[write_index].overflows = 0;
        read_bell.notify_one();
    }

public:
    using value_type = ValueType;

    /** Add or move an item into the queue.  If the queue is full,
        wait until there's space.
        @param value The value to add. */
    template <typename VT>
    void push_back (VT &&value) {
        std::unique_lock<std::mutex> lock (write_mutex);
        while (next (write_index) == read_index) {
            write_bell.wait (lock);
        }
        data[write_index].value = std::move (value);
        write_common();
    }

    /** Add or move an item into the queue.  If the queue is full,
        mark that an overflow occured, and do not block.
        @param value The value to add.
        @param wait_duration Optional: If specified, a duration to wait before giving up.
        Default is to not wait.  Observation, however, shows that small waits can go
        a long way to reduce overflows, providing a chance for readers to catch up when
        the queue is full.
        @return True on success, false on overflow. */
    template <typename VT>
    bool try_push_back (VT &&value, 
                        const std::chrono::milliseconds &wait_duration = std::chrono::milliseconds::zero()) {
        std::unique_lock<std::mutex> lock (write_mutex);
        if (next (write_index) == read_index) {
            if (!read_bell.wait_for (lock, wait_duration, [this] ()->bool {
                return (next (write_index) != read_index);
            })) {
                data[write_index].overflows++;
                return false;
            }
        }
        data[write_index].value = std::move (value);
        write_common();
        return true;
    }

    /** Retrieve an item from the queue.  If the queue is empty,
        block until an item becomes available.
        @param overflows If non-null, set to the number of items dropped due to overflow.
        @param sent_time If non-null, the time at which the item was queued.
        @return The next value from the queue. */
    ValueType pop_front (int *overflows = nullptr, time_t *sent_time = nullptr) {
        std::unique_lock<std::mutex> lock (read_mutex);
        while (read_index == write_index) {
            read_bell.wait (lock);
        }
        if (overflows) {
            *overflows = data[read_index].overflows;
        }
        if (sent_time) {
            *sent_time = data[read_index].time;
        }
        ValueType temp = std::move (data[read_index].value);
        read_index = next (read_index);
        write_bell.notify_one();
        return temp;
    }

    /** Retrieve an item from the queue.  If the queue is empty, don't block.
        @param value On return, set to the value retrieved from the queue.
        @param overflows If non-null, set to the number of items dropped due to overflow.
        @param sent_time If non-null, the time at which the item was queued.
        @param wait_duration A duration to wait for for an item; defaults to no wait.
        @return True if a value was retrieved, false if the queue was empty. */
    bool try_pop_front (ValueType *value,
                        int *overflows = nullptr,
                        time_t *sent_time = nullptr,
                        std::chrono::milliseconds wait_duration = std::chrono::milliseconds::zero()) {
        std::unique_lock<std::mutex> lock (read_mutex);
        if (read_index == write_index) {
            if (!read_bell.wait_for (lock, wait_duration, [this] ()->bool {
                return (read_index != write_index);
            })) {
                return false;
            };
        }
        if (overflows) {
            *overflows = data[read_index].overflows;
        }
        if (sent_time) {
            *sent_time = data[read_index].time;
        }
        *value = std::move (data[read_index].value);
        read_index = next (read_index);
        write_bell.notify_one();
        return true;
    }
};
