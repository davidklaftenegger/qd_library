#ifndef qd_hpp
#define qd_hpp qd_hpp

#include<array>
#include<atomic>
#include<cassert>
#include<cstddef>
#include<future>
#include<numeric>
#include<thread>
#include<utility>

#include "padded.hpp"
#include "threadid.hpp"

/* TODO */
static void pause();

/** @brief a test-and-test-and-set lock */
class tatas_lock {
	std::atomic<bool> locked; /* TODO can std::atomic_flag be used? */
	public:
		tatas_lock() : locked(false) {};
		tatas_lock(tatas_lock&) = delete; /* TODO? */
		bool try_lock() {
			if(locked.load(std::memory_order_acquire)) return false;
			return !locked.exchange(true, std::memory_order_release);
		}
		void unlock() {
			locked.store(false, std::memory_order_release);
		}
		bool is_locked() {
			return locked.load(std::memory_order_acquire);
		}
		void lock() {
			while(!try_lock()) {
				pause();
			}
		}
};

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

	/* some constants */
	static const bool CLOSED = false;
	static const bool SUCCESS = true;

	/** counter for how much of the buffer is currently in use; offset to free area in buffer_array */
	std::atomic<long> counter;
	/** optimization flag: no writes when queue in known-closed state */
	std::atomic<bool> closed; /*TODO atomic_flag? */
	/** the buffer for entries to this queue */
	std::array<char, ARRAY_SIZE> buffer_array;

	public:
		buffer_queue() : counter(ARRAY_SIZE), closed(true) {}
		/** opens the queue */
		void open() {
			counter.store(0, std::memory_order_relaxed);
			closed.store(false, std::memory_order_relaxed);
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
		 * @param p promise for return value
		 * @return SUCCESS on successful storing in queue, CLOSED otherwise
		 */
		template<typename... Ps>
		bool enqueue(void (*op)(char*), Ps&&... ps) {
			if(closed.load(std::memory_order_relaxed)) { //TODO MEMORDER
				return CLOSED;
			}
			/* entry size = size of size + size of wrapper functor + size of promise + size of all parameters*/
			std::initializer_list<std::size_t> sizeList = {sizeof(Ps)...};
			const long size = sizeof(sizetype) + sizeof(op) + std::accumulate(sizeList.begin(), sizeList.end(), 0); // TODO pad to next sizeof(sizetype)?
			/* get memory in buffer */
			long index = counter.fetch_add(size, std::memory_order_relaxed);
			if(index+size <= ARRAY_SIZE) {
				/* enough memory available: move op, p and parameters to buffer, then set size of entry */
				forwardall(index+sizeof(sizetype), std::move(op), std::move(ps)...);

				reinterpret_cast<sizetype*>(&buffer_array[index])->store(size, std::memory_order_release);
				return SUCCESS;
			} else {
				/* not enough memory available: avoid deadlock in flush by setting special value */
				if(index < static_cast<long>(ARRAY_SIZE - sizeof(sizetype))) {
					reinterpret_cast<sizetype*>(&buffer_array[index])->store(ARRAY_SIZE+1, std::memory_order_release); // TODO MEMORDER
				}
				//std::cout << "UEHIEÜPUBGRIBRIPUBIUBERN\n";
				return CLOSED;
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
					closed.store(true, std::memory_order_relaxed);
				}
				if(todo >= static_cast<long>(ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype))) { /* queue closed */
					todo = ARRAY_SIZE - sizeof(ftype) - sizeof(sizetype);
					open = false;
					closed.store(true, std::memory_order_relaxed);
				}
				long last_size = 0;
				for(long index = done; index < todo; index+=last_size) {
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
};


/* structure to create lists of types */
template<typename... Ts>
class types;
template<typename T, typename... Ts>
class types<T, Ts...> {
	public:
		typedef T type;
		typedef types<Ts...> tail;
};
template<>
class types<> {};

/* unpacking promise from delegation queue */
template<typename R, template<class> class Promise = std::promise>
class unpack_promise : public Promise<R> {
	public:
		unpack_promise(char* pptr) : Promise<R>(std::move(*reinterpret_cast<Promise<R>*>(pptr))) {}
		~unpack_promise() {} /* automatically destructs superclass */
		unpack_promise() = delete; /* meaningless */
		unpack_promise(unpack_promise&) = delete; /* not possible */
		unpack_promise(unpack_promise&&) = delete; /* actually implementable, TODO for now */
};


/* Next is a block of templates for delegated function wrappers.
 * They unpack the promise (using a wrapper) and fulfill it in various ways.
 * These are
 * 1) normal functions
 * 2) member functions and functors
 * A) with the function pointer known to the template (pointer known at compile time)
 * B) with the function pointer specified at runtime (signature known at compile time)
 * a) returning a future of some type
 * b) returning a future of void
 * c) returning void (fire and forget delegation)
 */

/* case 2Aa */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_function_future(char* pptr, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value((o.*f)(std::forward<Ps>(ps)...));
}

/* case 2Ba */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_function_future(char* pptr, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value((o.*f)(std::forward<Ps>(ps)...));
}

/* case 1Aa */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_future(char* pptr, Ps&&... ps)
-> typename std::enable_if<std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value(f(std::forward<Ps>(ps)...));
}

