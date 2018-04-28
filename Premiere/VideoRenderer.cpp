#include "VideoRenderer.h"
#include "InstructionSet.h"
#include <tmmintrin.h>
#include <immintrin.h>

#if defined(_DEBUG) 

#include <chrono>
#include <sstream>

using namespace std::chrono;

static time_point<steady_clock> tp1, tp2, tp3, tp4, tp5;

#endif

static const __m128i unpackMask8 = _mm_set_epi8(
	12, 8, 4, 0, // Y
	13, 9, 5, 1, // U
	14, 10, 6, 2, // V
	15, 11, 7, 3  // A (not needed)
);

static const __m128 scale_mul = _mm_setr_ps(
	57342.98164f,
	57342.98164f,
	56283.17216f,
	65535.0f
);

static const __m128 scale_add = _mm_setr_ps(
	32767.0f,
	32767.0f,
	4112.04855f,
	0.0f
);

VideoRenderer::VideoRenderer(csSDK_uint32 videoRenderID, csSDK_uint32 width, csSDK_uint32 height, PrSDKPPixSuite *ppixSuite, PrSDKPPix2Suite *ppix2Suite, PrSDKMemoryManagerSuite *memorySuite, PrSDKExporterUtilitySuite *exporterUtilitySuite) :
	videoRenderID(videoRenderID),
	width(width),
	height(height),
	ppixSuite(ppixSuite),
	ppix2Suite(ppix2Suite),
	memorySuite(memorySuite),
	exporterUtilitySuite(exporterUtilitySuite)
{
	for (int i = 0; i < MAX_FRAME_PLANES; i++)
	{
		encodingData.plane[i] = (char*)memorySuite->NewPtr(width * height * 4);
	}
}

VideoRenderer::~VideoRenderer()
{
	for (int i = 0; i < MAX_FRAME_PLANES; i++)
	{
		memorySuite->PrDisposePtr(encodingData.plane[i]);
	}
}

void VideoRenderer::unpackUint8(uint8_t *pixels, int rowBytes, uint8_t *bufferY, uint8_t *bufferU, uint8_t *bufferV)
{
	for (int r = height - 1; r >= 0; r--)
	{
		const uint8_t *p = &pixels[r * rowBytes];

		for (int c = 0; c < rowBytes; c += 16)
		{
			const __m128i yuva = _mm_loadu_si128((__m128i*)(p + c));
			const __m128i out = _mm_shuffle_epi8(yuva, unpackMask8);
			memcpy(bufferY += 4, out.m128i_u8 + 4, 4);
			memcpy(bufferU += 4, out.m128i_u8 + 8, 4);
			memcpy(bufferV += 4, out.m128i_u8 + 12, 4);
		}
	}
}

// Thanks to Peter Cordes
static inline __m128i load_and_scale(const float *src, const bool useFMA)
{  
	__m128 srcv = _mm_loadu_ps(src);
	__m128 scaled = useFMA ?
		_mm_fmadd_ps(srcv, scale_mul, scale_add) : 
		_mm_add_ps(_mm_mul_ps(srcv, scale_mul), scale_add);

	return _mm_cvttps_epi32(scaled);
}

// Thanks to Peter Cordes
void VideoRenderer::unpackFloatToUint16(float* pixels, uint16_t *bufferY, uint16_t *bufferU, uint16_t *bufferV, uint16_t *bufferA)
{
	const bool useFMA = InstructionSet::FMA();

	for (int r = height - 1; r >= 0; r--)
	{
		const float* p = &pixels[r * width * 4];

		for (int c = 0; c < (int)width * 4; c += 16)
		{
			__m128i vuya0 = load_and_scale(p + c, useFMA);
			__m128i vuya1 = load_and_scale(p + c + 4, useFMA);
			__m128i vuya2 = load_and_scale(p + c + 8, useFMA);
			__m128i vuya3 = load_and_scale(p + c + 12, useFMA);

			__m128i vuya02 = _mm_packus_epi32(vuya0, vuya2);
			__m128i vuya13 = _mm_packus_epi32(vuya1, vuya3);
			__m128i vvuuyyaa01 = _mm_unpacklo_epi16(vuya02, vuya13);
			__m128i vvuuyyaa23 = _mm_unpackhi_epi16(vuya02, vuya13);
			__m128i vvvvuuuu = _mm_unpacklo_epi32(vvuuyyaa01, vvuuyyaa23);
			__m128i yyyyaaaa = _mm_unpackhi_epi32(vvuuyyaa01, vvuuyyaa23);
			
			_mm_storel_epi64((__m128i*)bufferV, vvvvuuuu);
			_mm_storeh_pi((__m64*)bufferU, _mm_castsi128_ps(vvvvuuuu));
			_mm_storel_epi64((__m128i*)bufferY, yyyyaaaa);
			_mm_storeh_pi((__m64*)bufferA, _mm_castsi128_ps(yyyyaaaa));

			bufferV += 4;
			bufferU += 4;
			bufferY += 4;
			bufferA += 4;
		}
	}
}

