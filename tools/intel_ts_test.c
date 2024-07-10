/*
 * Copyright © 2024 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include "intel_io.h"
#include "intel_chipset.h"
#include "drmtest.h"

static volatile bool quit;

static void sighandler(int x)
{
	quit = true;
}

static uint32_t read_reg(uint32_t addr)
{
	return INREG(addr);
}

static void __attribute__((noreturn)) usage(const char *name)
{
	fprintf(stderr, "Usage: %s [options]\n"
		" -p,--pipe <pipe>\n",
		name);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct intel_mmio_data mmio_data;
	uint32_t last_count = 0, last_ts = 0;
	int pipe = 0;

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	for (;;) {
		static const struct option long_options[] = {
			{ .name = "pipe", .has_arg = required_argument, },
			{}
		};

		int opt = getopt_long(argc, argv, "p:", long_options, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'p':
			if (optarg[1] != '\0')
				usage(argv[0]);
			pipe = optarg[0];
			if (pipe >= 'a')
				pipe -= 'a';
			else if (pipe >= 'A')
				pipe -= 'A';
			else if (pipe >= '0')
				pipe -= '0';
			else
				usage(argv[0]);
			if (pipe < 0 || pipe > 3)
				usage(argv[0]);
			break;
		}
	}

	intel_register_access_init(&mmio_data, intel_get_pci_device(), 0, -1);

	while (!quit) {
		uint32_t count, ts;

		count = read_reg(0x70040 + pipe * 0x1000);
		ts = read_reg(0x70048 + pipe * 0x1000);

		printf("ts: %11u (change %11d), count: %11u (change %11d)\n",
		       ts, (int32_t)(ts - last_ts),
		       count, (int32_t)(count - last_count));

		last_count = count;
		last_ts = ts;
	}

	intel_register_access_fini(&mmio_data);

	return 0;
}
