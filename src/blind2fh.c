// blind2fh: translate blind to farbherd.
// oh boy.
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
#include <stdint.h>
#include <string.h>

// endian.h-esqe define are ensured in farbherd.h
#include <farbherd.h>
#include "conversion.h"

static inline void read_blind_header(farbherd_header_t* header) {
	if (scanf("%u %u %u xyza\n", &header->frameCount, &header->imageHead.width, &header->imageHead.height) != 3) {
		fprintf(stderr, "Error: Failed to parse.\n");
		exit(1);
	}
	char uivf[6] = {0};
	fread(uivf, 5, 1, stdin);
	if (strncmp(uivf + 1, "uivf", 4) != 0) {
		fprintf(stderr, "Error: Failed to read uivf token. Is the input blind? Got '%s'\n", uivf + 1);
		exit(1);
	}
	//fprintf(stderr, "\n\n%c\n\n", fgetc(stdin));
	if (!(header->imageHead.width && header->imageHead.height)) {
		fprintf(stderr, "Error: blind has zero width? %ux%u\n", header->imageHead.width, header->imageHead.height);
		exit(2);
	}
}

int main(int argc, char ** argv) {
	size_t datasize;
	farbherd_header_t filehead;
	uint16_t * workingcore, * imagecore;
	FP* blindbuffer;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s loop/once ftMul ftDiv\n", argv[0]);
		return 1;
	}
	if (!strcmp(argv[1], "loop")) {
		filehead.flags = FARBHERD_FLAG_LOOP;
	} else if (strcmp(argv[1], "once")) {
		// no ! because this is a fallback for in case of error
		fprintf(stderr, "%s must be 'loop' or 'once'\n", argv[1]);
		return 1;
	}
	filehead.frameTimeMul = atoi(argv[2]);
	filehead.frameTimeDiv = atoi(argv[3]);

	filehead.fileExtSize = 0;
	filehead.fileExtData = 0;
	filehead.frameExtSize = 0;
	filehead.frameCount = 0;

	// Read first header into imageHead, which sets the remaining 2 fields.
	read_blind_header(&filehead);

	// And get ready to write out the file.
	datasize = farbherd_datasize(filehead.imageHead);

	// Not freed, not replaced
	if (!datasize) {
		fprintf(stderr, "not data?\n");
		return 1;
	} else {
		workingcore = malloc(datasize);
		imagecore = malloc(datasize);
		blindbuffer = malloc(datasize * 4);
		if (!workingcore) {
			fprintf(stderr, "Failed to allocate working buffer\n");
			return 1;
		}
		if (!imagecore) {
			fprintf(stderr, "Failed to allocate conversion buffer\n");
			return 1;
		}
		if (!blindbuffer) {
			fprintf(stderr, "Failed to allocate blind reading buffer\n");
		}
		memset(workingcore, 0, datasize);
	}

	// And write out the file's header, beginning the stream.
	farbherd_write_farbherd_header(stdout, filehead);
	fflush(stdout);

#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint16_t tmp[4];
#endif

	FP scaled[4];
	while (1) {
		if (datasize) {
			// Read blind data into buffer workingcore.
			if (farbherd_read_buffer(stdin, blindbuffer, datasize * 4))
				return 0;

			// Convert format from CIE XYZ to sRGB.
			size_t p;
			for (p = 0; p < (filehead.imageHead.height * filehead.imageHead.width); p++) {
				size_t px = p * 4;
				xyz2rgb(&blindbuffer[px], scaled);
				rgb2srgb(scaled, scaled);
				//fprintf(stderr, "CIE XYZ: (%f,%f,%f,%f)", blindbuffer[px], blindbuffer[px + 1], blindbuffer[px + 2], blindbuffer[px + 3]);
#if __BYTE_ORDER == __LITTLE_ENDIAN
				qfp2ush(scaled, tmp);
				qush2beush(tmp, &imagecore[px]);
#else
				qfp2ush(scaled, &imagecore[px]);
#endif
			}

			farbherd_calc_apply_delta(workingcore, imagecore, datasize);
			// NOTE: There's no extension stuff and this bit is written manually, so no issue.
			// For a non-zero frame extension, this is where/when you'd have to put the contents.
			fwrite(imagecore, datasize, 1, stdout);
			fflush(stdout);
		}
	}
}
