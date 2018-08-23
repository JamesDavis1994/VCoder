﻿#include <sstream>
#include "Encoder.h"
#include "easylogging++.h"

using namespace LibVKDR;

Encoder::Encoder(ExportSettings exportSettings) :
	exportSettings(exportSettings)
{}

int Encoder::open()
{
	int ret;

	// Create format context
	formatContext = avformat_alloc_context();
	formatContext->oformat = av_guess_format(exportSettings.muxerName.c_str(), exportSettings.filename.c_str(), NULL);

	// Mark myself as encoding tool in the mp4/mov container
	const string appId = exportSettings.application + " - www.voukoder.org";
	av_dict_set(&formatContext->metadata, "encoding_tool", appId.c_str(), 0);

	av_log(NULL, AV_LOG_INFO, "### ENCODER STARTED ###\n");

	// Try to open the video encoder
	const int flags = getCodecFlags(AVMEDIA_TYPE_VIDEO);
	if ((ret = openCodec(exportSettings.videoCodecName.c_str(), exportSettings.videoOptions, &videoContext, flags)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Unable to open video encoder: %s\n", exportSettings.videoCodecName.c_str());
		close();
		return ret;
	}

	// Try to open the audio encoder (if wanted)
	if (exportSettings.exportAudio)
	{
		if ((ret = openCodec(exportSettings.audioCodecName.c_str(), exportSettings.audioOptions, &audioContext, 0)) < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Unable to open audio encoder: %s\n", exportSettings.audioCodecName.c_str());
			close();
			return ret;
		}

		// Find the right sample size (for variable frame size codecs (PCM) we use the number of samples that match for one video frame)
		audioFrameSize = audioContext.codecContext->frame_size;
		if (audioFrameSize == 0)
		{
			audioFrameSize = av_rescale_q(1, videoContext.codecContext->time_base, audioContext.codecContext->time_base);
		}
	}

	AVDictionary *options = NULL;
	//av_dict_set(&options, "write_index", 0, 0);

	// Support pipe export
	if (exportSettings.pipe)
	{
		// Open memory buffer
		ret = avio_open_dyn_buf(&formatContext->pb);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Unable to open dynamic buffer\n");
			close();
			return ret;
		}

		pipe.open(exportSettings.pipeCommand);
	}
	else
	{
		string filename;
		if (pass < exportSettings.passes)
		{
			filename = "NUL";
		}
		else
		{
			filename = exportSettings.filename;
			formatContext->url = av_strdup(exportSettings.filename.c_str());
		}

		// Open file writer
		ret = avio_open(&formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Unable to open file buffer\n");
			return ret;
		}

		// Dump format settings
		av_dump_format(formatContext, 0, filename.c_str(), 1);

		// Set the faststart flag in the last pass
		if (pass == exportSettings.passes && exportSettings.flagFaststart)
		{
			av_dict_set(&options, "movflags", "faststart", 0);
		}
	}
	
	return avformat_write_header(formatContext, &options);
}

int Encoder::openCodec(const string codecName, const string options, EncoderContext *encoderContext, const int flags)
{
	int ret;

	if ((ret = createCodecContext(codecName.c_str(), encoderContext, flags)) == 0)
	{
		av_log(NULL, AV_LOG_INFO, "Opening codec: %s with options: %s\n", codecName.c_str(), options.c_str());

		AVDictionary *dictionary = NULL;
		if ((ret = av_dict_parse_string(&dictionary, options.c_str(), "=", ",", 0)) == 0)
		{
			// Set the logfile for multi-pass encodes to a file in the temp directory
			if (encoderContext->codecContext->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				char charPath[MAX_PATH];
				if (GetTempPathA(MAX_PATH, charPath))
				{
					strcat_s(charPath, "voukoder-passlogfile");
					av_dict_set(&dictionary, "passlogfile", charPath, 0);
				}
				else
				{
					av_log(NULL, AV_LOG_WARNING, "System call failed: GetTempPathA()\n");
				}
			}

			// Open the codec
			if ((ret = avcodec_open2(encoderContext->codecContext, encoderContext->codecContext->codec, &dictionary)) == 0)
			{
				return avcodec_parameters_from_context(encoderContext->stream->codecpar, encoderContext->codecContext);
			}
		}
	}

	return ret;
}

