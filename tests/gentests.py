#! /usr/bin/env python3

import itertools

atomic_instr = ["atomic_instruction_policy_t::use_fetch_and_add", "atomic_instruction_policy_t::use_compare_and_swap"]

starvation = ["starvation_policy_t::starvation_free", "starvation_policy_t::may_starve"]
locks = [
	"extended_lock<pthreads_lock>",
	"extended_lock<tatas_lock>",
	"mcs_lock",
	"extended_lock<mutex_lock>",
	"futex_lock",
	"mcs_futex_lock",
	"ticket_futex_lock"
	]

buffer_q = ["buffer_queue<{0}>".format(x) for x in [262144, 262139]]
entry_q = ["entry_queue<{0}, {1}>".format(x,y) for (x,y) in itertools.product([4096], [32])]
dual_buffer_q = ["dual_buffer_queue<{0}, {1}, {2}>".format(x,y,z) for ((x,y),z) in itertools.product([(16384, 7), (16384, 8), (16000, 7), (16000, 8), (4096, 32), (6144, 24)], atomic_instr)]

#queues = ["simple_locked_queue"]
queues = []
queues.extend(buffer_q)
queues.extend(entry_q)
queues.extend(dual_buffer_q)


#SQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S) for (L,Q,S) in itertools.product(locks, ["simple_locked_queue"], starvation)]
BQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S) for (L,Q,S) in itertools.product(locks, buffer_q, starvation)]
EQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S) for (L,Q,S) in itertools.product(locks, entry_q, starvation)]
DBQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S) for (L,Q,S) in itertools.product(locks, dual_buffer_q, starvation)]

#for q in queues:
#    print("{0}, \\".format(q))

#print("#define QDTypes \\\nqdlock, \\")
#print(", \\\n".join(qd_impls))

lock_typelists = [
	("StandardLocks", locks),
#	("SimpleQueue", SQT),
	("BufferQueue", BQT), 
	("EntryQueue", EQT), 
	("DualBufferQueue", DBQT) 
	]
delegation_typelists = [
#	("SimpleQueue", SQT),
	("BufferQueue", BQT), 
	("EntryQueue", EQT), 
	("DualBufferQueue", DBQT) 
	]

def split_typelists(typelists): 
	splitpoint = 16
	split_typelists = []
	for (name, types) in typelists:
		i = 0
		while len(types) > splitpoint:
			split_typelists.append(("{0}{1}".format(name, str(i)), types[:splitpoint]))
			types = types[splitpoint:]
			i = i + 1
		if i > 0:
			name = "{0}{1}".format(name, str(i))
		split_typelists.append((name, types))
	return split_typelists


split_locks = split_typelists(lock_typelists)
split_delegate = split_typelists(delegation_typelists)

filelimit = 8
cnt = 0
for i in range(filelimit):
	fname = "generated_test{0}.cpp".format(str(i))
	f = open(fname, 'w');
	print("#include<lock.hpp>", file=f)
	f.close()

def roundrobin_tests(tests, split_typelists):
	global cnt
	for (name, types) in split_typelists:
		fname = "generated_test{0}.cpp".format(str(cnt % filelimit))
		f = open(fname, 'a');
		if len(types) > 50:
			raise "Too many types for a single instance!"
		print("typedef ::testing::Types <", file=f)
		print(", \n".join(types), file=f)
		print("> GeneratedTypes{0};".format(str(cnt)), file=f)
		print("INSTANTIATE_TYPED_TEST_CASE_P({1}, {0}, GeneratedTypes{2});".format(tests, name, str(cnt)), file=f)
		cnt = cnt + 1
		f.close()

roundrobin_tests("LockTest", split_locks)
roundrobin_tests("DelegationTest", split_delegate)
