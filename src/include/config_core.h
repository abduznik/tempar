#ifndef CONFIG_CORE_H
#define CONFIG_CORE_H

#include <stdint.h>
#ifndef u32
typedef uint32_t u32;
#endif
#ifndef u8
typedef uint8_t u8;
#endif

/* Config checksum + clamping helpers, shared between the plugin
 * (config.c) and the host-side unit tests. self-contained on purpose
 * (no PSP SDK deps). */

extern Config cfg;

/* Computes a simple additive checksum over all Config fields except the checksum field itself. */
static inline u32 config_checksum(void) {
	u32 sum = 0;
	u32 i;
	const u8 *p = (const u8*)&cfg;

	for(i = 0; i < sizeof(Config) - sizeof(u32); i++) {
		sum += p[i];
	}

	return sum;
}

/* Clamps config fields to safe ranges (preserves user settings). */
static inline void config_clamp(void) {
	if(cfg.max_cheats == 0 || cfg.max_cheats > 8192) {
		cfg.max_cheats = (cfg.max_cheats == 0 ? 1024 : 8192);
	}
	if(cfg.max_blocks == 0 || cfg.max_blocks > 16384) {
		cfg.max_blocks = (cfg.max_blocks == 0 ? 2048 : 16384);
	}
	if(cfg.max_text_rows == 0 || cfg.max_text_rows > 100000) {
		cfg.max_text_rows = (cfg.max_text_rows == 0 ? 5000 : 100000);
	}
}

#endif