/* case 1Ba */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_function_future(char* pptr, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	unpack_promise<R> p(pptr);
	p.set_value(f(std::forward<Ps>(ps)...));
}

/* cases Aa?, unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_future(char* buf, Ps&&... ps)
-> typename std::enable_if<!std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_future<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/* cases Ba?, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		true
		//std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_future<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

#if 0
/** wrapper function for non-void operations */
/* broken */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function(char* pptr, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	typedef std::promise<R> promise;
	auto pp = reinterpret_cast<promise*>(pptr);
	promise p(std::move(*pp));
	p.set_value(f(std::forward<Ps>(ps)...));
	pp->~promise();
}

/* broken */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		!std::is_same<types<>, Types>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

#endif 
/* case 2Ab */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_void_function_future(char* pptr, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	(o.*f)(std::forward<Ps>(ps)...);
	p.set_value();
}

/* case 2Bb */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_void_function_future(char* pptr, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	(o.*f)(std::forward<Ps>(ps)...);
	p.set_value();
}
/** wrapper function for void operations */
/* 1Ab */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_void_function_future(char* pptr, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	f(ps...);
	p.set_value();
}
/* case 1Bb */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_void_function_future(char* pptr, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	typedef decltype(f(ps...)) R;
	static_assert(std::is_same<R, void>::value, "void code path used for non-void function");
	unpack_promise<R> p(pptr);
	f(std::forward<Ps>(ps)...);
	p.set_value();
}

/* Ab unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_void_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		!std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_void_function_future<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}
/* cases Bb, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_void_function_future(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_void_function_future<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/** wrapper function for operations without associated future */
/* case 2Ac */
template<typename Types, typename Function, Function f, typename O, typename... Ps>
auto delegated_function_nofuture(char*, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& !std::is_same<Function, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	(o.*f)(std::forward<Ps>(ps)...);
}

/* case 2Bc */
template<typename Types, typename Ignored, Ignored i, typename Function, typename O, typename... Ps>
auto delegated_function_nofuture(char*, Function&& f, O&& o, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& std::is_member_function_pointer<Function>::value
	, void>::type
{
	(o.*f)(std::forward<Ps>(ps)...);
}

/* case 1Ac */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_nofuture(char*, Ps&&... ps)
-> typename std::enable_if<std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	f(std::forward<Ps>(ps)...);
}

/* case 1Bc */
template<typename Types, typename Ignored, Ignored i, typename Function, typename... Ps>
auto delegated_function_nofuture(char*, Function&& f, Ps&&... ps)
-> typename
	std::enable_if<
		std::is_same<types<>, Types>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
		&& !std::is_member_function_pointer<Function>::value
	, void>::type
{
	f(std::forward<Ps>(ps)...);
}

/* cases Ac, unrolling */
template<typename Types, typename Function, Function f, typename... Ps>
auto delegated_function_nofuture(char* buf, Ps&&... ps)
-> typename std::enable_if<!std::is_same<types<>, Types>::value && !std::is_same<Function, std::nullptr_t>::value, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_nofuture<typename Types::tail, Function, f>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}

/* cases Bc, unrolling */
template<typename Types, typename Ignored, std::nullptr_t i, typename... Ps>
auto delegated_function_nofuture(char* buf, Ps&&... ps)
-> typename
	std::enable_if<
		true
		//std::is_same<types<>, typename Types::tail>::value
		&& std::is_same<Ignored, std::nullptr_t>::value
	, void>::type
{
	typedef typename Types::type T;
	auto ptr = reinterpret_cast<T*>(buf);
	delegated_function_nofuture<typename Types::tail, Ignored, i>(buf+sizeof(T), std::forward<Ps>(ps)..., std::forward<T>(*ptr));
}



