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

#include <farbherd.h>
#include "conversion.h"

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

#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t tmp[4];
#endif

	FP scaled[4];
	while (1) {
		if (farbherd_read_farbherd_frame(stdin, &frame, head))
			return 0;
		farbherd_apply_delta(workspace, frame.deltas, datasize);

		// call conversion routine
		size_t p;
		for (p = 0; p < (head.imageHead.height * head.imageHead.width); p++) {
			size_t off = p * 4;
#if __BYTE_ORDER == __LITTLE_ENDIAN
			qbeush2ush(&workspace[off], tmp);
			qush2fp(tmp, scaled);
#else
			qush2fp(&workspace[off], scaled);
#endif
			srgb2rgb(scaled, scaled);
			rgb2xyz(scaled, &converted[off]);
		}

		fwrite(converted, datasize * 4, 1, stdout);
		fflush(stdout);
	}
	return 0;
}
