#ifndef ADDR_H
#define ADDR_H

#include <stdint.h>
#ifndef u32
typedef uint32_t u32;
#endif

/* Address mapping helpers. Kept self-contained (no PSP SDK deps) so host
 * unit tests can compile and exercise them directly. */

extern Config cfg;

static inline u32 real_address(u32 address) {
	address &= 0x0FFFFFFF;

	// VRAM is a legal raw target even though it sits below cfg.address_start
	if(address >= 0x04000000 && address <= 0x041FFFFF) {
		return address;
	}

	if(address <= cfg.address_end - cfg.address_start) {
		// relative addressing: offset into game memory
		return address + cfg.address_start;
	}

	if(address >= cfg.address_start && address <= cfg.address_end) {
		// already an absolute in-range address
		return address;
	}

	// ambiguous gap value or above range — clamp to the nearest valid edge
	if(address < cfg.address_start) {
		return cfg.address_start;
	}
	return cfg.address_end;
}

static inline u32 address(u32 address) {
	return real_address(address) - cfg.address_format;
}

#endif