int Encoder::createCodecContext(string codecName, EncoderContext *encoderContext, int flags)
{
	AVCodec *codec = avcodec_find_encoder_by_name(codecName.c_str());
	if (codec == NULL)
	{
		return AVERROR_ENCODER_NOT_FOUND;
	}

	encoderContext->frameFilter = NULL;

	encoderContext->codecContext = avcodec_alloc_context3(codec);
	encoderContext->codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	encoderContext->codecContext->thread_count = 0;
	encoderContext->codecContext->flags |= flags;

	if (codec->type == AVMEDIA_TYPE_VIDEO)
	{
		encoderContext->codecContext->width = exportSettings.resizeWidth > 0 ? exportSettings.resizeWidth : exportSettings.width;
		encoderContext->codecContext->height = exportSettings.resizeHeight > 0 ? exportSettings.resizeHeight : exportSettings.height;
		encoderContext->codecContext->time_base = exportSettings.videoTimebase;
		encoderContext->codecContext->pix_fmt = av_get_pix_fmt(exportSettings.pixelFormat.c_str());
		encoderContext->codecContext->colorspace = exportSettings.colorSpace;
		encoderContext->codecContext->color_range = exportSettings.colorRange;
		encoderContext->codecContext->color_primaries = exportSettings.colorPrimaries;
		encoderContext->codecContext->color_trc = exportSettings.colorTRC;
		encoderContext->codecContext->sample_aspect_ratio = exportSettings.videoSar;
		encoderContext->codecContext->field_order = exportSettings.fieldOrder;

		// Add stats_info to second pass
		if (encoderContext->codecContext->flags & AV_CODEC_FLAG_PASS2)
		{
			encoderContext->codecContext->stats_in = encoderContext->stats_info;
		}
	}
	else if (codec->type == AVMEDIA_TYPE_AUDIO)
	{
		encoderContext->codecContext->channels = av_get_channel_layout_nb_channels(exportSettings.audioChannelLayout);
		encoderContext->codecContext->channel_layout = exportSettings.audioChannelLayout;
		encoderContext->codecContext->time_base = exportSettings.audioTimebase;
		encoderContext->codecContext->sample_rate = exportSettings.audioTimebase.den;
		encoderContext->codecContext->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		encoderContext->codecContext->bit_rate = 0;
	}

	encoderContext->stream = avformat_new_stream(formatContext, codec);
	encoderContext->stream->id = formatContext->nb_streams - 1;
	encoderContext->stream->time_base = encoderContext->codecContext->time_base;

	if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		encoderContext->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	return 0;
}

int Encoder::getCodecFlags(const AVMediaType type)
{
	int flags = 0;

	if (type == AVMEDIA_TYPE_VIDEO)
	{
		// Mark as interlaced
		if (exportSettings.fieldOrder != AVFieldOrder::AV_FIELD_PROGRESSIVE)
		{
			flags |= AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME;
		}

		// Set encoding pass
		if (exportSettings.passes > 1)
		{
			if (pass == 1)
			{
				flags |= AV_CODEC_FLAG_PASS1;
			}
			else
			{
				flags |= AV_CODEC_FLAG_PASS2;
			}
		}
	}

	return flags;
}

void Encoder::finalize()
{
	flushContext(&videoContext);

	if (exportSettings.exportAudio)
		flushContext(&audioContext);

	av_write_trailer(formatContext);

	// Save stats data from first pass
	AVCodecContext* codec = videoContext.codecContext;
	if (codec->flags & AV_CODEC_FLAG_PASS1 && codec->stats_out)
	{
		int size = strlen(codec->stats_out) + 1;
		videoContext.stats_info = (char*)malloc(size);
		strcpy_s(videoContext.stats_info, size, codec->stats_out);
	}
	else if (codec->flags & AV_CODEC_FLAG_PASS2 && videoContext.stats_info)
	{
		free(videoContext.stats_info);
	}
}

