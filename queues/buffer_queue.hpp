#ifndef qd_buffer_queue_hpp
#define qd_buffer_queue_hpp qd_buffer_queue_hpp

#include<algorithm>
#include<array>
#include<atomic>
#include "util/type_tools.hpp"
#include "padded.hpp"

/**
 * @brief a buffer-based tantrum queue
 * @tparam ARRAY_SIZE the buffer's size in bytes
 */
template<long ARRAY_SIZE>
class buffer_queue {
	/** type for the size field for queue entries, loads must not be optimized away in flush */
	typedef std::atomic<long> sizetype;

	/** type for function pointers to be stored in this queue */
//	typedef std::function<void(char*)> ftype;
	typedef void(*ftype)(char*);

	public:
		/** constants for current state of the queue */
		enum class status : long { OPEN=0, SUCCESS=0, FULL, CLOSED }; 

		buffer_queue() : counter(ARRAY_SIZE), closed(status::CLOSED) {}
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
			closed.store(status::OPEN, std::memory_order_relaxed);
		}

		void forwardall(long) {};
		template<typename P, typename... Ts>
		void forwardall(long offset, P&& p, Ts&&... ts) {
			auto ptr = reinterpret_cast<P*>(&buffer_array[offset]);
			new (ptr) P(std::forward<P>(p));
			forwardall(offset+sizeof(p), std::forward<Ts>(ts)...);
		}
		/**
		 * @brief enqueues an entry
		 * @tparam P return type of associated function
		 * @param op wrapper function for associated function
		 * @return SUCCESS on successful storing in queue, FULL if the queue is full and CLOSED if the queue is closed explicitly
		 */
		template<typename... Ps>
		status enqueue(void (*op)(char*), Ps*... ps) {
			auto current_status = closed.load(std::memory_order_relaxed);
			if(current_status != status::OPEN) {
				return current_status;
			}
			/* entry size = size of size + size of wrapper functor + size of promise + size of all parameters*/
			constexpr long size = aligned(sizeof(sizetype) + sizeof(op) + sumsizes<Ps...>::size);
			/* get memory in buffer */
			long index = counter.fetch_add(size, std::memory_order_relaxed);
			if(index+size <= ARRAY_SIZE) {
				/* enough memory available: move op, p and parameters to buffer, then set size of entry */
				forwardall(index+sizeof(sizetype), std::move(op), std::move(*ps)...);

				reinterpret_cast<sizetype*>(&buffer_array[index])->store(size, std::memory_order_release);
				return status::SUCCESS;
			} else {
				/* not enough memory available: avoid deadlock in flush by setting special value */
				if(index < static_cast<long>(ARRAY_SIZE - sizeof(sizetype))) {
					reinterpret_cast<sizetype*>(&buffer_array[index])->store(ARRAY_SIZE+1, std::memory_order_release); // TODO MEMORDER
				}
					closed.store(status::FULL, std::memory_order_relaxed);
				return status::FULL;
			}
		}

		/** execute all stored operations, leave queue in closed state */
		void flush() {
			long todo = 0;
			bool open = true;
			while(open) {
				long done = todo;
				todo = counter.load(std::memory_order_relaxed);
				if(todo == done) { /* close queue */
					todo = counter.exchange(ARRAY_SIZE, std::memory_order_relaxed);
					open = false;
					closed.store(status::CLOSED, std::memory_order_relaxed);
				}
				if(todo >= static_cast<long>(ARRAY_SIZE)) { /* queue closed */
					todo = ARRAY_SIZE;
					open = false;
					closed.store(status::CLOSED, std::memory_order_relaxed);
				}
				long last_size = 0;
				for(long index = done; index < todo; index+=aligned(last_size)) {
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
//					fun->~ftype();
					std::fill(&buffer_array[index], &buffer_array[index+last_size], 0);
				}
			}
		}
	private:
		/** counter for how much of the buffer is currently in use; offset to free area in buffer_array */
		std::atomic<long> counter;

		/** the buffer for entries to this queue */
		std::array<char, ARRAY_SIZE> buffer_array;
		
		/** optimization flag: no writes when queue in known-closed state */
		std::atomic<status> closed;

		static constexpr long aligned(long size) {
			return ((size + sizeof(sizetype) - 1) / sizeof(sizetype)) * sizeof(sizetype); /* TODO: better way of computing a ceil? */
		}
};

#endif /* qd_buffer_queue_hpp */
