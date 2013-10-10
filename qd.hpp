#ifndef qd_hpp
#define qd_hpp qd_hpp

#include<array>
#include<atomic>
#include<future>
#include<thread>
#include<utility>

class tatas_lock {
	std::atomic<bool> locked; /* TODO can std::atomic_flag be used? */
	public:
		bool try_lock() {
			if(locked) return false;
			return !locked.exchange(true);
		}
		void unlock() {
			locked = false;
		}
		bool is_locked() {
			return locked;
		}
		void lock() {
			while(!try_lock()) {
				std::this_thread::yield();
			}
		}
};

template<int ARRAY_SIZE>
class buffer_queue {
	typedef std::atomic<int> sizetype;
	typedef std::function<void(char*)> ftype;
	static const bool CLOSED = false;
	static const bool SUCCESS = true;
	std::atomic<int> counter;
	std::atomic<bool> closed;
	std::array<char, ARRAY_SIZE> buffer_array;

	public:
		void open() {
			counter = 0;
			closed = false;
		}

		template<typename P>
		bool enqueue(std::function<void(char*)> op, std::promise<P> p) {
			if(closed) {
				return CLOSED;
			}
			const int size = sizeof(sizetype) + sizeof(op) + sizeof(p); // TODO pad to next sizeof(sizetype)?
			int index = counter.fetch_add(size);
			if(index+size <= ARRAY_SIZE) {
				auto opptr = reinterpret_cast<decltype(op)*>(&buffer_array[index + sizeof(sizetype)]);
				new (opptr) decltype(op)(std::move(op));
//				opptr->swap(op);
				auto pptr = reinterpret_cast<decltype(p)*>(&buffer_array[index + sizeof(op) + sizeof(sizetype)]);
				new (pptr) decltype(p)(std::move(p));
//				pptr->swap(p);
				*reinterpret_cast<sizetype*>(&buffer_array[index]) = size;
				return SUCCESS;
			} else {
				if(index < ARRAY_SIZE-sizeof(sizetype)) *reinterpret_cast<sizetype*>(&buffer_array[index]) = ARRAY_SIZE+1;
				return CLOSED;
			}
		}

		void flush() {
			int todo = 0;
			bool open = true;
			while(open) {
				int done = todo;
				todo = counter;
				if(todo == done) { /* close queue */
					todo = counter.exchange(ARRAY_SIZE);
					open = false;
					closed = true;
				}
				if(todo >= ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype)) { /* queue closed */
					todo = ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype);
					open = false;
					closed = true;
				}
				int last_size = 0;
				for(int index = done; index < todo; index+=last_size) {
					do {
						last_size = *reinterpret_cast<sizetype*>(&buffer_array[index]);
					} while(!last_size);
					if(last_size == ARRAY_SIZE+1) {
						std::fill(&buffer_array[index], &buffer_array[ARRAY_SIZE], 0);
						return;
					}

					ftype* fun = reinterpret_cast<ftype*>(&buffer_array[index + sizeof(sizetype)]);

					(*fun)(&buffer_array[index + sizeof(sizetype) + sizeof(*fun)]);
					//*fun = nullptr; /* reset */
					fun->~ftype();

					std::fill(&buffer_array[index], &buffer_array[index+last_size], 0);

				}
			}
		}
};

template<typename R, typename... Ps>
void delegated_function(char* pptr, std::function<R(Ps...)> f, Ps... ps) {
	typedef std::promise<R> promise;
	auto pp = reinterpret_cast<promise*>(pptr);
	promise p(std::move(*pp));
	p.set_value(f(ps...));
	pp->~promise();
}

template<typename... Ps>
void vdelegated_function(char* pptr, std::function<void(Ps...)> f, Ps... ps) {
	typedef std::promise<void> promise;
	auto pp = reinterpret_cast<promise*>(pptr);
	promise p(std::move(*pp));
	f(ps...);
	p.set_value();
	pp->~promise();
}

template<class MLock, class DQueue>
class qdlock_impl {
	MLock mutex_lock;
	DQueue delegation_queue;
	public:
		template<typename R, typename... Ps>
		std::future<R> delegate(std::function<R(Ps...)> f, Ps... ps) { /*TODO*/
			while(true) {
				std::promise<R> result;
				auto future = result.get_future();
				if(mutex_lock.try_lock()) {
					delegation_queue.open();
					result.set_value(f(ps...));
					delegation_queue.flush();
					mutex_lock.unlock();
					return future;
				} else {
					std::function<void(char*, std::function<R(Ps...)>, Ps...)> fn = delegated_function<R, Ps...>;
					std::function<void(char*)> d = std::bind(fn, std::placeholders::_1, f, ps...);
					if(delegation_queue.enqueue(d, std::move(result))) {
						return future;
					}
				}
				std::this_thread::yield();
			}
		}
		template<typename... Ps>
		std::future<void> delegate(std::function<void(Ps...)> f, Ps... ps) { /*TODO*/
			while(true) {
				std::promise<void> result;
				auto future = result.get_future();
				if(mutex_lock.try_lock()) {
					delegation_queue.open();
					f(ps...);
					result.set_value();
					delegation_queue.flush();
					mutex_lock.unlock();
					return future;
				} else {
					std::function<void(char*, std::function<void(Ps...)>, Ps...)> fn = vdelegated_function<Ps...>;
					std::function<void(char*)> d = std::bind(fn, std::placeholders::_1, f, ps...);
					if(delegation_queue.enqueue(d, std::move(result))) {
						return future;
					}
				}
				std::this_thread::yield();
			}
		}
};

using qdlock = qdlock_impl<tatas_lock, buffer_queue<16384>>;

#endif
