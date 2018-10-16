// fh2blind: convert farbherd to blind.
// See https://tools.suckless.org/blind/
//
// License:
// farbherd - animation format & tools, designed to be similar to farbfeld.
// Written in 2018 by 20kdc <asdd2808@gmail.com>, vifino <vifino@tty.sh> and contributors.
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide.
// This software is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

// sorry.
#include <endian.h>

#include "farbherd.h"

#define SCALE(chan) (((double) betoh16(chan)) / UINT16_MAX)
#define CONV(v1, v2, v3) (r * v1 + g * v2 + b * v3)
static inline void convert(uint16_t* src, double* dst) {
	// scale to 0..1
	double r = SCALE(src[0]);
	double g = SCALE(src[1]);
	double b = SCALE(src[2]);
	double a = SCALE(src[3]);

	dst[0] = CONV(0.4124564, 0.3565651, 0.1804375);
	dst[1] = CONV(0.2126729, 0.7151522, 0.0721750);
	dst[2] = CONV(0.0193339, 0.1191920, 0.9503041);
	dst[3] = a; // yep. very complicated.
}

int main(int argc, char ** argv) {
	if ((argc != 1)) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return 1;
	}

	farbherd_header_t head;
	if (farbherd_read_farbherd_header(stdin, &head)) {
		fputs("Failed to read farbherd header\n", stderr);
		return 1;
	}
	size_t datasize = farbherd_datasize(head.imageHead);
	uint16_t* workspace = 0;
	double* converted = 0;
	if (datasize) {
		workspace = malloc(datasize);
		if (!workspace) {
			fputs("Failed to allocate workspace memory\n", stderr);
			return 1;
		}
		memset(workspace, 0, datasize);
		converted = malloc(datasize * 4);
	}

	farbherd_frame_t frame;
	if (farbherd_init_farbherd_frame(&frame, head)) {
		fputs("Failed to allocate incoming frame memory\n", stderr);
		return 1;
	}


	// blind header.
	printf("%u %u %u xyza\n", head.frameCount, head.imageHead.width, head.imageHead.height);
	fputc(0, stdout);
	fputs("uivf", stdout);
	fflush(stdout);

	while (1) {
		if (farbherd_read_farbherd_frame(stdin, &frame, head))
			return 0;
		farbherd_apply_delta(workspace, frame.deltas, datasize);

		// call conversion routine
		size_t p;
		for (p = 0; p < (head.imageHead.height * head.imageHead.width); p++) {
			size_t off = p * 4;
			convert(&frame.deltas[off], &converted[off]);
		}

		fwrite(converted, datasize * 4, 1, stdout);
		fflush(stdout);
	}
	return 0;
}