void Encoder::close()
{
	// Free video context
	if (videoContext.codecContext != NULL)
		avcodec_free_context(&videoContext.codecContext);

	// Free audio context
	if (audioContext.codecContext != NULL)
		avcodec_free_context(&audioContext.codecContext);

	// Close pipe buffer or file
	if (exportSettings.pipe)
	{
		uint8_t *buffer = NULL;
		const int size = avio_close_dyn_buf(formatContext->pb, &buffer);
		if (exportSettings.pipe)
		{
			pipe.write(buffer, size);
		}
		av_free(buffer);
		pipe.close();
	}
	else
	{
		avio_close(formatContext->pb);
	}

	avformat_free_context(formatContext);

	av_log(NULL, AV_LOG_INFO, "### ENCODER FINISHED ###\n");
}

int Encoder::testSettings()
{
	// Create format context
	formatContext = avformat_alloc_context();
	formatContext->oformat = av_guess_format(exportSettings.muxerName.c_str(), exportSettings.filename.c_str(), NULL);

	// Mark myself as encoding tool in the mp4/mov container
	const string appId = exportSettings.application + " - www.voukoder.org";
	av_dict_set(&formatContext->metadata, "encoding_tool", appId.c_str(), 0);

	av_log(NULL, AV_LOG_INFO, "### ENCODER STARTED ###\n");

	int ret;

	if ((ret = openCodec(exportSettings.videoCodecName.c_str(), exportSettings.videoOptions, &videoContext, 0)) == 0)
	{
		if ((ret = openCodec(exportSettings.audioCodecName.c_str(), exportSettings.audioOptions, &audioContext, 0)) == 0)
		{
			if ((ret = avio_open_dyn_buf(&formatContext->pb)) == 0)
			{
				AVDictionary *options = NULL;
				ret = avformat_write_header(formatContext, &options);
			}
		}
	}

	if (videoContext.codecContext != NULL)
		avcodec_free_context(&videoContext.codecContext);

	// Free audio context
	if (audioContext.codecContext != NULL)
		avcodec_free_context(&audioContext.codecContext);

	uint8_t *buffer = NULL;
	avio_close_dyn_buf(formatContext->pb, &buffer);
	av_free(buffer);

	avformat_free_context(formatContext);

	return ret;
}

FrameType Encoder::getNextFrameType()
{
	return (av_compare_ts(videoContext.next_pts, videoContext.codecContext->time_base, audioContext.next_pts, audioContext.codecContext->time_base) <= 0) ? FrameType::VideoFrame : FrameType::AudioFrame;
}

void Encoder::flushContext(EncoderContext *encoderContext)
{
	delete(encoderContext->frameFilter);
	encoderContext->frameFilter = NULL;

	encodeAndWriteFrame(encoderContext, NULL);
}

int Encoder::getAudioFrameSize()
{
	return audioFrameSize;
}

int Encoder::writeVideoFrame(EncoderData *encoderData)
{
	int ret = 0;

	// Do we need a frame filter for pixel format conversion?
	if ((av_get_pix_fmt(encoderData->pix_fmt) != videoContext.codecContext->pix_fmt || exportSettings.videoFilters.size() > 0)
		&& videoContext.frameFilter == NULL)
	{
		FrameFilterOptions options;
		options.media_type = videoContext.codecContext->codec_type;
		options.width = videoContext.codecContext->width;
		options.height = videoContext.codecContext->height;
		options.pix_fmt = av_get_pix_fmt(encoderData->pix_fmt);
		options.time_base = videoContext.codecContext->time_base;
		options.sar.den = videoContext.codecContext->sample_aspect_ratio.den;
		options.sar.num = videoContext.codecContext->sample_aspect_ratio.num;

		string filters;
		for (string filter : exportSettings.videoFilters)
		{
			filters += "," + filter;
		}

		char filterConfig[256];
		sprintf_s(filterConfig, "format=pix_fmts=%s%s", 
			exportSettings.pixelFormat.c_str(),
			filters.c_str());

		videoContext.frameFilter = new FrameFilter();
		videoContext.frameFilter->configure(options, filterConfig);
	}

	// Create a new frame
	AVFrame *frame = av_frame_alloc();
	frame->width = exportSettings.width;
	frame->height = exportSettings.height;
	frame->format = av_get_pix_fmt(encoderData->pix_fmt);
	frame->top_field_first = videoContext.codecContext->field_order == AVFieldOrder::AV_FIELD_TT;

	if ((ret = av_frame_get_buffer(frame, 0)) < 0)
	{
		return ret;
	}

	// Fill the frame with data
	for (int i = 0; i < encoderData->planes; i++)
	{
		frame->data[i] = (uint8_t*)encoderData->plane[i];
		frame->linesize[i] = encoderData->stride[i];
	}

	frame->pts = videoContext.next_pts++;

	// Send the frame to the encoder
	if ((ret = encodeAndWriteFrame(&videoContext, frame)) < 0)
	{
		av_frame_free(&frame);
		return ret;
	}

	av_frame_free(&frame);

	return 0;
}

