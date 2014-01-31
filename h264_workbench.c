/*
 * Copyright (C) 2009-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"
#include "libavformat/avformat.h"


static void video_decode(const char *filename)
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
	if (!(format_context = avformat_alloc_context()))
		return;
	if (avformat_open_input(&format_context, filename, format, NULL) < 0)
		return;
	if (avformat_find_stream_info(format_context, NULL) < 0)
		return;
	if ((video_stream = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0)) < 0)
		return;
	if (!(codec_context = format_context->streams[video_stream]->codec))
		return;
	if (!(codec = avcodec_find_decoder(codec_context->codec_id)))
		return;
	
	process_init(codec_context, filename);
	
	if (avcodec_open2(codec_context, codec, NULL) < 0)
		return;
	if (!(frame = avcodec_alloc_frame()))
		return;
	
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
	
	av_free(frame);
	avcodec_close(codec_context);
	process_finish(codec_context);
	avformat_close_input(&format_context);
}


int main(int argc, const char **argv)
{
	const char *filename;
	int i;
	
	avcodec_register_all();
	av_register_all();
	
	if (argc <= 1)
		return 1;
	filename = argv[1];
	
	for (i = 1; i < argc; i++)
		video_decode(argv[i]);
	
	return 0;
}
