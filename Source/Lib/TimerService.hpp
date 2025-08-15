#pragma once
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_set>

class TimerService {
public:
    using TimerId = uint64_t;

    TimerService() : mStop(false), mNextId(1) {
        mWorker = std::thread([this]() { run(); });
    }

    ~TimerService() {
        {
            std::lock_guard<std::mutex> lock(mLock);
            mStop = true;
            mCondVar.notify_all();
        }

        if (mWorker.joinable()) {
            mWorker.join();
        }
    }

    // Schedule a one-shot timer
    TimerId Once(std::chrono::milliseconds delay, std::function<void()> callback) {
        return ScheduleTimer(delay, std::chrono::milliseconds(0), std::move(callback));
    }

    // Schedule a repeating timer
    TimerId Repeat(std::chrono::milliseconds interval, std::function<void()> callback) {
        return ScheduleTimer(interval, interval, std::move(callback));
    }

    // Cancel a scheduled timer
    void Cancel(TimerId id) {
        std::lock_guard<std::mutex> lock(mLock);
        mActiveTimers.erase(id); // will be skipped when popped
        mCondVar.notify_all();
    }

private:
    struct TimerItem {
        TimerId id;
        std::chrono::steady_clock::time_point time;
        std::chrono::milliseconds interval; // 0 for one-shot
        std::function<void()> callback;

        bool operator>(const TimerItem& other) const {
            return time > other.time;
        }
    };

    TimerId ScheduleTimer(std::chrono::milliseconds delay, std::chrono::milliseconds interval, std::function<void()> callback) {
        auto id = mNextId++;
        auto time = std::chrono::steady_clock::now() + delay;

        {
            std::lock_guard<std::mutex> lock(mLock);
            mTimers.push(TimerItem{id, time, interval, std::move(callback)});
            mActiveTimers.insert(id);
        }

        mCondVar.notify_all();
        return id;
    }

    void run() {
        std::unique_lock<std::mutex> lock(mLock);

        while (!mStop) {
            if (mTimers.empty()) {
                mCondVar.wait(lock, [this] { return mStop || !mTimers.empty(); });
                continue;
            }

            auto now = std::chrono::steady_clock::now();
            auto next = mTimers.top().time;

            if (mCondVar.wait_until(lock, next, [this] {
                return mStop || mTimers.empty() || mTimers.top().time <= std::chrono::steady_clock::now();
            })) {
                // Woken early (cancel, new timer, or stop)
            }

            if (mStop) {
                break;
            }

            now = std::chrono::steady_clock::now();
            while (!mTimers.empty() && mTimers.top().time <= now) {
                auto item = mTimers.top();
                mTimers.pop();

                if (mActiveTimers.count(item.id)) {
                    if (item.interval.count() > 0) {
                        // Reschedule repeating timer
                        item.time = now + item.interval;
                        mTimers.push(item);
                    } else {
                        // One-shot: remove from active set
                        mActiveTimers.erase(item.id);
                    }

                    lock.unlock();
                    item.callback();
                    lock.lock();
                }
            }
        }
    }

    std::thread mWorker;
    std::atomic<bool> mStop;
    std::atomic<TimerId> mNextId;
    std::priority_queue<TimerItem, std::vector<TimerItem>, std::greater<TimerItem>> mTimers;
    std::unordered_set<TimerId> mActiveTimers;
    std::condition_variable mCondVar;
    std::mutex mLock;
};
