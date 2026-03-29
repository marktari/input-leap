#ifndef ARCH_MULTITHREAD_ATARI_H
#define ARCH_MULTITHREAD_ATARI_H

#include "arch/IArchMultithread.h"
#include <functional>

#define ARCH_MULTITHREAD ArchMultithreadAtari

namespace inputleap {

class ArchMultithreadAtari : public IArchMultithread {
public:
    ArchMultithreadAtari();
    ~ArchMultithreadAtari() override;

    // IArchMultithread implementation
    ArchThread newThread(const std::function<void()>& func) override;
    ArchThread newCurrentThread() override;
    ArchThread copyThread(ArchThread thread) override;
    void closeThread(ArchThread thread) override;
    void cancelThread(ArchThread thread) override;
    void setPriorityOfThread(ArchThread thread, int n) override;
    void testCancelThread() override;
    bool wait(ArchThread thread, double timeout) override;
    bool isSameThread(ArchThread t1, ArchThread t2) override;
    bool isExitedThread(ArchThread thread) override;
    ThreadID getIDOfThread(ArchThread thread) override;
    void setSignalHandler(ESignal signal, SignalFunc func, void* userData) override;
    void raiseSignal(ESignal signal) override;

    // Atari-specific: run background threads cooperatively
    static void runBackgroundThreads();
};

} // namespace inputleap

#endif