/**
 * @brief queue delegation base class
 * @tparam MLock mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class MLock, class DQueue>
class qdlock_base {
	protected:
		MLock mutex_lock;
		DQueue delegation_queue;
		
		/** executes the operation */
		
		/* case 1Aa */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(Promise r, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& !std::is_same<void, decltype(f(ps...))>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
//			static_assert(std::is_same<Promise, std::promise<typename std::result_of<Function(Ps...)>::type>>::value, "WAT? GIANT RUBBER DUCK!");
//			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			r.set_value(f(std::move(ps)...));
		}
		
		/** alternative for operations with a promise, case member function pointer in template, object specified */
		/* case 2Aa */
		template<typename Function, Function f, typename R, typename O, typename... Ps>
		auto execute(std::promise<typename std::result_of<decltype(&O::Function)(O, Ps...)>::type> r, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			r.set_value((o.*f)(std::move(ps)...));
		}
		
		/** alternative for operations with a promise, case function pointer specified */
		/* case 1Ba */
		template<typename Ignored, Ignored i, typename R, typename Function, typename... Ps>
		auto execute(std::promise<typename std::result_of<Function(Ps...)>::type> r, Function&& f, Ps&&... ps)
		-> typename 
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& !std::is_same<void, typename std::result_of<Function(Ps...)>::type>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			r.set_value(f(std::move(ps)...));
		}
		
		/** alternative for operations witht a promise, case member function pointer and object specified */
		/* case 2Ba */
		template<typename Ignored, Ignored i, typename R, typename Function, typename O, typename... Ps>
		auto execute(std::promise<typename std::result_of<Function(Ps...)>::type> r, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			r.set_value((o.*f)(std::move(ps)...));
		}
		
		/** alternative for operations which return void */
		/* case 1Ab */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(std::promise<void> r, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_same<void, decltype(f(ps...))>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
			f(std::move(ps)...);
			r.set_value();
		}
		
		/** alternative for operations with a promise, case member function pointer in template, object specified */
		/* case 2Ab */
		template<typename Function, Function f, typename R, typename O, typename... Ps>
		auto execute(std::promise<void> r, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			(o.*f)(std::move(ps)...);
			r.set_value();
		}
		
		/** alternative for operations with a promise, case function pointer specified */
		/* case 1Bb */
		template<typename Ignored, Ignored i, typename R, typename Function, typename... Ps>
		auto execute(std::promise<void> r, Function&& f, Ps&&... ps)
		-> typename std::enable_if<std::is_same<Ignored, std::nullptr_t>::value, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			f(std::move(ps)...);
			r.set_value();
		}
		
		/** alternative for operations witht a promise, case member function pointer and object specified */
		/* case 2Bb */
		template<typename Ignored, Ignored i, typename R, typename Function, typename O, typename... Ps>
		auto execute(std::promise<void> r, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
		//	static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			(o.*f)(std::move(ps)...);
			r.set_value();
		}
		
		/** alternative for operations without a promise, case function pointer in template */
		/* case 1Ac */
		template<typename Function, Function f, typename Promise, typename... Ps>
		auto execute(std::nullptr_t, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_function<typename std::remove_pointer<Function>::type>::value
			, void>::type
		{
			f(std::move(ps)...);
		}

		/** alternative for operations without a promise, case member function pointer in template, object specified */
		/* case 2Ac */
		template<typename Function, Function f, typename Promise, typename O, typename... Ps>
		auto execute(std::nullptr_t, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
//				&& std::is_same< decltype(O::Function(ps...)), decltype(o.*f(ps...))>::value
			, void>::type
		{
			(o.*f)(std::move(ps)...);
		}
		
		/** alternative for operations without a promise, case function pointer specified */
		/* case 1Bc */
		template<typename Ignored, Ignored i, typename Promise, typename Function, typename... Ps>
		auto execute(std::nullptr_t, Function&& f, Ps&&... ps)
		-> typename std::enable_if<std::is_same<Ignored, std::nullptr_t>::value, void>::type
		{
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			f(std::move(ps)...);
		}
		
		/** alternative for operations without a promise, case member function pointer and object specified */
		/* case 2Bc */
		template<typename Ignored, Ignored i, typename Promise, typename Function, typename O, typename... Ps>
		auto execute(std::nullptr_t, Function&& f, O&& o, Ps&&... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& std::is_member_function_pointer<Function>::value
			, void>::type
		{
			static_assert(std::is_same<Ignored, std::nullptr_t>::value, "functors cannot be used when specifying a function");
			(o.*f)(std::move(ps)...);
		}
		
		
		/* ENQUEUE IMPLEMENTATIONS */
		
		/** maybe enqueues the operation */
		/* case Aa */
		template<typename Function, Function f, typename R, typename... Ps>
		auto enqueue(std::promise<R> r, Ps... ps)
		-> typename
			std::enable_if<
				!std::is_same<Function, std::nullptr_t>::value
				&& !std::is_same<R, void>::value
			, bool>::type
		{
			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_function_future<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, std::move(ps)..., std::move(r));
		}
		
		/** alternative with returning a result, case function specified as argument */
		/* case Ba */
		template<typename Ignored, std::nullptr_t i, typename R, typename Function, typename... Ps>
		auto enqueue(std::promise<R> r, Function f, Ps... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
				&& !std::is_same<R, void>::value
			, bool>::type
		{
			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_function_future<types<Function, Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, std::move(f), std::move(ps...), std::move(r));
		}

		/** alternative for operations which return void */
		/* case Ab */
		template<typename Function, Function f, typename... Ps>
		auto enqueue(std::promise<void> r, Ps... ps)
		-> bool {
			void (*d)(char*) = delegated_void_function_future<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, ps..., std::move(r));
		}
		/** alternative with returning a void, case function specified as argument */
		/* case Bb */
		template<typename Ignored, std::nullptr_t i, typename R, typename... Ps>
		auto enqueue(std::promise<void> r, Ps... ps)
		-> typename
			std::enable_if<
				std::is_same<Ignored, std::nullptr_t>::value
			, bool>::type
		{
			static_assert(std::is_same<R, decltype(f(ps...))>::value, "promise and function have different return types");
			void (*d)(char*) = delegated_void_function_future<types<Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, std::move(ps...), std::move(r));
		}

		
		/** alternative without returning a result, case function specified in template */
		/* case Ac */
		template<typename Function, Function f, typename... Ps>
		auto enqueue(std::nullptr_t, Ps... ps)
		-> typename std::enable_if<!std::is_same<Function, std::nullptr_t>::value, bool>::type
		{
			void (*d)(char*) = delegated_function_nofuture<types<Ps...>, Function, f>;
			return delegation_queue.enqueue(d, ps...);
		}
		
		/** alternative without returning a result, case function specified as argument */
		/* case Bc */
		template<typename Ignored, std::nullptr_t i, typename... Ps>
		auto enqueue(std::nullptr_t, Ps... ps)
		-> bool
		{
			void (*d)(char*) = delegated_function_nofuture<types<Ps...>, std::nullptr_t, nullptr>;
			return delegation_queue.enqueue(d, ps...);
		}
		struct no_promise {
			typedef std::nullptr_t promise;
			typedef std::nullptr_t future;
			static promise create_promise() {
				return nullptr;
			}
			static future create_future(promise) {
				return nullptr;
			}
		};

		struct no_reader_sync {
			static void wait_writers(qdlock_base<MLock, DQueue>*) {};
			static void wait_readers(qdlock_base<MLock, DQueue>*) {};
		};
		template<typename R>
		struct std_promise {
			typedef std::promise<R> promise;
			typedef std::future<R> future;
			static promise create_promise() {
				return promise();
			}
			static future create_future(promise& p) {
				return p.get_future();
			}
		};

		//-> typename std::conditional<std::is_same<std::nullptr_t, typename Promise::promise>::value, void, typename Promise::future>::type
		template<typename Function, Function f, typename Promise, typename RSync, typename... Ps>
		auto delegate(Ps&&... ps)
		-> typename Promise::future
		{
			RSync::wait_writers(this);
			while(true) {
				auto result = Promise::create_promise();
				auto future = Promise::create_future(result);
				if(this->mutex_lock.try_lock()) {
					this->delegation_queue.open();
					RSync::wait_readers(this);
					execute<Function, f, typename Promise::promise, Ps...>(std::move(result), std::forward<Ps>(ps)...);
					this->delegation_queue.flush();
					this->mutex_lock.unlock();
					return future;
				} else {
					if(enqueue<Function, f>(std::move(result), ps...)) {
						return future;
					}
				}
				pause();
			}
		}


};


