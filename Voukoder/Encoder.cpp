#include "Encoder.h"
#include "ExporterX264Common.h"

static void avlog_cb(void *, int level, const char * szFmt, va_list varg)
{
	char logbuf[2000];
	vsnprintf(logbuf, sizeof(logbuf), szFmt, varg);
	logbuf[sizeof(logbuf) - 1] = '\0';

	OutputDebugStringA(logbuf);
}

Encoder::Encoder(const char *filename)
{
	av_register_all();
	avfilter_register_all();

	this->filename = filename;
	
	/* Create the container format */
	formatContext = avformat_alloc_context();
	formatContext->oformat = av_guess_format(NULL, filename, NULL);

	av_log_set_level(AV_LOG_DEBUG);
	av_log_set_callback(avlog_cb);
}

Encoder::~Encoder()
{
	/* Free the muxer */
	avformat_free_context(formatContext);
}

void Encoder::setVideoCodec(AVCodecID codecId, csSDK_int32 width, csSDK_int32 height, AVPixelFormat pixelFormat, AVRational pixelAspectRation, AVRational timebase, AVFieldOrder fieldOrder)
{
	videoContext = new AVContext;

	/* Find codec */
	videoContext->codec = avcodec_find_encoder(codecId);
	if (videoContext->codec == NULL)
	{
		return;
	}

	/* Create the stream */
	videoContext->stream = avformat_new_stream(formatContext, NULL);
	videoContext->stream->id = formatContext->nb_streams - 1;
	videoContext->stream->time_base = timebase;

	/* Allocate the codec context */
	videoContext->codecContext = avcodec_alloc_context3(videoContext->codec);
	videoContext->codecContext->codec_id = codecId;
	videoContext->codecContext->width = width;
	videoContext->codecContext->height = height;
	videoContext->codecContext->bit_rate = 400000; // dummy
	videoContext->codecContext->time_base = timebase;
	videoContext->codecContext->pix_fmt = pixelFormat;

	if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		videoContext->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	int error = avcodec_parameters_from_context(videoContext->stream->codecpar, videoContext->codecContext);

	/* Set up RGB -> YUV converter */
	FrameFilterOptions options;
	options.media_type = AVMEDIA_TYPE_VIDEO;
	options.width = width;
	options.height = height;
	options.pix_fmt = PLUGIN_VIDEO_PIX_FORMAT;
	options.time_base = timebase;
	options.sar.den = 1;
	options.sar.num = 1;

	/* Get pixel format name */
	char filterString[256];
	const char *pixFmtName = av_get_pix_fmt_name(pixelFormat);
	sprintf_s(filterString, "vflip,format=pix_fmts=%s", pixFmtName);

	/* Configure video frame filter output format */
	videoContext->frameFilter = new FrameFilter();
	videoContext->frameFilter->configure(options, filterString);

	/* Set colorspace and -range */
	videoContext->codecContext->colorspace = AVColorSpace::AVCOL_SPC_BT709;
	videoContext->codecContext->color_range = AVColorRange::AVCOL_RANGE_MPEG;
	videoContext->codecContext->color_primaries = AVColorPrimaries::AVCOL_PRI_BT709;
	videoContext->codecContext->color_trc = AVColorTransferCharacteristic::AVCOL_TRC_BT709;
}

