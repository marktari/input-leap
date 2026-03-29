/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap contributors
 *
 * Thread compatibility for Atari m68k targets.
 */

#pragma once

#if !WINAPI_ATARI

#include <condition_variable>
#include <mutex>
#include <thread>

#else

#include <pth.h>
#include <chrono>
#include <cstdint>

namespace std {

// Use pth mutexes for proper cooperative threading
class mutex {
public:
    mutex() {
        pth_mutex_init(&m_mutex);
    }

    void lock() {
        pth_mutex_acquire(&m_mutex, FALSE, nullptr);
    }

    bool try_lock() {
        return pth_mutex_acquire(&m_mutex, TRUE, nullptr) == TRUE;
    }

    void unlock() {
        pth_mutex_release(&m_mutex);
    }

    // Allow condition_variable access to the mutex
    friend class condition_variable;

private:
    pth_mutex_t m_mutex;
};

template<class Mutex>
class lock_guard {
public:
    explicit lock_guard(Mutex& mutex) : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~lock_guard()
    {
        mutex_.unlock();
    }

private:
    Mutex& mutex_;
};

template<class Mutex>
class unique_lock {
public:
    explicit unique_lock(Mutex& mutex) : mutex_(&mutex), owns_(false)
    {
        lock();
    }

    ~unique_lock()
    {
        if (owns_) {
            mutex_->unlock();
        }
    }

    void lock()
    {
        if (!owns_) {
            mutex_->lock();
            owns_ = true;
        }
    }

    void unlock()
    {
        if (owns_) {
            mutex_->unlock();
            owns_ = false;
        }
    }

    bool owns_lock() const
    {
        return owns_;
    }

    Mutex* mutex() const
    {
        return mutex_;
    }

private:
    Mutex* mutex_;
    bool owns_;
};

enum class cv_status {
    no_timeout,
    timeout
};

// Use pth condition variables
class condition_variable {
public:
    condition_variable() {
        pth_cond_init(&m_cond);
    }

    void notify_one() {
        pth_cond_notify(&m_cond, FALSE);
    }

    void notify_all() {
        pth_cond_notify(&m_cond, TRUE);
    }

    void wait(unique_lock<mutex>& lock)
    {
        // Simple cooperative wait - unlock, yield, and relock
        lock.unlock();
        pth_yield(nullptr);
        lock.lock();
    }

    template<class Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred)
    {
        while (!pred()) {
            wait(lock);
        }
    }

    template<class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel_time)
    {
        lock.unlock();

        const auto microseconds = chrono::duration_cast<chrono::microseconds>(rel_time).count();
        if (microseconds > 0) {
            // Use pth_usleep for microsecond precision
            pth_usleep(static_cast<unsigned int>(microseconds));
        } else {
            pth_yield(nullptr);
        }

        lock.lock();
        return cv_status::timeout;
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock, const chrono::duration<Rep, Period>& rel_time,
                  Predicate pred)
    {
        const auto deadline = chrono::steady_clock::now() + rel_time;
        while (!pred()) {
            const auto now = chrono::steady_clock::now();
            if (now >= deadline) {
                return pred();
            }

            const auto remaining = chrono::duration_cast<chrono::microseconds>(deadline - now).count();
            if (remaining <= 0) {
                return pred();
            }

            wait_for(lock, chrono::microseconds(remaining > 1000 ? 1000 : remaining));
        }

        return true;
    }

private:
    pth_cond_t m_cond;
};

namespace this_thread {

template<class Rep, class Period>
void sleep_for(const chrono::duration<Rep, Period>& rel_time)
{
    const auto microseconds = chrono::duration_cast<chrono::microseconds>(rel_time).count();
    if (microseconds <= 0) {
        return;
    }

    // Use pth_usleep for microsecond precision
    pth_usleep(static_cast<unsigned int>(microseconds));
}

inline void yield()
{
    pth_yield(nullptr);
}

} // namespace this_thread

} // namespace std

#endif
