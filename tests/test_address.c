/* Host unit tests for real_address()/address() — exercises the REAL code
 * from src/include/addr.h (what cheat.c uses at runtime). */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "test_common.h"

Config cfg; /* provided by addr.h's extern */
#include "../src/include/addr.h"

static int failures = 0;

static void expect(const char *name, u32 got, u32 want) {
	if(got != want) {
		printf("FAIL %-28s got 0x%08X want 0x%08X\n", name, got, want);
		failures++;
	} else {
		printf("ok   %-28s 0x%08X\n", name, got);
	}
}

int main(void) {
	/* ---- default game range (user RAM) ---- */
	memset(&cfg, 0, sizeof(cfg));
	cfg.address_start = 0x08800000;
	cfg.address_end = 0x09FFFFFF;
	cfg.address_format = 0;

	expect("rel 0",              real_address(0x00000000), 0x08800000);
	expect("rel 0x10000",        real_address(0x00010000), 0x08810000);
	expect("rel max",            real_address(0x017FFFFF), 0x09FFFFFF);
	expect("gap 0x01800000->start", real_address(0x01800000), 0x08800000);
	expect("gap 0x07FFFFFF->start", real_address(0x07FFFFFF), 0x08800000);
	expect("VRAM base kept",     real_address(0x04000000), 0x04000000);
	expect("VRAM top kept",      real_address(0x041FFFFF), 0x041FFFFF);
	expect("abs start kept",     real_address(0x08800000), 0x08800000);
	expect("abs end kept",       real_address(0x09FFFFFF), 0x09FFFFFF);
	expect("above 0x0A000000->end", real_address(0x0A000000), 0x09FFFFFF);
	expect("mask overflow->end", real_address(0x0FFFFFFF), 0x09FFFFFF);
	expect("addr() rel w/ format 0", address(0x00000000), 0x08800000);

	/* ---- format offset ---- */
	cfg.address_format = 1;
	expect("addr() rel w/ format 1", address(0x00000000), 0x087FFFFF);
	cfg.address_format = 0;

	/* ---- POPS range ---- */
	cfg.address_start = 0x09800000;
	cfg.address_end = 0x09FFFFFF;
	expect("pops rel 0",         real_address(0x00000000), 0x09800000);
	expect("pops VRAM kept",     real_address(0x04000000), 0x04000000);
	expect("pops gap->start",    real_address(0x01900000), 0x09800000);
	expect("pops abs 0x09E80400", real_address(0x09E80400), 0x09E80400);
	expect("pops above->end",    real_address(0x0A000000), 0x09FFFFFF);

	/* invariant: result is always VRAM or within [start,end] */
	{
		u32 a;
		for(a = 0; a < 0x10000000u; a += 0x1000) {
			u32 r = real_address(a);
			int ok = (r >= 0x04000000 && r <= 0x041FFFFF) ||
			         (r >= cfg.address_start && r <= cfg.address_end);
			if(!ok) {
				printf("FAIL invariant 0x%08X -> 0x%08X\n", a, r);
				failures++;
				break;
			}
		}
	}

	printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}