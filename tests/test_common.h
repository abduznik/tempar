#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdint.h>

#ifndef u32
typedef uint32_t u32;
#endif
#ifndef u8
typedef uint8_t u8;
#endif
#ifndef u16
typedef uint16_t u16;
#endif
#ifndef s8
typedef int8_t s8;
#endif

/* Mirrors src/include/config.h — MUST stay in sync (same layout, packed). */
typedef struct Config {
	u8 ver;
	u8 mac_enable;
	u8 mac[6];
	u32 menu_key;
	u32 trigger_key;
	u32 screen_key;
	u8 cheat_pause;
	u32 cheat_hz;
	u32 address_format;
	u8 color_file;
	s8 cheat_file;
	u8 hijack_pspar_buttons;
	u8 cheat_status;
	u8 hbid_pbp_force;
	u32 max_text_rows;
	u16 max_cheats;
	u16 max_blocks;
	u8 auto_off;
	u32 address_start;
	u32 address_end;
	u8 swap_xo;
	u8 language_file;
	u32 checksum;
} __attribute__((__packed__)) Config;

#endif