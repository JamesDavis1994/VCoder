#pragma once

#include <string>
#include "Encoder.h"
#include "EncoderData.h"
#include "EncoderContext.h"
#include "ExportSettings.h"
#include "lavf.h"

#define LIB_VKDR_APPNAME "LibVKDR 0.9.1 (www.voukoder.org)"

using namespace std;

namespace LibVKDR
{
	enum FrameType {
		VideoFrame,
		AudioFrame
	};

	class Encoder
	{
	public:
		Encoder(ExportSettings exportSettings);
		~Encoder();
		int open();
		void close(bool writeTrailer);
		void flushContext(EncoderContext *encoderContext);
		int getAudioFrameSize();
		int testSettings();
		int writeVideoFrame(EncoderData *encoderData);
		int writeAudioFrame(float **data, int32_t sampleCount);
		FrameType getNextFrameType();

	private:
		ExportSettings exportSettings;
		AVFormatContext *formatContext;
		EncoderContext videoContext;
		EncoderContext audioContext;
		int createCodecContext(string codecName, EncoderContext *encoderContext);
		int encodeAndWriteFrame(EncoderContext *context, AVFrame *frame, FrameFilter *frameFilter);
		int openCodec(string codecName, const string options, EncoderContext *encoderContext, int flags);
		int prepare(EncoderData *encoderData);
		int pass;
		int audioFrameSize;
	};
}