void Encoder::setAudioCodec(AVCodecID codecId, csSDK_int64 channelLayout, csSDK_int64 bitrate, int sampleRate, csSDK_int32 frame_size)
{
	audioContext = new AVContext;

	/* Find codec */
	audioContext->codec = avcodec_find_encoder(codecId);
	if (audioContext->codec == NULL)
	{
		return;
	}

	/* Configure the audio encoder */
	audioContext->codecContext = avcodec_alloc_context3(audioContext->codec);
	if (audioContext->codecContext == NULL)
	{
		return;
	}

	/* Configure context */
	audioContext->codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	audioContext->codecContext->channels = av_get_channel_layout_nb_channels(channelLayout);
	audioContext->codecContext->channel_layout = channelLayout;
	audioContext->codecContext->sample_rate = sampleRate;
	audioContext->codecContext->sample_fmt = audioContext->codec->sample_fmts ? audioContext->codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
	audioContext->codecContext->bit_rate = bitrate;

	/* Create the stream */
	audioContext->stream = avformat_new_stream(formatContext, NULL);
	audioContext->stream->id = formatContext->nb_streams - 1;
	audioContext->stream->time_base.den = audioContext->codecContext->sample_rate;
	audioContext->stream->time_base.num = 1;
	
	if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		audioContext->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	int error = avcodec_parameters_from_context(audioContext->stream->codecpar, audioContext->codecContext);

	/* Set up audio filters */
	FrameFilterOptions options;
	options.media_type = AVMEDIA_TYPE_AUDIO;
	options.channel_layout = channelLayout;
	options.sample_fmt = PLUGIN_AUDIO_SAMPLE_FORMAT;
	options.time_base = { 1, audioContext->codecContext->sample_rate };

	char filterConfig[256];
	sprintf_s(filterConfig, "aformat=channel_layouts=stereo:sample_fmts=%s:sample_rates=%d", av_get_sample_fmt_name(audioContext->codecContext->sample_fmt), audioContext->codecContext->sample_rate);

	/* Create the audio filter */
	audioContext->frameFilter = new FrameFilter();
	audioContext->frameFilter->configure(options, filterConfig);
}

int Encoder::open(const char *videoOptions, const char *audioOptions)
{
	int ret;

	/* Configure x264 encoder  */
	AVDictionary *options = NULL;
	av_dict_set(&options, "x264-params", videoOptions, 0);
		
	/* Open video stream */
	if ((ret = openStream(videoContext, options)) < 0)
	{
		return ret;
	}

	/* Open audio stream */
	if ((ret = openStream(audioContext, NULL)) > 0)
	{
		return ret;
	}

	/* Create audio fifo buffer */
	if (!(fifo = av_audio_fifo_alloc(audioContext->codecContext->sample_fmt, audioContext->codecContext->channels, 1))) 
	{
		return AVERROR_EXIT;
	}

	/* Open the target file */
	avio_open(&formatContext->pb, filename, AVIO_FLAG_WRITE);
	avformat_write_header(formatContext, NULL);

	return S_OK;
}

void Encoder::close()
{
	/* Write trailer */
	av_write_trailer(formatContext);

	if (videoContext->frameFilter != NULL)
	{
		videoContext->frameFilter->~FrameFilter();
	}

	avcodec_close(videoContext->codecContext);

	if (audioContext->frameFilter != NULL)
	{
		audioContext->frameFilter->~FrameFilter();
	}

	avcodec_close(videoContext->codecContext);
	avcodec_close(audioContext->codecContext);

	/* Close the file */
	avio_close(formatContext->pb);
}

int Encoder::openStream(AVContext *context, AVDictionary *options)
{
	int ret;

	/* Open the codec */
	if ((ret = avcodec_open2(context->codecContext, context->codec, &options)) < 0)
	{
		return ret ;
	}

	/* Copy the stream parameters to the context */
	if ((ret = avcodec_parameters_from_context(context->stream->codecpar, context->codecContext)) < 0)
	{
		return ret;
	}

	return S_OK;
}

int Encoder::writeVideoFrame(char *data)
{
	int ret;

	/* Do we just want to flush the encoder? */
	if (data == NULL)
	{
		/* Send the frame to the encoder */
		if ((ret = encodeAndWriteFrame(videoContext, NULL)) < 0)
		{
			return ret;
		}

		return S_OK;
	}

	/* Create a new frame */
	AVFrame *frame = av_frame_alloc();
	frame->width = videoContext->codecContext->width;
	frame->height = videoContext->codecContext->height;
	frame->format = PLUGIN_VIDEO_PIX_FORMAT;

	/* Reserve buffer space */
	if ((ret = av_frame_get_buffer(frame, 32)) < 0)
	{
		return ret;
	}

	/* Fill the source frame */
	frame->data[0] = (uint8_t*)data;

	/* Presentation timestamp */
	frame->pts = videoContext->next_pts++;
	
	/* Send the frame to the encoder */
	if ((ret = encodeAndWriteFrame(videoContext, frame)) < 0)
	{
		av_frame_free(&frame);
		return ret;
	}

	av_frame_free(&frame);

	return S_OK;
}

