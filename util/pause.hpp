#ifndef qd_pause_hpp
#define qd_pause_hpp qd_pause_hpp

namespace qd {

static inline void pause() {
	asm("pause");
//	std::this_thread::yield();
}

} /* namespace qd */

#endif /* qd_pause_hpp */