/**
 * @brief queue delegation lock implementation
 * @tparam MLock mutual exclusion lock
 * @tparam DQueue delegation queue
 */
template<class MLock, class DQueue>
class qdlock_impl : private qdlock_base<MLock, DQueue> {
	typedef qdlock_base<MLock, DQueue> base;
	public:
		/**
		 * @brief delegate function
		 * @tparam R return type of delegated operation
		 * @tparam Ps parameter types of delegated operation
		 * @param f the delegated operation
		 * @param ps the parameters for the delegated operation
		 * @return a future for return value of delegated operation
		 */
#if 0
		template<typename R, typename... Ps>
		std::future<R> delegate(R (*f)(Ps...), Ps... ps) {
			while(true) {
				std::promise<R> result;
				auto future = result.get_future();
				if(this->mutex_lock.try_lock()) {
					this->delegation_queue.open();
					this->execute(std::move(result), f, ps...);
					this->delegation_queue.flush();
					this->mutex_lock.unlock();
					return future;
				} else {
					if(this->enqueue(std::move(result), f, ps...)) {
						return future;
					}
				}
				asm("pause");
				//std::this_thread::yield();
			}
		}
#endif
		template<typename Function, Function f, typename... Ps>
		void delegate_n(Ps&&... ps) {
			/* template provides function address */
			base::template delegate<Function, f, typename base::no_promise, typename base::no_reader_sync, Ps...>(std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			base::template delegate<std::nullptr_t, nullptr, typename base::no_promise, typename base::no_reader_sync, Function, Ps...>(std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
		template<typename Function, Function f, typename... Ps>
		auto delegate_f(Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			using return_t = decltype(f(ps...));
			return base::template delegate<Function, f, typename base::template std_promise<return_t>, typename base::no_reader_sync, Ps...>(std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		auto delegate_f(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using return_t = decltype(f(ps...));
			return base::template delegate<std::nullptr_t, nullptr, typename base::template std_promise<return_t>, typename base::no_reader_sync, Function, Ps...>(std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
		

		void lock() {
			this->mutex_lock.lock();
		}
		void unlock() {
			this->mutex_lock.unlock();
		}
};

template<int GROUPS>
class reader_groups {
	std::array<std::atomic<int>, GROUPS> counters;
	public:
		reader_groups() {
			for(int i = 0; i < GROUPS; i++) {
				counters[i] = 0;
			}
		}
		bool query() {
			for(std::atomic<int>& counter : counters)
				if(counter.load() > 0) return true;
			return false;
		}
		void arrive() {
			counters[thread_id % GROUPS] += 1;
		}
		void depart() {
			counters[thread_id % GROUPS] -= 1;
		}
};

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
		mrqdlock_impl() : writeBarrier(0) {} // TODO proper comment. YES THIS NEEDS TO BE INITIALIZED
		/**
		 * @brief delegate function
		 * @tparam R return type of delegated operation
		 * @tparam Ps parameter types of delegated operation
		 * @param f the delegated operation
		 * @param ps the parameters for the delegated operation
		 * @return a future for return value of delegated operation
		 */
		
		template<typename Function, Function f, typename... Ps>
		void delegate_n(Ps&&... ps) {
			/* template provides function address */
			base::template delegate<Function, f, typename base::no_promise, reader_indicator_sync, Ps...>(std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		void delegate_n(Function&& f, Ps&&... ps) {
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			base::template delegate<std::nullptr_t, nullptr, typename base::no_promise, reader_indicator_sync, Function, Ps...>(std::forward<Function>(f), std::forward<Ps>(ps)...);
		}
		template<typename Function, Function f, typename... Ps>
		auto delegate_f(Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			using return_t = decltype(f(ps...));
			return base::template delegate<Function, f, typename base::template std_promise<return_t>, reader_indicator_sync, Ps...>(std::forward<Ps>(ps)...);
		}
		template<typename Function, typename... Ps>
		auto delegate_f(Function&& f, Ps&&... ps)
		-> typename base::template std_promise<decltype(f(ps...))>::future
		{
			/* type of functor/function ptr stored in f, set template function pointer to NULL */
			using return_t = decltype(f(ps...));
			return base::template delegate<std::nullptr_t, nullptr, typename base::template std_promise<return_t>, reader_indicator_sync, Function, Ps...>(std::forward<Function>(f), std::forward<Ps>(ps)...);
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
static void pause() {
	asm("pause");
//	std::this_thread::yield();
}

using qdlock = qdlock_impl<tatas_lock, buffer_queue<16384>>;
using mrqdlock = mrqdlock_impl<tatas_lock, buffer_queue<16384>, reader_groups<64>, 65536>;

#define DELEGATE_F(function, ...) template delegate_f<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_N(function, ...) template delegate_n<decltype(function), function>(__VA_ARGS__)

#endif
