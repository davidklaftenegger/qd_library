#ifndef qd_mrqdlock_hpp
#define qd_mrqdlock_hpp qd_mrqdlock_hpp

#include<array>
#include<atomic>
#include "readindicator/reader_groups.hpp"
#include "util/pause.hpp"
#include "qdlock_base.hpp"

template<class MLock, class DQueue, class RIndicator, int READ_PATIENCE_LIMIT>
class mrqdlock_impl : private qdlock_base<MLock, DQueue> {
	std::atomic<int> writeBarrier;
	RIndicator reader_indicator;
	typedef qdlock_base<MLock, DQueue> base;
	typedef mrqdlock_impl<MLock, DQueue, RIndicator, READ_PATIENCE_LIMIT>* this_t;
	struct reader_indicator_sync {
		static void wait_writers(base* t) {
			while(static_cast<this_t>(t)->writeBarrier.load() > 0) {
				pause();
			}
		}
		static void wait_readers(base* t) {
			while(static_cast<this_t>(t)->reader_indicator.query()) {
				pause();
			}
		}
	};
	public:
		typedef RIndicator reader_indicator_t;
		mrqdlock_impl() : writeBarrier(0) {} // TODO proper comment. YES THIS NEEDS TO BE INITIALIZED
#if 0
		/**
		 * @brief delegate function
		 * @tparam R return type of delegated operation
		 * @tparam Ps parameter types of delegated operation
		 * @param f the delegated operation
		 * @param ps the parameters for the delegated operation
		 * @return a future for return value of delegated operation
		 */
#endif

		/* the following delegate_XX functions are all specified twice:
		 * First for a templated version, where a function is explicitly
		 * given in the template argument list. (Porbably using a macro)
		 * Second for a version where the function or functor is a
		 * normal parameter to the delegate_XX function.
		 *
		 * XX specifies how futures are dealt with:
		 * n  - no futures
		 * f  - returns future for return value of delegated operation
		 * p  - delegated operation returns void and takes a promise as
		 *      first argument, which is passed here by the user.
		 * fp - returns future for a type which is specified in the
		 *      template parameter list. The delegated operation takes
		 *      a promise for that type as its first argument.
		 */

		/* interface _n functions: Do not wait, do not provide a future. */
		template<typename Function, Function f, typename... Ps>
		void delegate_n(Ps&&... ps) {
			/* template provides function address */
			using promise_t = typename base::no_promise::promise;
			base::template delegate<Function, f, promise_t, reader_indicator_sync, Ps...>(nullptr, std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_t = typename base::no_promise::promise;
			base::template delegate<std::nullptr_t, nullptr, promise_t, reader_indicator_sync, Function, Ps...>(nullptr, std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
		template<typename Function, Function f, typename... Ps>
		auto delegate_f(Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<Function, f, promise_t, reader_indicator_sync, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		
		/* interface _f functions: Provide a future for the return value of the delegated operation */
		template<typename Function, typename... Ps>
		auto delegate_f(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using return_t = decltype(f(ps...));
			using promise_factory = typename base::template std_promise<return_t>;
			using promise_t = typename promise_factory::promise;;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<std::nullptr_t, nullptr, promise_t, reader_indicator_sync, Function, Ps...>(std::move(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
			return future;
		}
		
		/* interface _p functions: User provides a promise, which is used explicitly by the delegated (void) function */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto delegate_p(Promise&& result, Ps&&... ps)
		-> void
		{
			base::template delegate<Function, f, Promise, reader_indicator_sync, Ps...>(std::forward<Promise>(result), std::forward<Ps>(ps)...);
		}
		template<typename Function, typename Promise, typename... Ps>
		auto delegate_p(Function&& f, Promise&& result, Ps&&... ps)
		-> void
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			base::template delegate<std::nullptr_t, nullptr, Promise, reader_indicator_sync, Function, Ps...>(std::forward<Promise>(result), std::forward<Function>(f), std::forward<Ps>(ps)...);
		}

		/* interface _fp functions: Promise is generated here, but delegated function uses it explicitly. */
		template<typename Return, typename Function, Function f, typename... Ps>
		auto delegate_fp(Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename base::no_promise::promise;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<Function, f, promise_t, reader_indicator_sync, Ps...>(std::move(result), std::forward<Ps>(ps)...);
			return future;
		}
		template<typename Return, typename Function, typename... Ps>
		auto delegate_fp(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<Return>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using promise_factory = typename base::template std_promise<Return>;
			using promise_t = typename base::no_promise::promise;
			auto result = promise_factory::create_promise();
			auto future = promise_factory::create_future(result);
			base::template delegate<std::nullptr_t, nullptr, promise_t, reader_indicator_sync, Function, Ps...>(nullptr, std::forward<Function>(f), std::move(result), std::forward<Ps>(ps)...);
			return future;
		}

		void lock() {
			while(writeBarrier.load() > 0) {
				pause();
			}
			this->mutex_lock.lock();
			this->delegation_queue.open();
			while(reader_indicator.query()) {
				pause();
			}
		}
		void unlock() {
			this->delegation_queue.flush();
			this->mutex_lock.unlock();
		}

		void rlock() {
			bool bRaised = false;
			int readPatience = 0;
start:
			reader_indicator.arrive();
			if(this->mutex_lock.is_locked()) {
				reader_indicator.depart();
				while(this->mutex_lock.is_locked()) {
					pause();
					if((readPatience == READ_PATIENCE_LIMIT) && !bRaised) {
						writeBarrier.fetch_add(1);
						bRaised = true;
					}
					readPatience += 1;
				}
				goto start;
			}
			if(bRaised) writeBarrier.fetch_sub(1);
		}
		void runlock() {
			reader_indicator.depart();
		}

};

#endif /* qd_mrqdlock_hpp */