prSuiteError VideoRenderer::frameCompleteCallback(const csSDK_uint32 inWhichPass, const csSDK_uint32 inFrameNumber, const csSDK_uint32 inFrameRepeatCount, PPixHand inRenderedFrame, void* inCallbackData)
{
	prSuiteError error = suiteError_NoError;

#if defined(_DEBUG) 
	tp1 = high_resolution_clock::now();
#endif

	PrPixelFormat inFormat;
	ppixSuite->GetPixelFormat(inRenderedFrame, &inFormat);

	// yuv420p
	if (inFormat == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601 ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601 ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709 ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange ||
		inFormat == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange)
	{
		EncoderData frameData;
		frameData.pass = inWhichPass + 1;
		frameData.planes = 3;
		frameData.pix_fmt = "yuv420p";

		ppix2Suite->GetYUV420PlanarBuffers(
			inRenderedFrame,
			PrPPixBufferAccess_ReadOnly,
			&frameData.plane[0],
			(uint32_t*)&frameData.stride[0],
			&frameData.plane[1],
			(uint32_t*)&frameData.stride[1],
			&frameData.plane[2],
			(uint32_t*)&frameData.stride[2]);

		return frameFinished(&frameData, inFormat, inFrameRepeatCount);
	}

	csSDK_int32 rowBytes;
	ppixSuite->GetRowBytes(inRenderedFrame, &rowBytes);

	char *pixels;
	ppixSuite->GetPixels(inRenderedFrame, PrPPixBufferAccess_ReadOnly, &pixels);

	// UYVY
	if (inFormat == PrPixelFormat_UYVY_422_8u_601 ||
		inFormat == PrPixelFormat_UYVY_422_8u_709)
	{
		EncoderData frameData;
		frameData.pass = inWhichPass + 1;
		frameData.planes = 1;
		frameData.pix_fmt = "uyvy422";
		frameData.plane[0] = pixels;
		frameData.stride[0] = rowBytes;

		return frameFinished(&frameData, inFormat, inFrameRepeatCount);
	}

	// YUVA
	if (inFormat == PrPixelFormat_VUYA_4444_8u ||
		inFormat == PrPixelFormat_VUYA_4444_8u_709)
	{
		// Set output format
		encodingData.pass = inWhichPass + 1;
		encodingData.planes = 3;
		encodingData.pix_fmt = "yuv444p";
		encodingData.stride[0] = encodingData.stride[1] = encodingData.stride[2] = rowBytes / 4;

		// Deinterleave vuya
		unpackUint8(
			(uint8_t*)pixels,
			rowBytes,
			(uint8_t*)encodingData.plane[0],
			(uint8_t*)encodingData.plane[1],
			(uint8_t*)encodingData.plane[2]);

		return frameFinished(&encodingData, inFormat, inFrameRepeatCount);
	}
	
	// VUYA (> 8bit)
	if (inFormat == PrPixelFormat_VUYA_4444_32f ||
		inFormat == PrPixelFormat_VUYA_4444_32f_709)
	{
		encodingData.pass = inWhichPass + 1;
		encodingData.planes = 4;
		encodingData.pix_fmt = "yuva444p16le";
		encodingData.stride[0] = encodingData.stride[1] = encodingData.stride[2] = encodingData.stride[3] = width * 2;

		// Unpack and convert float to uint16_t
		unpackFloatToUint16(
			(float*)pixels,
			(uint16_t*)encodingData.plane[0],
			(uint16_t*)encodingData.plane[1],
			(uint16_t*)encodingData.plane[2],
			(uint16_t*)encodingData.plane[3]);

		return frameFinished(&encodingData, inFormat, inFrameRepeatCount);
	}

	return suiteError_RenderInvalidPixelFormat;
}