int Encoder::writeAudioFrame(float **data, int32_t sampleCount)
{
	if (audioContext.frameFilter == NULL)
	{
		FrameFilterOptions options;
		options.media_type = audioContext.codecContext->codec->type;
		options.channel_layout = audioContext.codecContext->channel_layout;
		options.sample_fmt = AV_SAMPLE_FMT_FLTP;
		options.time_base = { 1, audioContext.codecContext->sample_rate };

		char filterConfig[256];
		sprintf_s(filterConfig,
			"aformat=channel_layouts=%dc:sample_fmts=%s:sample_rates=%d,asetnsamples=n=%d",
			audioContext.codecContext->channels,
			av_get_sample_fmt_name(audioContext.codecContext->sample_fmt),
			audioContext.codecContext->sample_rate,
			audioFrameSize);

		audioContext.frameFilter = new FrameFilter();
		audioContext.frameFilter->configure(options, filterConfig);
	}

	// Create the source frame
	AVFrame *frame;
	frame = av_frame_alloc();
	frame->nb_samples = sampleCount;
	frame->channel_layout = audioContext.codecContext->channel_layout;
	frame->format = AV_SAMPLE_FMT_FLTP;
	frame->sample_rate = audioContext.codecContext->sample_rate;
	frame->pts = audioContext.next_pts;

	audioContext.next_pts += sampleCount;
	
	int ret = 0;
	if ((ret = av_frame_get_buffer(frame, 0)) < 0)
	{
		return ret;
	}

	// Fill the frame with data
	for (int c = 0; c < audioContext.codecContext->channels; c++)
	{
		frame->data[c] = (uint8_t*)data[c];
	}

	// Send the frame to the encoder
	ret = encodeAndWriteFrame(&audioContext, frame);

	av_frame_free(&frame);

	return ret;
}

int Encoder::encodeAndWriteFrame(EncoderContext *context, AVFrame *frame)
{
	int ret = 0;

	if (frame == NULL)
	{
		// Send NULL frame
		if ((ret = avcodec_send_frame(context->codecContext, frame)) < 0)
		{
			return ret;
		}

		return receivePackets(context);
	}

	if (context->frameFilter != NULL)
	{
		// Send the uncompressed frame to frame filter
		if ((ret = context->frameFilter->sendFrame(frame)) < 0)
		{
			return ret;
		}

		AVFrame *tmp_frame;
		tmp_frame = av_frame_alloc();

		// Receive frames from the rame filter
		while (context->frameFilter->receiveFrame(tmp_frame) >= 0)
		{
			if ((ret = avcodec_send_frame(context->codecContext, tmp_frame)) < 0)
			{
				av_frame_free(&tmp_frame);
				return ret;
			}

			av_frame_unref(tmp_frame);
		}

		av_frame_free(&tmp_frame);
	}
	else
	{
		if ((ret = avcodec_send_frame(context->codecContext, frame)) < 0)
		{
			return ret;
		}
	}

	return receivePackets(context);
}

int Encoder::receivePackets(EncoderContext *context)
{
	int ret = 0;

	AVPacket *packet = av_packet_alloc();
	while (avcodec_receive_packet(context->codecContext, packet) >= 0)
	{
		if (context->codecContext->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO && packet->duration == 0)
			packet->duration = 1;

		av_packet_rescale_ts(packet, context->codecContext->time_base, context->stream->time_base);

		packet->stream_index = context->stream->index;

		if ((ret = av_interleaved_write_frame(formatContext, packet)) < 0)
			break;

		av_packet_unref(packet);

		if (exportSettings.pipe)
		{
			// Write current buffer
			uint8_t *buffer = NULL;
			const int size = avio_close_dyn_buf(formatContext->pb, &buffer);
			pipe.write(buffer, size);
			av_free(buffer);

			// Create a new buffer
			avio_open_dyn_buf(&formatContext->pb);
		}
	}

	return ret;
}