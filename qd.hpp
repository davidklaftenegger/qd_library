#ifndef qd_qd_hpp
#define qd_qd_hpp qd_qd_hpp

#include "locks/waitable_lock.hpp"
#include "locks/pthreads_lock.hpp"
#include "locks/tatas_lock.hpp"
#include "locks/mutex_lock.hpp"
#include "locks/futex_lock.hpp"
#include "locks/mcs_futex_lock.hpp"
#include "locks/mcs_lock.hpp"
#include "locks/ticket_futex_lock.hpp"

#include "queues/buffer_queue.hpp"
#include "queues/dual_buffer_queue.hpp"
#include "queues/entry_queue.hpp"
#include "queues/simple_locked_queue.hpp"

#include "qdlock.hpp"
#include "hqdlock.hpp"
#include "mrqdlock.hpp"

#include "qd_condition_variable.hpp"

namespace qd {
	using internal_lock = qd::locks::mcs_futex_lock;
	using qdlock = qdlock_impl<qd::locks::mcs_futex_lock, qd::queues::dual_buffer_queue<6144, 24, qd::queues::atomic_instruction_policy_t::use_fetch_and_add>, starvation_policy_t::starvation_free>;
	using mrqdlock = mrqdlock_impl<internal_lock, qd::queues::dual_buffer_queue<6144,24>, reader_groups<64>, 65536>;
	//using qd_condition_variable = qd_condition_variable_impl<mutex_lock, simple_locked_queue>;
} // namespace qd

#define DELEGATE_F(function, ...) template delegate_f<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_N(function, ...) template delegate_n<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_P(function, ...) template delegate_p<decltype(function), function>(__VA_ARGS__)
#define DELEGATE_FP(function, ...) template delegate_fp<decltype(function), function>(__VA_ARGS__)
#define WAIT_REDELEGATE_P(function, ...) template wait_redelegate_p<decltype(function), function>(__VA_ARGS__)

using qdlock = qd::qdlock;
using mrqdlock = qd::mrqdlock;

#endif /* qd_qd_hpp */
