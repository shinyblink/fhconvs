// 2fh: Convert anything FFMPEG eats into a farbherd stream.
//
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

#include "farbherd.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define MSG(...) fprintf(stderr, __VA_ARGS__)
#define ERRF(fmt, ...) fprintf(stderr, "[error] " fmt "\n", __VA_ARGS__)
#define ERR(fmt) ERRF("%s", fmt)

int main(int argc, char* argv[]) {
	size_t datasize;
	farbherd_header_t filehead;
	uint16_t* workingcore;
	uint16_t* imagecore;
	int ret;

	if (argc != 2) {
		MSG("Usage: %s videofile\n", argv[0]);
		return 1;
	}

	// load file.
	AVFormatContext* fmtctx = avformat_alloc_context();
	if (!fmtctx) {
		ERR("Could not allocate memory for format context.");
		return 2;
	}

	if ((ret = avformat_open_input(&fmtctx, argv[1], NULL, NULL)) != 0) {
		ERRF("Couldn't open file: %s", av_err2str(ret));
		return 2;
	}

	MSG("[open] %s: %s\n", argv[1], fmtctx->iformat->long_name);

	// get stream info.
	if (avformat_find_stream_info(fmtctx, NULL) < 0) {
		ERR("Could not get stream info.");
		return 1;
	}

	// find right stream.
	AVCodec* codec;
	AVCodecParameters* cparams;
	AVStream* stream;
	int streamidx = -1;

	unsigned int i;
	for (i = 0; i < fmtctx->nb_streams; i++) {
		stream = fmtctx->streams[i];
		if (!stream) {
			ERR("Stream is NULL?");
			return 2;
		}
		cparams = stream->codecpar;
		MSG("[info] stream %i: framerate: %d/%d frames: %i\n", i, stream->r_frame_rate.num, stream->r_frame_rate.den, stream->nb_frames);

		codec = avcodec_find_decoder(cparams->codec_id);
		if (!codec) {
			ERR("Couldn't find decoder.");
			return 2;
		}
		if (cparams->codec_type == AVMEDIA_TYPE_VIDEO) {
			MSG("[info] Using %s\n", codec->long_name);
			streamidx = i;
			break;
		}
	}

	if (streamidx == -1) {
		ERR("Couldn't find a suitable video stream.");
		return 2;
	}

	// construct header information.
	filehead.imageHead.width = cparams->width;
	filehead.imageHead.height = cparams->height;

	// Get frame rate.
	AVRational rate;
#ifdef av_guess_frame_rate
	rate = av_guess_frame_rate(fmtctx, stream, NULL);
#else
	rate = stream->r_frame_rate;
#endif
	// Convert into frame time from FPS.
	// tl;dw: They are swapped.
	filehead.frameTimeMul = rate.den;
	filehead.frameTimeDiv = rate.num;

	filehead.fileExtSize = 0;
	filehead.fileExtData = 0;
	filehead.frameExtSize = 0;
	filehead.frameCount = stream->nb_frames;
	filehead.flags = 0;

	// And get ready to write out the file.
	datasize = farbherd_datasize(filehead.imageHead);

	// Not freed, not replaced
	if (datasize) {
		workingcore = av_malloc(datasize);
		imagecore = av_malloc(datasize);
		if (!workingcore) {
			ERR("Failed to allocate working buffer");
			return 1;
		}
		if (!imagecore) {
			ERR("Failed to allocate image buffer");
			return 1;
		}
		memset(workingcore, 0, datasize);
	}

	// get ready to read
	AVCodecContext* coctx = avcodec_alloc_context3(codec);
	if (!coctx) {
		ERR("Failed to allocate codec context.");
		return 2;
	}
	if (avcodec_parameters_to_context(coctx, cparams) < 0) {
		ERR("Failed to copy params to context.");
		return 2;
	}
	if ((ret = avcodec_open2(coctx, codec, NULL)) < 0) {
		ERRF("Failed to open codec: %s", av_err2str(ret));
		return 2;
	}

	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		ERR("Failed to alloc frame.");
		return 2;
	}

	AVPacket* packet = av_packet_alloc();
	if (!packet) {
		ERR("Failed to alloc packet.");
		return 2;
	}

	// the conversion stuff.
	AVFrame* cframe = av_frame_alloc();
	if (!cframe) {
		ERR("Failed to alloc frame.");
		return 2;
	}
	cframe->colorspace = AVCOL_SPC_RGB;
	av_image_fill_arrays(cframe->data, cframe->linesize, (unsigned char*) imagecore, AV_PIX_FMT_RGBA64BE, cparams->width, cparams->height, 1);

	struct SwsContext* swsctx = sws_getContext(cparams->width, cparams->height, coctx->pix_fmt, cparams->width, cparams->height, AV_PIX_FMT_RGBA64BE, 0, NULL, NULL, NULL);
	if (!swsctx) {
		ERR("Failed to initialize conversion context.");
		return 2;
	}

	// write out the file's header, beginning the stream.
	farbherd_write_farbherd_header(stdout, filehead);
	fflush(stdout);
	while (av_read_frame(fmtctx, packet) >= 0) {
		if (packet->stream_index == streamidx) {
			// send to decoder
			int ret = avcodec_send_packet(coctx, packet);
			if (ret < 0) {
				ERRF("failed sending packet: %s", av_err2str(ret));
				return 2;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(coctx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				if (ret < 0) {
					ERRF("while receiving frame: %s", av_err2str(ret));
					return 2;
				}

				ret = sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, cframe->data, cframe->linesize);
				if (!ret) {
					ERRF("while converting frame: %s", av_err2str(ret));
					return 2;
				}

				farbherd_calc_apply_delta(workingcore, imagecore, datasize);
				// NOTE: There's no extension stuff and this bit is written manually, so no issue.
				// For a non-zero frame extension, this is where/when you'd have to put the contents.
				fwrite(imagecore, datasize, 1, stdout);
				fflush(stdout);
			}
		}
		av_packet_unref(packet);
	}

	// free all the things.
	free(workingcore);

	sws_freeContext(swsctx);

	avformat_close_input(&fmtctx);
	avformat_free_context(fmtctx);
	av_frame_free(&frame);
	av_frame_free(&cframe);
	av_packet_free(&packet);
	avcodec_free_context(&coctx);
}
