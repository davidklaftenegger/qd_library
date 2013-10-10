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
		tatas_lock() : locked(false) {};
		tatas_lock(tatas_lock&) = delete; /* TODO? */
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

/**
 * @brief a buffer-based tantrum queue
 * @tparam ARRAY_SIZE the buffer's size in bytes
 */
template<int ARRAY_SIZE>
class buffer_queue {
	/** type for the size field for queue entries, loads must not be optimized away in flush */
	typedef std::atomic<int> sizetype;
	
	/** type for function pointers to be stored in this queue */
	typedef std::function<void(char*)> ftype;

	/* some constants */
	static const bool CLOSED = false;
	static const bool SUCCESS = true;

	/** counter for how much of the buffer is currently in use; offset to free area in buffer_array */
	std::atomic<int> counter;
	/** optimization flag: no writes when queue in known-closed state */
	std::atomic<bool> closed; /*TODO atomic_flag? */
	/** the buffer for entries to this queue */
	std::array<char, ARRAY_SIZE> buffer_array;

	public:
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
			closed.store(false, std::memory_order_relaxed);
		}

		/**
		 * @brief enqueues an entry
		 * @tparam P return type of associated function
		 * @param op wrapper function for associated function
		 * @param p promise for return value
		 * @return SUCCESS on successful storing in queue, CLOSED otherwise
		 */
		template<typename P>
		bool enqueue(std::function<void(char*)> op, std::promise<P> p) {
			if(closed.load()) { //TODO MEMORDER
				return CLOSED;
			}
			/* entry size = size of size + size of wrapper functor + size of promise */
			const int size = sizeof(sizetype) + sizeof(op) + sizeof(p); // TODO pad to next sizeof(sizetype)?
			/* get memory in buffer */
			int index = counter.fetch_add(size, std::memory_order_relaxed);
			if(index+size <= ARRAY_SIZE) {
				/* enough memory available: move op and p to buffer, then set size of entry */
				auto opptr = reinterpret_cast<decltype(op)*>(&buffer_array[index + sizeof(sizetype)]);
				new (opptr) decltype(op)(std::move(op));
				auto pptr = reinterpret_cast<decltype(p)*>(&buffer_array[index + sizeof(op) + sizeof(sizetype)]);
				new (pptr) decltype(p)(std::move(p));
//				opptr->swap(op);
//				pptr->swap(p);
				reinterpret_cast<sizetype*>(&buffer_array[index])->store(size, std::memory_order_release);
				return SUCCESS;
			} else {
				/* not enough memory available: avoid deadlock in flush by setting special value */
				if(index < ARRAY_SIZE-sizeof(sizetype)) reinterpret_cast<sizetype*>(&buffer_array[index])->store(ARRAY_SIZE+1, std::memory_order_release); // TODO MEMORDER
				return CLOSED;
			}
		}

		/** execute all stored operations, leave queue in closed state */
		void flush() {
			int todo = 0;
			bool open = true;
			while(open) {
				int done = todo;
				todo = counter.load(std::memory_order_relaxed);
				if(todo == done) { /* close queue */
					todo = counter.exchange(ARRAY_SIZE, std::memory_order_relaxed);
					open = false;
					closed.store(true, std::memory_order_relaxed);
				}
				if(todo >= ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype)) { /* queue closed */
					todo = ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype);
					open = false;
					closed.store(true, std::memory_order_relaxed);
				}
				int last_size = 0;
				for(int index = done; index < todo; index+=last_size) {
					/* synchronization on entry size field: 0 until entry available */
					do {
						last_size = reinterpret_cast<sizetype*>(&buffer_array[index])->load(std::memory_order_acquire);
					} while(!last_size);

					/* buffer full signal: everything done */
					if(last_size == ARRAY_SIZE+1) {
						std::fill(&buffer_array[index], &buffer_array[ARRAY_SIZE], 0);
						return;
					}

					/* get functor from buffer */ // TODO: move-construct?
					ftype* fun = reinterpret_cast<ftype*>(&buffer_array[index + sizeof(sizetype)]);

					/* call functor with pointer to promise (of unknown type) */
					(*fun)(&buffer_array[index + sizeof(sizetype) + sizeof(*fun)]);

					/* cleanup: call destructor of (now empty) functor and clear buffer area */
					fun->~ftype();
					std::fill(&buffer_array[index], &buffer_array[index+last_size], 0);
				}
			}
		}
};

/** wrapper function for non-void operations */
template<typename R, typename... Ps>
void delegated_function(char* pptr, std::function<R(Ps...)> f, Ps... ps) {
	typedef std::promise<R> promise;
	auto pp = reinterpret_cast<promise*>(pptr);
	promise p(std::move(*pp));
	p.set_value(f(ps...));
	pp->~promise();
}

/** wrapper function for void operations */
template<typename... Ps>
void delegated_void_function(char* pptr, std::function<void(Ps...)> f, Ps... ps) {
	typedef std::promise<void> promise;
	auto pp = reinterpret_cast<promise*>(pptr);
	promise p(std::move(*pp));
	f(ps...);
	p.set_value();
	pp->~promise();
}

/**
 * @brief queue delegation lock implementation
 * @tparam MLock mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class MLock, class DQueue>
class qdlock_impl {
	MLock mutex_lock;
	DQueue delegation_queue;
	public:
		/**
		 * @brief delegate function
		 * @tparam R return type of delegated operation
		 * @tparam Ps parameter types of delegated operation
		 * @param f the delegated operation
		 * @param ps the parameters for the delegated operation
		 * @return a future for return value of delegated operation
		 */
		template<typename R, typename... Ps>
		std::future<R> delegate(std::function<R(Ps...)> f, Ps... ps) {
			while(true) {
				std::promise<R> result;
				auto future = result.get_future();
				if(mutex_lock.try_lock()) {
					delegation_queue.open();
					execute(std::move(result), f, ps...);
					delegation_queue.flush();
					mutex_lock.unlock();
					return future;
				} else {
					if(enqueue(std::move(result), f, ps...)) {
						return future;
					}
				}
				std::this_thread::yield();
			}
		}
	private:
		/** executes the operation */
		template<typename R, typename... Ps>
		void execute(std::promise<R> r, std::function<R(Ps...)> f, Ps... ps) {
			r.set_value(f(ps...));
		}
		/** alternative for operations which return void */
		template<typename... Ps>
		void execute(std::promise<void> r, std::function<void(Ps...)> f, Ps... ps) {
			f(ps...);
			r.set_value();
		}
		/** maybe enqueues the operation */
		template<typename R, typename... Ps>
		bool enqueue(std::promise<R> r, std::function<R(Ps...)> f, Ps... ps) {
			std::function<void(char*, std::function<R(Ps...)>, Ps...)> fn = delegated_function<R, Ps...>;
			std::function<void(char*)> d = std::bind(fn, std::placeholders::_1, f, ps...);
			return delegation_queue.enqueue(d, std::move(r));
		}
		/** alternative for operations which return void */
		template<typename... Ps>
		bool enqueue(std::promise<void> r, std::function<void(Ps...)> f, Ps... ps) {
			std::function<void(char*, std::function<void(Ps...)>, Ps...)> fn = delegated_void_function<Ps...>;
			std::function<void(char*)> d = std::bind(fn, std::placeholders::_1, f, ps...);
			return delegation_queue.enqueue(d, std::move(r));
		}
};

using qdlock = qdlock_impl<tatas_lock, buffer_queue<16384>>;

#endif
