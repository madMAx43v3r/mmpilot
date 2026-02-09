/*
 * threads.h
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_THREADS_H_
#define INCLUDE_MMPILOT_THREADS_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>


namespace mmpilot {

class ThreadPool {
public:
	explicit ThreadPool(std::size_t thread_count)
	{
		if(thread_count == 0) {
			throw std::invalid_argument("ThreadPool: thread_count must be > 0");
		}

		threads_.reserve(thread_count);
		for(std::size_t i = 0; i < thread_count; ++i) {
			threads_.emplace_back([this]
			{
				worker_loop();
			});
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	~ThreadPool()
	{
		shutdown();
	}

	// Enqueue a task. Throws if shutdown was requested.
	void dispatch(std::function<void()> task)
	{
		if(!task) {
			return;
		}
		{
			std::lock_guard<std::mutex> lk(mtx_);
			if(shutdown_requested_) {
				throw std::runtime_error("ThreadPool: dispatch() after shutdown()");
			}
			queue_.push(std::move(task));
			++pending_; // counts tasks "in flight" (queued or executing)
		}
		cv_work_.notify_one();
	}

	// Convenience overload: accepts any callable/lambda, stores as std::function<void()>.
	template<class F>
	void dispatch(F&& f)
	{
		dispatch(std::function<void()>(std::forward<F>(f)));
	}

	// Wait until all tasks submitted so far are finished (queue empty and nothing running).
	void wait()
	{
		std::unique_lock<std::mutex> lk(mtx_);
		cv_done_.wait(lk, [this] {
			return pending_ == 0;
		});
	}

	// Stop accepting new tasks, drain existing tasks, then join threads.
	// Safe to call multiple times.
	void shutdown()
	{
		{
			std::lock_guard<std::mutex> lk(mtx_);
			if(shutdown_complete_) {
				return;
			}
			shutdown_requested_ = true;
		}

		// Drain all work first (clean shutdown).
		wait();

		{
			std::lock_guard<std::mutex> lk(mtx_);
			shutdown_complete_ = true;
		}
		cv_work_.notify_all();

		for(auto& t : threads_) {
			if(t.joinable()) {
				t.join();
			}
		}
		threads_.clear();
	}

private:
	void worker_loop()
	{
		for(;;) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lk(mtx_);
				cv_work_.wait(lk, [this] {
					return shutdown_complete_ || !queue_.empty();
				});

				if(shutdown_complete_ && queue_.empty()) {
					return; // exit thread
				}
				task = std::move(queue_.front());
				queue_.pop();
			}

			// Execute outside the lock.
			try {
				task();
			} catch(...) {
				// Swallow exceptions so one task can't kill a worker thread.
				// If you want reporting, store std::exception_ptr in a threadsafe list.
			}

			{
				std::lock_guard<std::mutex> lk(mtx_);
				if(--pending_ == 0) {
					cv_done_.notify_all();
				}
			}
		}
	}

private:
	std::mutex mtx_;
	std::condition_variable cv_work_;
	std::condition_variable cv_done_;

	std::queue<std::function<void()>> queue_;
	std::vector<std::thread> threads_;

	std::size_t pending_ = 0;          // tasks queued or executing
	bool shutdown_requested_ = false;  // no longer accept dispatch
	bool shutdown_complete_ = false;   // workers should exit when queue drains
};



} // mmpilot

#endif /* INCLUDE_MMPILOT_THREADS_H_ */
