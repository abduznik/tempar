/* Host unit tests for config_checksum()/config_clamp() — the REAL code
 * from src/include/config_core.h (what config.c uses at runtime). */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "test_common.h"

Config cfg; /* provided by config_core.h's extern */
#include "../src/include/config_core.h"

static int failures = 0;

static void check(const char *name, int cond) {
	if(!cond) {
		printf("FAIL %s\n", name);
		failures++;
	} else {
		printf("ok   %s\n", name);
	}
}

int main(void) {
	/* ---- checksum round-trip ---- */
	memset(&cfg, 0, sizeof(cfg));
	cfg.ver = 8;
	cfg.max_cheats = 1024;
	cfg.max_blocks = 2048;
	cfg.max_text_rows = 5000;
	cfg.address_start = 0x08800000;
	cfg.address_end = 0x09FFFFFF;

	cfg.checksum = config_checksum();
	check("checksum self-consistent", cfg.checksum == config_checksum());

	/* flipping a byte must break the checksum */
	{
		u32 original = cfg.checksum;
		cfg.menu_key = 1;
		check("checksum detects change", config_checksum() != original);
	}

	/* ---- clamp ---- */
	cfg.max_cheats = 0;
	cfg.max_blocks = 99999;
	cfg.max_text_rows = 0;
	config_clamp();
	check("clamp max_cheats 0->1024", cfg.max_cheats == 1024);
	check("clamp max_blocks 99999->16384", cfg.max_blocks == 16384);
	check("clamp max_text_rows 0->5000", cfg.max_text_rows == 5000);

	cfg.max_cheats = 20000;
	cfg.max_blocks = 1;
	cfg.max_text_rows = 123456;
	config_clamp();
	check("clamp max_cheats 20000->8192", cfg.max_cheats == 8192);
	check("clamp max_blocks 1 kept (valid)", cfg.max_blocks == 1);
	check("clamp max_text_rows 123456->100000", cfg.max_text_rows == 100000);

	/* in-range values are untouched */
	cfg.max_cheats = 4096;
	cfg.max_blocks = 8000;
	cfg.max_text_rows = 20000;
	config_clamp();
	check("clamp keeps in-range max_cheats", cfg.max_cheats == 4096);
	check("clamp keeps in-range max_blocks", cfg.max_blocks == 8000);
	check("clamp keeps in-range max_text_rows", cfg.max_text_rows == 20000);

	printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}