prSuiteError VideoRenderer::frameFinished(EncoderData *frameData, PrPixelFormat inFormat, const csSDK_uint32 inFrameRepeatCount)
{
#if defined(_DEBUG) 
	tp2 = high_resolution_clock::now();
#endif

	for (csSDK_uint32 i = 0; i < inFrameRepeatCount; i++)
	{
		exporterUtilitySuite->ReportIntermediateProgressForRepeatedVideoFrame(videoRenderID, 1);

		if (!callback(frameData))
		{
			return suiteError_ExporterSuspended;
		}
	}

#if defined(_DEBUG) 
	tp3 = high_resolution_clock::now();

	stringstream abc;
	abc << "Render: " << ((tp1 - tp4).count() / 1000) << "us, ";
	abc << "Process: " << ((tp2 - tp1).count() / 1000) << "us, ";
	abc << "Encoding: " << ((tp3 - tp2).count() / 1000) << "us, ";
	abc << "Total: " << ((tp3 - tp4).count() / 1000) << "us";
	
	LOG(INFO) << abc.str();
	
	tp4 = high_resolution_clock::now();
#endif

	return suiteError_NoError;
}

prSuiteError VideoRenderer::render(PrPixelFormat pixelFormat, PrTime startTime, PrTime endTime, csSDK_uint32 passes, function<bool(EncoderData*)> callback)
{
	this->width = width;
	this->height = height;
	this->callback = callback;

	ExportLoopRenderParams renderParams;
	renderParams.inStartTime = startTime;
	renderParams.inEndTime = endTime;
	renderParams.inFinalPixelFormat = pixelFormat;
	renderParams.inRenderParamsSize = sizeof(renderParams);
	renderParams.inRenderParamsVersion = 1;
	renderParams.inReservedProgressPreRender = 0;
	renderParams.inReservedProgressPostRender = 0;

	// Create C conform callback
	Callback<prSuiteError(csSDK_uint32, csSDK_uint32, csSDK_uint32, PPixHand, void*)>::func = bind(&VideoRenderer::frameCompleteCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4, placeholders::_5);
	PrSDKMultipassExportLoopFrameCompletionFunction c_callback = static_cast<PrSDKMultipassExportLoopFrameCompletionFunction>(Callback<prSuiteError(csSDK_uint32, csSDK_uint32, csSDK_uint32, PPixHand, void*)>::callback);

	return exporterUtilitySuite->DoMultiPassExportLoop(videoRenderID, &renderParams, passes, c_callback, NULL);
}

PrPixelFormat VideoRenderer::GetTargetRenderFormat(ExportSettings exportSettings)
{
	if (exportSettings.pixelFormat == "yuv420p" || 
		exportSettings.pixelFormat == "nv12")
	{
		if (exportSettings.fieldOrder == AVFieldOrder::AV_FIELD_PROGRESSIVE)
		{
			if (exportSettings.colorSpace == AVColorSpace::AVCOL_SPC_BT709)
			{
				return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709;
			}

			return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601;
		}
		else
		{
			if (exportSettings.colorSpace == AVColorSpace::AVCOL_SPC_BT709)
			{
				return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709;
			}

			return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601;
		}
	}
	else if (exportSettings.pixelFormat == "yuv422p")
	{
		if (exportSettings.colorSpace == AVColorSpace::AVCOL_SPC_BT709)
		{
			return PrPixelFormat_UYVY_422_8u_709;
		}

		return PrPixelFormat_UYVY_422_8u_601;
	}
	else if (exportSettings.pixelFormat == "yuv444p")
	{
		if (exportSettings.colorSpace == AVColorSpace::AVCOL_SPC_BT709)
		{
			return PrPixelFormat_VUYA_4444_8u_709;
		}

		return PrPixelFormat_VUYA_4444_8u;
	}
	else if (exportSettings.pixelFormat == "yuv420p10le" || exportSettings.pixelFormat == "yuv420p12le" ||
		exportSettings.pixelFormat == "yuv422p10le" || exportSettings.pixelFormat == "yuv422p12le" ||
		exportSettings.pixelFormat == "yuv444p10le" || exportSettings.pixelFormat == "yuv444p12le" ||
		exportSettings.pixelFormat == "yuva420p10le" || exportSettings.pixelFormat == "yuva420p12le" ||
		exportSettings.pixelFormat == "yuva422p10le" || exportSettings.pixelFormat == "yuva422p12le" ||
		exportSettings.pixelFormat == "yuva444p10le" || exportSettings.pixelFormat == "yuva444p12le")
	{
		if (exportSettings.colorSpace == AVColorSpace::AVCOL_SPC_BT709)
		{
			return PrPixelFormat_VUYA_4444_32f_709;
		}

		return PrPixelFormat_VUYA_4444_32f;
	}

	return PrPixelFormat_Invalid;
}