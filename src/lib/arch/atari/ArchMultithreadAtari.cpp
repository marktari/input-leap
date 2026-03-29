#include <pth.h>
#include "arch/atari/ArchMultithreadAtari.h"
#include "mt/XMT.h"
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <cstring>

namespace inputleap {

// GNU Pth thread implementation for Atari
class ArchThreadImpl {
public:
    ArchThreadImpl(const std::function<void()>& func)
        : m_func(func), m_thread(nullptr), m_cancelled(false), m_id(++s_nextId) {
        if (m_func) {
            // Create the pth thread with optimized attributes
            pth_attr_t attr = pth_attr_new();
            if (attr) {
                // Set standard priority for better performance
                pth_attr_set(attr, PTH_ATTR_PRIO, PTH_PRIO_STD);
                m_thread = pth_spawn(attr, threadWrapper, this);
                pth_attr_destroy(attr);
            } else {
                m_thread = pth_spawn(PTH_ATTR_DEFAULT, threadWrapper, this);
            }
            if (!m_thread) {
                throw XMTThreadUnavailable();
            }
        }
    }

    ~ArchThreadImpl() {
        if (m_thread) {
            // Don't wait here, just detach if not already joined
            if (!m_cancelled && pth_join(m_thread, nullptr) != TRUE) {
                // If join fails, try to cancel
                pth_cancel(m_thread);
            }
            m_thread = nullptr;
        }
    }

    static void* threadWrapper(void* arg) {
        ArchThreadImpl* impl = static_cast<ArchThreadImpl*>(arg);
        try {
            if (impl->m_func && !impl->m_cancelled) {
                impl->m_func();
            }
        } catch (...) {
            // Catch any exceptions to prevent crashes
        }
        return nullptr;
    }

    void cancel() {
        m_cancelled = true;
        if (m_thread) {
            pth_cancel(m_thread);
        }
    }

    bool isCancelled() const {
        return m_cancelled;
    }

    bool isRunning() const {
        if (!m_thread) return false;
        pth_attr_t attr = pth_attr_of(m_thread);
        if (!attr) return false;
        pth_state_t state;
        pth_attr_get(attr, PTH_ATTR_STATE, &state);
        return state == PTH_STATE_READY || state == PTH_STATE_NEW;
    }

    bool isExited() const {
        if (!m_thread) return true;
        if (m_cancelled) return true;
        pth_attr_t attr = pth_attr_of(m_thread);
        if (!attr) return true;
        pth_state_t state;
        pth_attr_get(attr, PTH_ATTR_STATE, &state);
        return state == PTH_STATE_DEAD;
    }

    bool wait(double timeout) {
        if (!m_thread) return true;

        if (timeout < 0.0) {
            // Wait indefinitely
            return pth_join(m_thread, nullptr) == TRUE;
        } else if (timeout == 0.0) {
            // Don't wait, just check status
            return isExited();
        } else {
            // Wait with timeout - use a simpler approach for now
            double start = static_cast<double>(time(nullptr));
            int yield_count = 0;
            while (!isExited()) {
                double now = static_cast<double>(time(nullptr));
                if (now - start >= timeout) {
                    return false; // Timeout
                }
                pth_yield(nullptr); // Yield to other threads

                // Only sleep occasionally to reduce latency
                if (++yield_count % 10 == 0) {
                    pth_usleep(50); // Sleep 0.05ms every 10th iteration
                }
            }
            return true;
        }
    }

    IArchMultithread::ThreadID getId() const {
        return m_id;
    }

    pth_t getPthThread() const {
        return m_thread;
    }

private:
    std::function<void()> m_func;
    pth_t m_thread;
    bool m_cancelled;
    IArchMultithread::ThreadID m_id;
    static unsigned int s_nextId;
};

unsigned int ArchThreadImpl::s_nextId = 0;

// Signal handler storage
static ArchMultithreadAtari::SignalFunc s_signalHandlers[ArchMultithreadAtari::kNUM_SIGNALS] = {nullptr};
static void* s_signalUserDatas[ArchMultithreadAtari::kNUM_SIGNALS] = {nullptr};

ArchMultithreadAtari::ArchMultithreadAtari() {
    // Initialize GNU Pth library
    if (!pth_init()) {
        throw XMTThreadUnavailable();
    }
}

ArchMultithreadAtari::~ArchMultithreadAtari() {
    // Cleanup GNU Pth library
    pth_kill();
}

ArchThread ArchMultithreadAtari::newThread(const std::function<void()>& func) {
    return new ArchThreadImpl(func);
}

ArchThread ArchMultithreadAtari::newCurrentThread() {
    // Create a thread object representing the current (main) pth thread
    ArchThreadImpl* impl = new ArchThreadImpl(nullptr);
    return impl;
}

ArchThread ArchMultithreadAtari::copyThread(ArchThread thread) {
    // Just return the same pointer (reference counting not implemented)
    return thread;
}

void ArchMultithreadAtari::closeThread(ArchThread thread) {
    if (thread) {
        delete thread;
    }
}

void ArchMultithreadAtari::cancelThread(ArchThread thread) {
    if (thread) {
        static_cast<ArchThreadImpl*>(thread)->cancel();
    }
}

void ArchMultithreadAtari::setPriorityOfThread(ArchThread, int) {
    // No-op - priority not supported
}

void ArchMultithreadAtari::testCancelThread() {
    // Test for pth cancellation and smart yield
    pth_cancel_point();

    // Only yield if there are other threads ready to run
    if (pth_ctrl(PTH_CTRL_GETTHREADS) > 1) {
        pth_yield(nullptr);
    }
}

bool ArchMultithreadAtari::wait(ArchThread thread, double timeout) {
    if (!thread) {
        return true;
    }

    auto* impl = static_cast<ArchThreadImpl*>(thread);
    return impl->wait(timeout);
}

bool ArchMultithreadAtari::isSameThread(ArchThread t1, ArchThread t2) {
    return t1 == t2;
}

bool ArchMultithreadAtari::isExitedThread(ArchThread thread) {
    if (!thread) {
        return true;
    }
    return static_cast<ArchThreadImpl*>(thread)->isExited();
}

IArchMultithread::ThreadID ArchMultithreadAtari::getIDOfThread(ArchThread thread) {
    if (!thread) {
        return 0;
    }
    return static_cast<ArchThreadImpl*>(thread)->getId();
}

void ArchMultithreadAtari::setSignalHandler(ESignal signal, SignalFunc func, void* userData) {
    if (signal >= 0 && signal < kNUM_SIGNALS) {
        s_signalHandlers[signal] = func;
        s_signalUserDatas[signal] = userData;
    }
}

void ArchMultithreadAtari::raiseSignal(ESignal signal) {
    if (signal >= 0 && signal < kNUM_SIGNALS && s_signalHandlers[signal]) {
        s_signalHandlers[signal](signal, s_signalUserDatas[signal]);
    } else if (signal == kINTERRUPT || signal == kTERMINATE) {
        // Default behavior: cancel main thread
        testCancelThread();
    }
}

void ArchMultithreadAtari::runBackgroundThreads() {
    // With GNU Pth, thread scheduling is handled automatically
    // Just yield to allow other threads to run
    pth_yield(nullptr);
}

} // namespace inputleap
