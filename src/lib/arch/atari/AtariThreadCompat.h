/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// Compatibility layer for Atari FreeMiNT threading
// Only needed for older GCC versions that lack C++11 threading support

#include <pthread.h>
#include <time.h>
#include <cstdint>

// Minimal std library compatibility for m68k-atari-mint
namespace std {

// Minimal chrono implementation
namespace chrono {
    typedef std::int64_t rep;

    template<std::intmax_t Num, std::intmax_t Denom = 1>
    struct ratio {
        static constexpr std::intmax_t num = Num;
        static constexpr std::intmax_t den = Denom;
    };

    template<typename Rep, typename Period = ratio<1>>
    class duration {
    public:
        typedef Rep rep;
        typedef Period period;

    private:
        rep m_rep;

    public:
        constexpr duration() : m_rep(0) {}
        constexpr duration(const rep& r) : m_rep(r) {}

        constexpr rep count() const { return m_rep; }

        duration operator-(const duration& other) const {
            return duration(m_rep - other.m_rep);
        }
    };

    typedef duration<rep, ratio<1, 1000000000>> nanoseconds;
    typedef duration<rep> seconds;

    template<typename ToDuration, typename Rep, typename Period>
    constexpr ToDuration duration_cast(const duration<Rep, Period>& d) {
        return ToDuration(d.count());
    }
} // namespace chrono

// Simple mutex wrapper for pthread_mutex_t
class mutex {
private:
    pthread_mutex_t m_mutex;

public:
    mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

    bool try_lock() {
        return pthread_mutex_trylock(&m_mutex) == 0;
    }

    // Disable copy
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    // For condition variable access
    pthread_mutex_t* native_handle() {
        return &m_mutex;
    }
};

// Simple unique_lock wrapper
template<typename Mutex>
class unique_lock {
private:
    Mutex* m_mutex;
    bool m_locked;

public:
    unique_lock(Mutex& mutex) : m_mutex(&mutex), m_locked(false) {
        lock();
    }

    ~unique_lock() {
        if (m_locked) {
            unlock();
        }
    }

    void lock() {
        if (!m_locked) {
            m_mutex->lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex->unlock();
            m_locked = false;
        }
    }

    // Disable copy
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    // For condition variable access
    Mutex* mutex() {
        return m_mutex;
    }
};

// Simple condition_variable wrapper
class condition_variable {
private:
    pthread_cond_t m_cond;

public:
    condition_variable() {
        pthread_cond_init(&m_cond, nullptr);
    }

    ~condition_variable() {
        pthread_cond_destroy(&m_cond);
    }

    void notify_one() {
        pthread_cond_signal(&m_cond);
    }

    void notify_all() {
        pthread_cond_broadcast(&m_cond);
    }

    void wait(unique_lock<mutex>& lock) {
        pthread_cond_wait(&m_cond, lock.mutex()->native_handle());
    }

    template<typename Rep, typename Period>
    bool wait_for(unique_lock<mutex>& lock, const std::chrono::duration<Rep, Period>& timeout) {
        struct timespec ts;
        auto timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(timeout_ns);
        ts.tv_sec = sec.count();
        ts.tv_nsec = (timeout_ns - sec).count();

        return pthread_cond_timedwait(&m_cond, lock.mutex()->native_handle(), &ts) == 0;
    }

    // Disable copy
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;
};

} // namespace std

