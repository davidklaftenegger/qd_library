/**
 * @file
 * @brief This file provides simple unit tests for the QD locks
 */

#include <thread>

#include "qd.hpp"
#include "gtest/gtest.h"

/**
 * @brief Class to allow instantiating Tests for multiple implementation types.
 */
template <typename Lock>
class LockTestBase : public testing::Test {

protected:
	LockTestBase() {
	}

	~LockTestBase() {
	}

	Lock lock;
};

template <typename Lock>
class LockTest : public LockTestBase<Lock> {};

template <typename DelegationLock>
class DelegationTest : public LockTestBase<DelegationLock> {};

TYPED_TEST_CASE_P(LockTest);
TYPED_TEST_CASE_P(DelegationTest);

/**
 * @brief Tests lock functionality
 */
TYPED_TEST_P(LockTest, LockSpawnThread) {
	int* counter = new int;
	*counter = 0;
	std::thread threads[1];
	threads[0] = std::thread(
		[this, counter]() {
			ASSERT_NO_THROW(this->lock.lock());
			(*counter)++;
			this->lock.unlock();
		}
	);
	
	threads[0].join();
	ASSERT_EQ(*counter, 1);
}

/**
 * @brief Tests lock functionality with multiple threads
 */
TYPED_TEST_P(LockTest, LockSpawnManyThread) {
	const int threadcount = 128;
	int* counter = new int;
	*counter = 0;
	std::thread threads[threadcount];
	for(auto& t : threads) {
		t = std::thread(
			[this, counter]() {
				ASSERT_NO_THROW(this->lock.lock());
				(*counter)++;
				this->lock.unlock();
			}
		);
	}
	for(auto& t : threads) {	
		t.join();
	}
	ASSERT_EQ(*counter, threadcount);
}
REGISTER_TYPED_TEST_CASE_P(LockTest, LockSpawnThread, LockSpawnManyThread);

/**
 * @brief Tests delegation lock functionality
 */
TYPED_TEST_P(DelegationTest, DelegateSpawnThread) {
	int* counter = new int;
	*counter = 0;
	std::thread threads[1];
	threads[0] = std::thread(
		[this, counter]() {
			ASSERT_NO_THROW(this->lock.delegate_n([counter]() { (*counter)++; }));
		}
	);
	
	threads[0].join();
	ASSERT_EQ(*counter, 1);
}

/**
 * @brief Tests delegation lock functionality with multiple threads
 */
TYPED_TEST_P(DelegationTest, DelegateSpawnManyThread) {
	const int threadcount = 128;
	int* counter = new int;
	*counter = 0;
	std::thread threads[threadcount];
	for(auto& t : threads) {
		t = std::thread(
			[this, counter]() {
				ASSERT_NO_THROW(this->lock.delegate_n([counter]() { (*counter)++; }));
			}
		);
	}
	for(auto& t : threads) {	
		t.join();
	}
	ASSERT_EQ(*counter, threadcount);
}
REGISTER_TYPED_TEST_CASE_P(DelegationTest, DelegateSpawnThread, DelegateSpawnManyThread);
