#!/usr/bin/env python3

"""Generates the tests."""

import itertools

atomic_instr = [
    "atomic_instruction_policy_t::use_fetch_and_add",
    "atomic_instruction_policy_t::use_compare_and_swap"
    ]
starvation = [
    "starvation_policy_t::starvation_free",
    "starvation_policy_t::may_starve"
    ]
locks = [
	"pthreads_lock",
	"tatas_lock",
	"mcs_lock",
	"mutex_lock",
	"futex_lock",
	"mcs_futex_lock",
	"ticket_futex_lock"
	]

buffer_q = ["buffer_queue<{0}>".format(x) for x in [262144, 262139]]
entry_q = ["entry_queue<{0}, {1}>".format(x,y) for (x,y) in itertools.product([4096], [32])]
xy_list = [(16384, 7), (16384, 8), (16000, 7), (16000, 8), (4096, 32), (6144, 24)]
dual_buffer_q = ["dual_buffer_queue<{0}, {1}, {2}>".format(x,y,z)
                 for ((x,y),z) in itertools.product(xy_list, atomic_instr)]

#SQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S)
#       for (L,Q,S) in itertools.product(locks, simple_q, starvation)]
BQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S)
       for (L,Q,S) in itertools.product(locks, buffer_q, starvation)]
EQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S)
       for (L,Q,S) in itertools.product(locks, entry_q, starvation)]
DBQT = ["qdlock_impl<{0}, {1}, {2}>".format(L, Q, S)
        for (L,Q,S) in itertools.product(locks, dual_buffer_q, starvation)]

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


def split_typelists(tlists):
    """Splits a type list"""
    splitpoint = 16
    split_tlists = []
    for (name, types) in tlists:
        i = 0
        while len(types) > splitpoint:
            split_tlists.append(("{0}{1}".format(name, str(i)), types[:splitpoint]))
            types = types[splitpoint:]
            i = i + 1
        if i > 0:
            name = "{0}{1}".format(name, str(i))
        split_tlists.append((name, types))
    return split_tlists

split_locks = split_typelists(lock_typelists)
split_delegate = split_typelists(delegation_typelists)

FILE_LIMIT = 8
for j in range(FILE_LIMIT):
    with open("generated_test{0}.cpp".format(str(j)), 'w') as f:
        print("#include<lock.hpp>", file=f)
        print("using namespace qd;", file=f)
        print("using namespace qd::locks;", file=f)
        print("using namespace qd::queues;", file=f)
        f.close()

CNT = 0
def roundrobin_tests(tests, split_tlists):
    """Writes the tests"""
    global CNT
    for (name, types) in split_tlists:
        fname = "generated_test{0}.cpp".format(str(CNT % FILE_LIMIT))
        with open(fname, 'a') as out_f:
            if len(types) > 50:
                raise "Too many types for a single instance!"
            print("typedef ::testing::Types <", file=out_f)
            print(", \n".join(types), file=out_f)
            print("> GeneratedTypes{0};".format(str(CNT)), file=out_f)
            print("INSTANTIATE_TYPED_TEST_SUITE_P({1}, {0}, GeneratedTypes{2});"
                  .format(tests, name, str(CNT)), file=out_f)
            CNT = CNT + 1
            out_f.close()

roundrobin_tests("LockTest", split_locks)
roundrobin_tests("DelegationTest", split_delegate)
