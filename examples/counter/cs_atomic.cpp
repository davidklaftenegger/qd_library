#include<atomic>
#include<chrono>
#include<iostream>

static std::atomic<int> shared_counter;
static std::chrono::time_point<std::chrono::high_resolution_clock> start_time;

void cs_init() {
	shared_counter = 0;
	start_time = std::chrono::high_resolution_clock::now();
}

void cs() {
	shared_counter += 1;
}

void cs_finish() {
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	std::cout << "final counter value: " << shared_counter.load() << std::endl;
	std::cout << "time needed: " << duration.count() << " ms" << std::endl;
}

