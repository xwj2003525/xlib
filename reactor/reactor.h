#pragma once
#include "../time/time.h"
#include <atomic>
#include <map>
#include <thread>
#include <mutex>
#ifdef REACTOR_ENABLE_FUNCTIONAL
#include <functional>
#endif

namespace x {
	namespace Eventloop {

#ifdef REACTOR_ENABLE_FUNCTIONAL
#pragma message("use callable as std::functional<void()>")
	using Callable = std::function<void()>;
#else
	using Callable = void (*)();
#pragma message("use callable as void(*)()")
#endif

	using Stamp = x::time::Stamp;
	using Gap = x::time::Gap;
	using Fd = int32_t;
	using Events = int16_t;
	using Iteration = uint64_t;

	enum class Event : int16_t { Read = 1, Write = 2, Error = 4, Timeout = 8, Close = 16 };

	class EventView {
	public:
		Fd fd;
		Event event;
		Callable callable;
		Iteration iteration;
		
		// 只有计划任务关心此两个成员
		Stamp plan; 
		Gap interval;
	};

	class Reactor : boost::noncopyable {
	public:
		Reactor();
		~Reactor();

		void run();
		void stop();

		void add(Fd,Event,Callable);
		void del(Fd,Event);
		void del(Fd);
		Events get(Fd);
		EventView get(Fd, Event);
		Fd  plan(Callable, Stamp, Gap);
	protected:
		std::atomic<bool> running_;
		const std::thread::id tid_;
		std::atomic<Iteration> iteration_;
		std::map<Fd, std::map<Event,std::pair<Iteration, Callable>>> fd_event_callable_;
		std::mutex mutex_;
	};
	};
}; // namespace x