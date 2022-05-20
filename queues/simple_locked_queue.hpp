#ifndef qd_simple_locked_queue_hpp
#define qd_simple_locked_queue_hpp qd_simple_locked_queue_hpp

#include<array>
#include<cassert>
#include<mutex>
#include<queue>

#include "queues.hpp"

namespace qd {
	namespace queues {

		/**
		 * @brief a simple lock-based queue
		 * @note this tantrum queue does not limit the number of entries,
		 *       so it cannot provide starvation freedom guarantees
		 */
		class simple_locked_queue {
			/** @brief internal lock to protect the queue */
			std::mutex lock;

			/** @brief internal queue */
			std::queue<std::array<char, 128>> queue;

			/** @brief current open/closed state */
			status state = status::CLOSED;

			/** @brief convenience type alias for managing the lock */
			typedef std::lock_guard<std::mutex> scoped_guard;

			void forwardall(char*, long i) {
				assert(i <= 120);
				if(i > 120) throw "up";
			};
			template<typename P, typename... Ts>
			void forwardall(char* buffer, long offset, P&& p, Ts&&... ts) {
				assert(offset <= 120);
				auto ptr = reinterpret_cast<P*>(&buffer[offset]);
				new (ptr) P(std::forward<P>(p));
				forwardall(buffer, offset+sizeof(p), std::forward<Ts>(ts)...);
			}
			public:
				void open() {
					scoped_guard l(lock);
					state = status::OPEN;
				}

				/**
				 * @brief enqueues an entry
				 * @tparam P return type of associated function
				 * @param op wrapper function for associated function
				 * @return SUCCESS on successful storing in queue, which always succeeds.
				 */
				template<typename... Ps>
				status enqueue(ftype op, Ps*... ps) {

					std::array<char, 128> val;
					scoped_guard l(lock);
					if(state == status::CLOSED) {
						return status::CLOSED;
					}
					queue.push(val);
					forwardall(queue.back().data(), 0, std::move(op), std::move(*ps)...);
					return status::SUCCESS;
				}

				/** execute all stored operations */
				void flush() {
					scoped_guard l(lock);
					state = status::CLOSED;
					while(!queue.empty()) {
						auto operation = queue.front();
						char* ptr = operation.data();
						ftype* fun = reinterpret_cast<ftype*>(ptr);
						ptr += sizeof(ftype*);
						(*fun)(ptr);
						queue.pop();
					}
				}
				/** execute one stored operation */
				void flush_one() {
					scoped_guard l(lock);
					if(!queue.empty()) {
						char* ptr = queue.front().data();
						ftype* fun = reinterpret_cast<ftype*>(ptr);
						ptr += sizeof(ftype);
						(*fun)(ptr);
						queue.pop();
					}
				}
		};
	} // namespace queues
} // namespace qd

#endif /* qd_simple_locked_queue_hpp */
