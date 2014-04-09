#ifndef qd_pause_hpp
#define qd_pause_hpp qd_pause_hpp

static inline void pause() {
	asm("pause");
//	std::this_thread::yield();
}

#endif /* qd_pause_hpp */
