/*
 * Copyright (C) 2009-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"
#include "libavformat/avformat.h"


static void video_decode(char *filename)
{
	AVInputFormat *format;
	AVFormatContext *format_context;
	AVCodec *codec;
	AVCodecContext *codec_context = NULL;
	AVFrame *frame;
	AVPacket packet;
	int video_stream, frame_finished;
	
	if (!(format = av_find_input_format("h264")))
		return;
	if (av_open_input_file(&format_context, filename, format, 0, NULL) != 0)
		return;
	if (av_find_stream_info(format_context) < 0)
		return;
	for (video_stream = 0; video_stream < format_context->nb_streams; video_stream++)
		if (format_context->streams[video_stream]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			codec_context = format_context->streams[video_stream]->codec;
			break;
		}
	if (!codec_context)
		return;
#warn Multithreading is broken right now.
#if 0 && defined(THREADS) && THREADS > 1
	if (avcodec_thread_init(codec_context, THREADS) < 0)
		return;
#endif
	if (!(codec = avcodec_find_decoder(codec_context->codec_id)))
		return;
	if (avcodec_open(codec_context, codec) < 0)
		return;
	if (!(frame = avcodec_alloc_frame()))
		return;
	
	process_init(codec_context, filename);
	while (av_read_frame(format_context, &packet) >= 0) {
		if (packet.stream_index == video_stream) {
			AVPacket working_packet = packet;
			do {
				int length = avcodec_decode_video2(codec_context, frame, &frame_finished, &working_packet);
				if (length < 0) return;
				working_packet.size -= length;
				working_packet.data += length;
			} while (working_packet.size);
		}
		av_free_packet(&packet);
	}
	packet.size = 0;
	packet.data = NULL;
	do {
		if (avcodec_decode_video2(codec_context, frame, &frame_finished, &packet) < 0)
			return;
	} while (frame_finished);
	process_finish(codec_context);
	
	av_free(frame);
	avcodec_close(codec_context);
	av_close_input_file(format_context);
}


int main(int argc, char **argv)
{
	const char *filename;
	int i;
	
	avcodec_init();
	av_register_all();
	
	if (argc <= 1)
		return 1;
	filename = argv[1];
	
	for (i = 1; i < argc; i++)
		video_decode(argv[i]);
	
	return 0;
}
