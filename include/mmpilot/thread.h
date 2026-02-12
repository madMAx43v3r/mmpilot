/*
 * thread.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_THREAD_H_
#define INCLUDE_MMPILOT_THREAD_H_

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <stdexcept>

namespace mmpilot {

class Thread {
public:
	using Task = std::function<void()>;
	using Main = std::function<void(Thread& self)>;

	Thread() = default;

	explicit Thread(Main main_fn) {
		start(std::move(main_fn));
	}

	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	~Thread() {
		close();
	}

	void start(Main main_fn)
	{
		std::lock_guard<std::mutex> lk(_mtx);
		if(_thr.joinable()) {
			throw std::runtime_error("Thread already started");
		}
		_shutdown = false;

		_thr = std::thread([this, main_fn = std::move(main_fn)] {
			main_fn(*this);
		});
	}

	// Enqueue work to be executed on the thread.
	// Returns false if shutting down.
	bool post(Task task)
	{
		if(!task) {
			return true;
		}
		{
			std::lock_guard<std::mutex> lk(_mtx);
			if(_shutdown) {
				return false;
			}
			_queue.emplace_back(std::move(task));
		}
		_cv.notify_all();
		return true;
	}

	void shutdown()
	{
		{
			std::lock_guard<std::mutex> lk(_mtx);
			_shutdown = true;
		}
		_cv.notify_all();
	}

	bool is_shutdown() const
	{
		std::lock_guard<std::mutex> lk(_mtx);
		return _shutdown;
	}

	void run()
	{
		for(;;) {
			Task task;
			{
				std::unique_lock<std::mutex> lk(_mtx);

				_busy = false;
				_cv.notify_all();

				_cv.wait(lk, [&] {
					return _shutdown || !_queue.empty();
				});

				if(_queue.empty()) {
					return;
				}
				_busy = true;

				task = std::move(_queue.front());
				_queue.pop_front();
			}
			task();
		}
	}

	void sync()
	{
		std::unique_lock<std::mutex> lk(_mtx);
		_cv.wait(lk, [&] {
			return (!_busy && _queue.empty()) || _shutdown;
		});
	}

	void join()
	{
		if(_thr.joinable()) {
			_thr.join();
		}
	}

	void close()
	{
		shutdown();
		join();
	}

private:
	mutable std::mutex _mtx;
	std::condition_variable _cv;
	std::deque<Task> _queue;

	bool _busy = false;
	bool _shutdown = false;
	std::thread _thr;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_THREAD_H_ */