int Encoder::writeAudioFrame(const uint8_t **data, int32_t sampleCount)
{
	int ret, err;
	bool finished = true;

	/* Do we have samples to add to the buffer? */
	if (data != NULL)
	{
		finished = false;

		/* Resize the buffer so it can store all samples */
		if ((err = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + sampleCount)) < 0)
		{
			return AVERROR_EXIT;
		}

		/* Add the new samples to the buffer */
		if (av_audio_fifo_write(fifo, (void **)data, sampleCount) < sampleCount)
		{
			return AVERROR_EXIT;
		}
	}

	/* Do we have enough samples for the encoder? */
	while (av_audio_fifo_size(fifo) >= audioContext->codecContext->frame_size || (finished && av_audio_fifo_size(fifo) > 0))
	{
		AVFrame *frame;

		const int frame_size = FFMIN(av_audio_fifo_size(fifo), audioContext->codecContext->frame_size);

		frame = av_frame_alloc();
		frame->nb_samples = frame_size;
		frame->channel_layout = audioContext->codecContext->channel_layout;
		frame->format = PLUGIN_AUDIO_SAMPLE_FORMAT;
		frame->sample_rate = audioContext->codecContext->sample_rate;

		/* Allocate the buffer for the frame */
		if ((err = av_frame_get_buffer(frame, 0)) < 0)
		{
			av_frame_free(&frame);
			return AVERROR_EXIT;
		}

		/* Fill buffers with data from the fifo */
		if (av_audio_fifo_read(fifo, (void **)frame->data, frame_size) < frame_size)
		{
			av_frame_free(&frame);
			return AVERROR_EXIT;
		}

		frame->pts = audioContext->next_pts;
		audioContext->next_pts += frame->nb_samples;

		/* Send the frame to the encoder */
		if ((ret = encodeAndWriteFrame(audioContext, frame)) < 0)
		{
			av_frame_free(&frame);
			return ret;
		}

		av_frame_free(&frame);
	}

	if (finished) 
	{
		/* Flush the encoder */
		if ((ret = encodeAndWriteFrame(audioContext, NULL)) < 0)
		{
			return ret;
		}
	}

	return S_OK;
}

int Encoder::encodeAndWriteFrame(AVContext *context, AVFrame *frame)
{
	int ret;

	/* Is a frame filter set? */
	if (context->frameFilter && frame != NULL)
	{
		/* Send the frame to the frameFilter */
		if ((ret = context->frameFilter->sendFrame(frame)) < 0)
		{
			return AVERROR_EXIT;
		}

		AVFrame *tmp_frame;
		tmp_frame = av_frame_alloc();

		/* */
		while ((ret = context->frameFilter->receiveFrame(tmp_frame)) >= 0)
		{
			/* Send the frame to the encoder */
			if ((ret = avcodec_send_frame(context->codecContext, tmp_frame)) < 0)
			{
				av_frame_free(&tmp_frame);
				return AVERROR_EXIT;
			}
			av_frame_free(&tmp_frame);
		}
	}
	else
	{
		/* Send the frame to the encoder */
		if ((ret = avcodec_send_frame(context->codecContext, frame)) < 0)
		{
			return AVERROR_EXIT;
		}
	}

	AVPacket *packet = av_packet_alloc();

	/* If we receive a packet from the encoder write it to the stream */
	while ((ret = avcodec_receive_packet(context->codecContext, packet)) >= 0)
	{
		av_packet_rescale_ts(packet, context->codecContext->time_base, context->stream->time_base);
		packet->stream_index = context->stream->index;
		av_interleaved_write_frame(formatContext, packet);
		av_packet_unref(packet);
	}

	return S_OK;
}