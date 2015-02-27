#ifndef qd_futex_lock_hpp
#define qd_futex_lock_hpp qd_futex_lock_hpp

#include<atomic>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

/**
 * @brief a futex based lock
 * @details this lock can directly be waited on, but wakeup
 *          does NOT take the lock
 * @note lock() and unlock() are taken from mutex in http://www.akkadia.org/drepper/futex.pdf with additional documentation used from http://locklessinc.com/articles/mutex_cv_futex/
 */
class futex_lock {
	using field_t = int;
	std::atomic<field_t> locked;
	enum { free, taken, contended };
	public:
		futex_lock() : locked(0) {}
		futex_lock(futex_lock&) = delete; /* TODO? */
		
		bool try_lock(int tries) {
			field_t c = free;
			if(is_locked()) {
				bool do_wait = tries%128 == 0;
				return do_wait?wait():false;
			}
			if(locked.compare_exchange_strong(c, taken, std::memory_order_release)) {
				return true;
			} else {
				/* no waiting here: lock was just acquired by another thread */
				return false;
			}
		}

		bool try_lock2() {
			field_t c = free;
			if(is_locked()) {
				return wait();
			}
			if(locked.compare_exchange_strong(c, taken, std::memory_order_release)) {
				return true;
			} else {
				return wait();
			}
		}
		void unlock() {
			auto old = locked.exchange(free, std::memory_order_release);
			if(old == contended) {
				notify_all(); // notify_one instead?
			}
		}

		bool is_locked() {
			return locked.load(std::memory_order_acquire) != free;
		}

		void lock() {
			field_t c = free;
			if(locked.compare_exchange_strong(c, taken)) {
				if(c != contended) {
					c = locked.exchange(contended);
				}
				while(c != free) {
					wait();
					c = locked.exchange(contended);
				}
			}
		}

		bool wait() {
			if(locked.load(std::memory_order_acquire) != contended) {
				field_t c = locked.exchange(contended, std::memory_order_release);
				if(c == free) {
					return true;
				}
			}
			sys_futex(&locked, FUTEX_WAIT, contended, NULL, NULL, 0);
			return false;
		}

		void notify_one() {
			sys_futex(&locked, FUTEX_WAKE, 1, NULL, NULL, 0);
		}

		void notify_all() {
			sys_futex(&locked, FUTEX_WAKE, 65000, NULL, NULL, 0);
		}
	private:

		static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
		{
			return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
		}
		static_assert(sizeof(locked) == sizeof(field_t), "std::atomic<field_t> size differs from size of field_t. Your architecture does not support the futex_lock");
};

#endif /* qd_futex_lock_hpp */
