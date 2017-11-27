#include <windows.h>
#include <chrono>
#include <cmath>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/clamp.hpp>
#include <boost/locale.hpp>
#include <boost/spirit/include/qi.hpp>
#include "Encoder.h"
#include "Voukoder.h"
#include "Common.h"
#include "resource.h"
#include "x264.h"
#include "ConfigImportDialog.h"

static void avlog_cb(void *, int level, const char * szFmt, va_list varg)
{
	char logbuf[2000];
	vsnprintf(logbuf, sizeof(logbuf), szFmt, varg);
	logbuf[sizeof(logbuf) - 1] = '\0';

	OutputDebugStringA(logbuf);
}

DllExport PREMPLUGENTRY xSDKExport(csSDK_int32 selector, exportStdParms *stdParmsP, void *param1, void	*param2)
{
	switch (selector)
	{
		case exSelStartup:					return exStartup(stdParmsP, reinterpret_cast<exExporterInfoRec*>(param1));
		case exSelShutdown:					return exShutdown();
		case exSelBeginInstance:			return exBeginInstance(stdParmsP, reinterpret_cast<exExporterInstanceRec*>(param1));
		case exSelGenerateDefaultParams:	return exGenerateDefaultParams(stdParmsP, reinterpret_cast<exGenerateDefaultParamRec*>(param1));
		case exSelPostProcessParams:		return exPostProcessParams(stdParmsP, reinterpret_cast<exPostProcessParamsRec*>(param1));
		case exSelValidateParamChanged:		return exValidateParamChanged(stdParmsP, reinterpret_cast<exParamChangedRec*>(param1));
		case exSelGetParamSummary:          return exGetParamSummary(stdParmsP, reinterpret_cast<exParamSummaryRec*>(param1));
		case exSelEndInstance:				return exEndInstance(stdParmsP, reinterpret_cast<exExporterInstanceRec*>(param1));
		case exSelExport:					return exExport(stdParmsP, reinterpret_cast<exDoExportRec*>(param1));
		case exSelQueryExportFileExtension: return exFileExtension(stdParmsP, reinterpret_cast<exQueryExportFileExtensionRec*>(param1));
		//case exSelQueryOutputFileList
		case exSelQueryOutputSettings:		return exQueryOutputSettings(stdParmsP, reinterpret_cast<exQueryOutputSettingsRec*>(param1));
		case exSelValidateOutputSettings:   return exValidateOutputSettings(stdParmsP, reinterpret_cast<exValidateOutputSettingsRec*>(param1));
		case exSelParamButton:				return exParamButton(stdParmsP, reinterpret_cast<exParamButtonRec*>(param1));
	}

	return exportReturn_Unsupported;
}

prMALError exStartup(exportStdParms *stdParmsP, exExporterInfoRec *infoRecP)
{
	av_log_set_level(AV_LOG_DEBUG);
	av_log_set_callback(avlog_cb);

	av_register_all();
	avfilter_register_all();
	
	infoRecP->fileType = 'X264';
	prUTF16CharCopy(infoRecP->fileTypeName, PLUGIN_APPNAME);
	prUTF16CharCopy(infoRecP->fileTypeDefaultExtension, L"mkv");
	infoRecP->classID = 'VOUK';
	infoRecP->exportReqIndex = 0;
	infoRecP->wantsNoProgressBar = kPrFalse;
	infoRecP->hideInUI = kPrFalse;
	infoRecP->doesNotSupportAudioOnly = kPrTrue;
	infoRecP->canExportVideo = kPrTrue;
	infoRecP->canExportAudio = kPrTrue;
	infoRecP->interfaceVersion = EXPORTMOD_VERSION;
	infoRecP->isCacheable = kPrFalse;

	return malNoError;
}

prMALError exShutdown()
{
	// Maybe needed later

	return malNoError;
}

prMALError exBeginInstance(exportStdParms *stdParmsP, exExporterInstanceRec *instanceRecP)
{
	prMALError result = malNoError;

	/* Get the basic suite */
	SPBasicSuite *spBasic = stdParmsP->getSPBasicSuite();
	if (spBasic != NULL)
	{
		/* Get the memory manager */
		PrSDKMemoryManagerSuite *memorySuite;
		SPErr spError = spBasic->AcquireSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&memorySuite)));

		/* Create the export settings */
		InstanceRec *instRec = reinterpret_cast<InstanceRec *>(memorySuite->NewPtrClear(sizeof(InstanceRec)));
		if (instRec)
		{
			instRec->spBasic = spBasic;
			instRec->memorySuite = memorySuite;

			spError = spBasic->AcquireSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->timeSuite))));
			spError = spBasic->AcquireSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->exportParamSuite))));
			spError = spBasic->AcquireSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->exportInfoSuite))));
			spError = spBasic->AcquireSuite(kPrSDKSequenceAudioSuite, kPrSDKSequenceAudioSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->sequenceAudioSuite))));
			spError = spBasic->AcquireSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->sequenceRenderSuite))));
			spError = spBasic->AcquireSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->exportFileSuite))));
			spError = spBasic->AcquireSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->ppixSuite))));
			spError = spBasic->AcquireSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->ppix2Suite))));
			spError = spBasic->AcquireSuite(kPrSDKExportProgressSuite, kPrSDKExportProgressSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->exportProgressSuite))));
			spError = spBasic->AcquireSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion, const_cast<const void**>(reinterpret_cast<void**>(&(instRec->windowSuite))));

			// Get the DLL module handle
			MEMORY_BASIC_INFORMATION mbi;
			static int dummy;
			VirtualQuery(&dummy, &mbi, sizeof(mbi));
			instRec->hInstance = reinterpret_cast<HMODULE>(mbi.AllocationBase);

			instRec->settings = new Settings(instRec->hInstance);
			instRec->videoConfig = new EncoderConfig(instRec->exportParamSuite, instanceRecP->exporterPluginID);
			instRec->audioConfig = new EncoderConfig(instRec->exportParamSuite, instanceRecP->exporterPluginID);
		}

		instanceRecP->privateData = reinterpret_cast<void*>(instRec);
	}
	else
	{
		result = exportReturn_ErrMemory;
	}

	return result;
}

prMALError exEndInstance(exportStdParms *stdParmsP, exExporterInstanceRec *instanceRecP)
{
	prMALError result = malNoError;
	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(instanceRecP->privateData);
	SPBasicSuite *spBasic = instRec->spBasic;

	if (spBasic != NULL)
	{
		if (instRec->timeSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		}

		if (instRec->exportParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion);
		}

		if (instRec->exportInfoSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion);
		}

		if (instRec->sequenceAudioSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceAudioSuite, kPrSDKSequenceAudioSuiteVersion);
		}

		if (instRec->sequenceRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion);
		}

		if (instRec->exportFileSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion);
		}

		if (instRec->ppixSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		}

		if (instRec->ppix2Suite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		}

		if (instRec->exportProgressSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportProgressSuite, kPrSDKExportProgressSuiteVersion);
		}
		
		if (instRec->windowSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}

		if (instRec->memorySuite)
		{
			instRec->memorySuite->PrDisposePtr(reinterpret_cast<PrMemoryPtr>(instRec));
			result = spBasic->ReleaseSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion);
		}
		
		instRec->videoConfig->~EncoderConfig();
		instRec->videoConfig = NULL;

		instRec->audioConfig->~EncoderConfig();
		instRec->audioConfig = NULL;

		instRec->settings->~Settings();
		instRec->settings = NULL;
	}

	return malNoError;
}

prMALError exFileExtension(exportStdParms *stdParmsP, exQueryExportFileExtensionRec *exportFileExtensionRecP)
{
	csSDK_uint32 exID = exportFileExtensionRecP->exporterPluginID;
	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(exportFileExtensionRecP->privateData);
	Settings *settings = instRec->settings;

	// Get the users multiplexer choice
	exParamValues multiplexer;
	instRec->exportParamSuite->GetParamValue(exID, 0, FFMultiplexer, &multiplexer);

	// Get selected muxer
	const json muxers = settings->getMuxers();
	const json muxer = Settings::filterArrayById(muxers, multiplexer.value.intValue);

	// Set the right extension
	if (!muxer.empty())
	{
		const std::string extension = muxer["extension"].get<std::string>();
		const std::wstring widestr = std::wstring(extension.begin(), extension.end());
		prUTF16CharCopy(exportFileExtensionRecP->outFileExtension, widestr.c_str());

		return malNoError;
	}

	return malUnknownError;
}

prMALError exQueryOutputSettings(exportStdParms *stdParmsP, exQueryOutputSettingsRec *outputSettingsP)
{
	prMALError result = malNoError;

	csSDK_uint32 exID = outputSettingsP->exporterPluginID;
	csSDK_int32 mgroupIndex = 0;
	exParamValues width, height, frameRate, pixelAspectRatio, fieldType, codec, sampleRate, channelType;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(outputSettingsP->privateData);
	Settings *settings = instRec->settings;
	PrSDKExportParamSuite *paramSuite = instRec->exportParamSuite;

	float fps = 0.0f;

	if (outputSettingsP->inExportVideo)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		outputSettingsP->outVideoWidth = width.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		outputSettingsP->outVideoHeight = height.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
	}
	if (outputSettingsP->inExportAudio)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_32BitFloat;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		outputSettingsP->outAudioChannelType = (PrAudioChannelType)channelType.value.intValue;
	}

	// Calculate bitrate
	PrTime ticksPerSecond = 0;
	csSDK_uint32 videoBitrate = 0, audioBitrate = 0;
	if (outputSettingsP->inExportVideo)
	{
		instRec->timeSuite->GetTicksPerSecond(&ticksPerSecond);
		fps = static_cast<float>(ticksPerSecond) / frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoCodec, &codec);
		videoBitrate = static_cast<csSDK_uint32>(width.value.intValue * height.value.intValue * 4 * fps); //TODO
	}
	if (outputSettingsP->inExportAudio)
	{
		audioBitrate = static_cast<csSDK_uint32>(sampleRate.value.floatValue * 4 * 2); //TODO
	}
	outputSettingsP->outBitratePerSecond = (videoBitrate + audioBitrate) * 8 / 1000;

	//TODO: Add proper bitrate calculation if possible

	return result;
}

prMALError exExport(exportStdParms *stdParmsP, exDoExportRec *exportInfoP)
{
	prMALError result = malNoError;

	csSDK_uint32 exID = exportInfoP->exporterPluginID;
	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(exportInfoP->privateData);

	// Get the file name 
	prUTF16Char prFilename[kPrMaxPath];
	csSDK_int32 prFilenameLength = kPrMaxPath;
	size_t i;
	instRec->exportFileSuite->GetPlatformPath(exportInfoP->fileObject, &prFilenameLength, prFilename);
	char *filename = (char*)malloc(kPrMaxPath);
	wcstombs_s(&i, filename, (size_t)kPrMaxPath, prFilename, (size_t)kPrMaxPath);
		
	// Do encoding loops
	instRec->maxPasses = instRec->videoConfig->getMaxPasses();
	for (instRec->currentPass = 1; instRec->currentPass <= instRec->maxPasses; instRec->currentPass++)
	{
		// Create encoder instance
		Encoder *encoder = new Encoder(NULL, filename);
		result = SetupEncoderInstance(instRec, exID, encoder);
		
		// Open the encoder
		if (encoder->open() == S_OK)
		{
			// Render and write all video and audio frames
			result = RenderAndWriteAllFrames(exportInfoP, encoder);

			// Flush the encoders
			encoder->writeVideoFrame(NULL);
			encoder->writeAudioFrame(NULL, 0);

			encoder->close(true);
		}

		encoder->~Encoder();
		encoder = NULL;
	}

	return result;
}

prMALError exGenerateDefaultParams(exportStdParms *stdParms, exGenerateDefaultParamRec *generateDefaultParamRec)
{
	prMALError result = malNoError;
	PrParam seqWidth, seqHeight, seqPARNum, seqPARDen, seqFrameRate, seqFieldOrder, seqChannelType, seqSampleRate;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(generateDefaultParamRec->privateData);
	PrSDKExportParamSuite *exportParamSuite = instRec->exportParamSuite;
	PrSDKExportInfoSuite *exportInfoSuite = instRec->exportInfoSuite;
	PrSDKTimeSuite *timeSuite = instRec->timeSuite;
	Settings *settings = instRec->settings;

	csSDK_int32 exID = generateDefaultParamRec->exporterPluginID;
	csSDK_int32 groupIndex = 0;

	// Get the values from the sequence
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoWidth, &seqWidth);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoHeight, &seqHeight);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectNumerator, &seqPARNum);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectDenominator, &seqPARDen);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFrameRate, &seqFrameRate);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFieldType, &seqFieldOrder);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioSampleRate, &seqSampleRate);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioChannelsType, &seqChannelType);

	// Width
	if (seqWidth.mInt32 == 0)
	{
		seqWidth.mInt32 = 1920;
	}

	// Height
	if (seqHeight.mInt32 == 0)
	{
		seqHeight.mInt32 = 1080;
	}

	exportParamSuite->AddMultiGroup(exID, &groupIndex);

#pragma region Group: Basic Video Settings

	// Video tab group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBETopParamGroup, ADBEVideoTabGroup, L"Video", kPrFalse, kPrFalse, kPrFalse);

	// Basic settings group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBEVideoTabGroup, ADBEBasicVideoGroup, L"Basic Video Settings", kPrFalse, kPrFalse, kPrFalse);

	// Video Encoder
	exParamValues videoCodecValues;
	videoCodecValues.structVersion = 1;
	videoCodecValues.value.intValue = settings->getDefaultVideoCodecId();
	videoCodecValues.disabled = kPrFalse;
	videoCodecValues.hidden = kPrFalse;
	videoCodecValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo videoCodecParam;
	videoCodecParam.structVersion = 1;
	videoCodecParam.flags = exParamFlag_none;
	videoCodecParam.paramType = exParamType_int;
	videoCodecParam.paramValues = videoCodecValues;
	::lstrcpyA(videoCodecParam.identifier, ADBEVideoCodec);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &videoCodecParam);

	// Video width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.value.intValue = seqWidth.mInt32;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 4096;
	widthValues.disabled = kPrFalse;
	widthValues.hidden = kPrFalse;
	widthValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo widthParam;
	widthParam.structVersion = 1;
	widthParam.flags = exParamFlag_none;
	widthParam.paramType = exParamType_int;
	widthParam.paramValues = widthValues;
	::lstrcpyA(widthParam.identifier, ADBEVideoWidth);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &widthParam);

	// Video height
	exParamValues heightValues;
	heightValues.structVersion = 1;
	heightValues.value.intValue = seqHeight.mInt32;
	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 4096;
	heightValues.disabled = kPrFalse;
	heightValues.hidden = kPrFalse;
	heightValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo heightParam;
	heightParam.structVersion = 1;
	heightParam.flags = exParamFlag_none;
	heightParam.paramType = exParamType_int;
	heightParam.paramValues = heightValues;
	::lstrcpyA(heightParam.identifier, ADBEVideoHeight);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &heightParam);

	// Pixel aspect ratio
	exParamValues PARValues;
	PARValues.structVersion = 1;
	PARValues.value.ratioValue.numerator = seqPARNum.mInt32;
	PARValues.value.ratioValue.denominator = seqPARDen.mInt32;
	PARValues.rangeMin.ratioValue.numerator = 10;
	PARValues.rangeMin.ratioValue.denominator = 11;
	PARValues.rangeMax.ratioValue.numerator = 2;
	PARValues.rangeMax.ratioValue.denominator = 1;
	PARValues.disabled = kPrFalse;
	PARValues.hidden = kPrFalse;
	PARValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo PARParam;
	PARParam.structVersion = 1;
	PARParam.flags = exParamFlag_none;
	PARParam.paramType = exParamType_ratio;
	PARParam.paramValues = PARValues;
	::lstrcpyA(PARParam.identifier, ADBEVideoAspect);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &PARParam);

	// Video frame rate
	exParamValues frameRateValues;
	frameRateValues.structVersion = 1;
	frameRateValues.value.timeValue = seqFrameRate.mInt64;
	frameRateValues.rangeMin.timeValue = 1;
	timeSuite->GetTicksPerSecond(&frameRateValues.rangeMax.timeValue);
	frameRateValues.disabled = kPrFalse;
	frameRateValues.hidden = kPrFalse;
	frameRateValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo frameRateParam;
	frameRateParam.structVersion = 1;
	frameRateParam.flags = exParamFlag_none;
	frameRateParam.paramType = exParamType_ticksFrameRate;
	frameRateParam.paramValues = frameRateValues;
	::lstrcpyA(frameRateParam.identifier, ADBEVideoFPS);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &frameRateParam);

	// Field order
	exParamValues fieldOrderValues;
	fieldOrderValues.structVersion = 1;
	fieldOrderValues.value.intValue = seqFieldOrder.mInt32;
	fieldOrderValues.disabled = kPrFalse;
	fieldOrderValues.hidden = kPrFalse;
	fieldOrderValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo fieldOrderParam;
	fieldOrderParam.structVersion = 1;
	fieldOrderParam.flags = exParamFlag_none;
	fieldOrderParam.paramType = exParamType_int;
	fieldOrderParam.paramValues = fieldOrderValues;
	::lstrcpyA(fieldOrderParam.identifier, ADBEVideoFieldType);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &fieldOrderParam);

	// TV standard
	exParamValues tvStandardValues;
	tvStandardValues.structVersion = 1;
	tvStandardValues.value.intValue = seqFieldOrder.mInt32;
	tvStandardValues.disabled = kPrFalse;
	tvStandardValues.hidden = kPrFalse;
	tvStandardValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo tvStandardParam;
	tvStandardParam.structVersion = 1;
	tvStandardParam.flags = exParamFlag_none;
	tvStandardParam.paramType = exParamType_int;
	tvStandardParam.paramValues = tvStandardValues;
	::lstrcpyA(tvStandardParam.identifier, VKDRTVStandard);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &tvStandardParam);

	// Color space
	exParamValues colorSpaceValues;
	colorSpaceValues.structVersion = 1;
	colorSpaceValues.value.intValue = vkdrBT709;
	colorSpaceValues.disabled = kPrFalse;
	colorSpaceValues.hidden = kPrFalse;
	colorSpaceValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo colorSpaceParam;
	colorSpaceParam.structVersion = 1;
	colorSpaceParam.flags = exParamFlag_none;
	colorSpaceParam.paramType = exParamType_int;
	colorSpaceParam.paramValues = colorSpaceValues;
	::lstrcpyA(colorSpaceParam.identifier, VKDRColorSpace);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &colorSpaceParam);

	// Color range
	exParamValues colorRangeValues;
	colorRangeValues.structVersion = 1;
	colorRangeValues.value.intValue = vkdrLimitedColorRange;
	colorRangeValues.disabled = kPrFalse;
	colorRangeValues.hidden = kPrFalse;
	colorRangeValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo colorRangeParam;
	colorRangeParam.structVersion = 1;
	colorRangeParam.flags = exParamFlag_none;
	colorRangeParam.paramType = exParamType_bool;
	colorRangeParam.paramValues = colorRangeValues;
	::lstrcpyA(colorRangeParam.identifier, VKDRColorRange);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicVideoGroup, &colorRangeParam);

#pragma endregion

	// Advanced video codec tab
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBETopParamGroup, VKDRAdvVideoCodecTabGroup, L"Advanced", kPrFalse, kPrFalse, kPrFalse);

#pragma region Group: Video Encoder Options

	// Basic settings group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBEVideoTabGroup, ADBEVideoCodecGroup, L"Basic Encoder Settings", kPrFalse, kPrFalse, kPrFalse);

	// Populate video encoder options
	json videoCodecs = settings->getVideoEncoders();
	CreateEncoderParamElements(exportParamSuite, exID, groupIndex, videoCodecs, videoCodecValues.value.intValue);

#pragma endregion

#pragma region Group: Basic Audio Settings

	// Audio tab group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBETopParamGroup, ADBEAudioTabGroup, L"Audio", kPrFalse, kPrFalse, kPrFalse);

	// Basic audio settings group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBEAudioTabGroup, ADBEBasicAudioGroup, L"Basic Audio Settings", kPrFalse, kPrFalse, kPrFalse);

	// Audio Codec
	exParamValues audioCodecValues;
	audioCodecValues.structVersion = 1;
	audioCodecValues.value.intValue = settings->getDefaultAudioCodecId();
	audioCodecValues.disabled = kPrFalse;
	audioCodecValues.hidden = kPrFalse;
	audioCodecValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo audioCodecParam;
	audioCodecParam.structVersion = 1;
	audioCodecParam.flags = exParamFlag_none;
	audioCodecParam.paramType = exParamType_int;
	audioCodecParam.paramValues = audioCodecValues;
	::lstrcpyA(audioCodecParam.identifier, ADBEAudioCodec);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicAudioGroup, &audioCodecParam);

	// Sample rate
	exParamValues sampleRateValues;
	sampleRateValues.structVersion = 1;
	sampleRateValues.value.floatValue = seqSampleRate.mFloat64;
	sampleRateValues.disabled = kPrFalse;
	sampleRateValues.hidden = kPrFalse;
	sampleRateValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo sampleRateParam;
	sampleRateParam.structVersion = 1;
	sampleRateParam.flags = exParamFlag_none;
	sampleRateParam.paramType = exParamType_float;
	sampleRateParam.paramValues = sampleRateValues;
	::lstrcpyA(sampleRateParam.identifier, ADBEAudioRatePerSecond);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicAudioGroup, &sampleRateParam);

	// Channels
	exParamValues channelsValues;
	channelsValues.structVersion = 1;
	channelsValues.value.intValue = seqChannelType.mInt32;
	channelsValues.disabled = kPrFalse;
	channelsValues.hidden = kPrFalse;
	channelsValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo channelsParam;
	channelsParam.structVersion = 1;
	channelsParam.flags = exParamFlag_none;
	channelsParam.paramType = exParamType_int;
	channelsParam.paramValues = channelsValues;
	::lstrcpyA(channelsParam.identifier, ADBEAudioNumChannels);
	exportParamSuite->AddParam(exID, groupIndex, ADBEBasicAudioGroup, &channelsParam);

#pragma endregion

#pragma region Group: Audio Encoder Options

	// Basic settings group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBEAudioTabGroup, ADBEAudioCodecGroup, L"Encoder Options", kPrFalse, kPrFalse, kPrFalse);

	// Populate audio encoder options
	json audioCodecs = settings->getAudioEncoders();
	CreateEncoderParamElements(exportParamSuite, exID, groupIndex, audioCodecs, audioCodecValues.value.intValue);

#pragma endregion

#pragma region Group: Multiplexer

	// Multiplexer tab group
	exportParamSuite->AddParamGroup(exID, groupIndex, ADBETopParamGroup, FFMultiplexerTabGroup, L"Multiplexer", kPrFalse, kPrFalse, kPrFalse);

	// Multiplexer settings group
	exportParamSuite->AddParamGroup(exID, groupIndex, FFMultiplexerTabGroup, FFMultiplexerBasicGroup, L"Multiplexer Settings", kPrFalse, kPrFalse, kPrFalse);

	// Multiplexer
	exParamValues multiplexerValues;
	multiplexerValues.structVersion = 1;
	multiplexerValues.value.intValue = settings->getDefaultMuxerId();
	multiplexerValues.disabled = kPrFalse;
	multiplexerValues.hidden = kPrFalse;
	multiplexerValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo multiplexerParam;
	multiplexerParam.structVersion = 1;
	multiplexerParam.flags = exParamFlag_multiLine;
	multiplexerParam.paramType = exParamType_int;
	multiplexerParam.paramValues = multiplexerValues;
	::lstrcpyA(multiplexerParam.identifier, FFMultiplexer);
	exportParamSuite->AddParam(exID, groupIndex, FFMultiplexerBasicGroup, &multiplexerParam);

#pragma endregion

	exportParamSuite->SetParamsVersion(exID, 1);

	return result;
}

prMALError exPostProcessParams(exportStdParms *stdParmsP, exPostProcessParamsRec *postProcessParamsRecP)
{
	prMALError result = malNoError;
	prUTF16Char paramString[kPrMaxName];
	exOneParamValueRec tempParamValue;

	const csSDK_int32		PARs[][2] = {
		{ 1, 1 },			// Square pixels (1.0)
		{ 10, 11 },		// D1/DV NTSC (0.9091)
		{ 40, 33 },		// D1/DV NTSC Widescreen 16:9 (1.2121)
		{ 768, 702 },		// D1/DV PAL (1.0940)
		{ 1024, 702 },	// D1/DV PAL Widescreen 16:9 (1.4587)
		{ 2, 1 },			// Anamorphic 2:1 (2.0)
		{ 4, 3 },			// HD Anamorphic 1080 (1.3333)
		{ 3, 2 }			// DVCPRO HD (1.5)
	};
	const wchar_t* const	PARStrings[] = {
		L"Square pixels (1.0)",
		L"D1/DV NTSC (0.9091)",
		L"D1/DV NTSC Widescreen 16:9 (1.2121)",
		L"D1/DV PAL (1.0940)",
		L"D1/DV PAL Widescreen 16:9 (1.4587)",
		L"Anamorphic 2:1 (2.0)",
		L"HD Anamorphic 1080 (1.3333)",
		L"DVCPRO HD (1.5)"
	};

	const csSDK_int32		fieldOrders[] = {
		prFieldsNone,
		prFieldsUpperFirst,
		prFieldsLowerFirst
	};
	const wchar_t* const	fieldOrderStrings[] = {
		L"None (Progressive)",
		L"Upper First",
		L"Lower First"
	};

	csSDK_int32 exID = postProcessParamsRecP->exporterPluginID;
	csSDK_int32 groupIndex = 0;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(postProcessParamsRecP->privateData);
	Settings *settings = instRec->settings;

	// Get various settings
	json configuration = settings->getConfiguration();
	json videoEncoders = settings->getVideoEncoders();
	json audioEncoders = settings->getAudioEncoders();
	json muxers = settings->getMuxers();

	instRec->exportParamSuite->SetParamName(exID, groupIndex, VKDRAdvVideoCodecTabGroup, L"Advanced");

#pragma region Group: Video settings

	// Label elements
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEBasicVideoGroup, L"Video settings");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoCodec, L"Video Codec");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoWidth, L"Width");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoHeight, L"Height");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoAspect, L"Pixel Aspect Ratio");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoFPS, L"Frame Rate");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, VKDRTVStandard, L"TV Standard");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEVideoFieldType, L"Field Order");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, VKDRColorSpace, L"Color Space");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, VKDRColorRange, L"Use full color range");

	// Populate video encoder dropdown
	PopulateEncoders(instRec, exID, groupIndex, ADBEVideoCodec, videoEncoders);

	// Pixel aspect ratio
	instRec->exportParamSuite->ClearConstrainedValues(exID, 0, ADBEVideoAspect);
	exOneParamValueRec tempPAR;
	for (csSDK_int32 i = 0; i < sizeof(PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		swprintf(paramString, kPrMaxName, L"%s", PARStrings[i]);
		instRec->exportParamSuite->AddConstrainedValuePair(exID, 0, ADBEVideoAspect, &tempPAR, paramString);
	}

	// Get ticks per second
	PrTime ticksPerSecond = 0;
	instRec->timeSuite->GetTicksPerSecond(&ticksPerSecond);

	// Clear the framerate dropdown
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, ADBEVideoFPS);

	// Populate framerate dropdown
	exOneParamValueRec tempFPS;
	for (auto iterator = configuration["videoFramerates"].begin(); iterator != configuration["videoFramerates"].end(); ++iterator)
	{
		json framerate = *iterator;

		int num = framerate["num"].get<int>();
		int den = framerate["den"].get<int>();

		tempFPS.timeValue = ticksPerSecond / num * den;

		std::string name = framerate["name"].get<std::string>();
		swprintf(paramString, kPrMaxName, L"%hs", name.c_str());
		instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, ADBEVideoFPS, &tempFPS, paramString);
	}

	// TV Standard
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, VKDRTVStandard);
	exOneParamValueRec tempTvStandard;
	tempTvStandard.intValue = vkdrPAL;
	swprintf(paramString, kPrMaxName, L"%s", L"PAL");
	instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, VKDRTVStandard, &tempTvStandard, paramString);
	tempTvStandard.intValue = vkdrNTSC;
	swprintf(paramString, kPrMaxName, L"%s", L"NTSC");
	instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, VKDRTVStandard, &tempTvStandard, paramString);

	// Field orders
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, ADBEVideoFieldType);
	exOneParamValueRec tempOrder;
	for (csSDK_int32 i = 0; i < sizeof(fieldOrders) / sizeof(fieldOrders[0]); i++)
	{
		tempOrder.intValue = fieldOrders[i];
		swprintf(paramString, kPrMaxName, L"%s", fieldOrderStrings[i]);
		instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, ADBEVideoFieldType, &tempOrder, paramString);
	}

	// Color Space
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, VKDRColorSpace);
	exOneParamValueRec tempcolorSpace;
	tempcolorSpace.intValue = vkdrBT601;
	swprintf(paramString, kPrMaxName, L"%s", L"BT.601 (SD)");
	instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, VKDRColorSpace, &tempcolorSpace, paramString);
	tempcolorSpace.intValue = vkdrBT709;
	swprintf(paramString, kPrMaxName, L"%s", L"BT.709 (HD)");
	instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, VKDRColorSpace, &tempcolorSpace, paramString);

#pragma endregion

#pragma region Group: Video encoder settings

	instRec->exportParamSuite->SetParamName(exID, 0, ADBEVideoCodecGroup, L"Codec settings");

	PopulateParamValues(instRec, exID, groupIndex, videoEncoders);

#pragma endregion

#pragma region Group: Audio settings

	// Label elements
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEBasicAudioGroup, L"Audio settings");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEAudioCodec, L"Audio Codec");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEAudioRatePerSecond, L"Sample Rate");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, ADBEAudioNumChannels, L"Channels");

	// Populate audio encoder dropdown
	PopulateEncoders(instRec, exID, groupIndex, ADBEAudioCodec, audioEncoders);

	// Clear the samplerate dropdown
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, ADBEAudioRatePerSecond);

	// Populate the samplerate dropdown
	exOneParamValueRec oneParamValueRec;
	for (auto iterator = configuration["audioSampleRates"].begin(); iterator != configuration["audioSampleRates"].end(); ++iterator)
	{
		json sampleRate = *iterator;

		oneParamValueRec.floatValue = sampleRate["id"].get<double>();

		std::string name = sampleRate["name"].get<std::string>();
		swprintf(paramString, kPrMaxName, L"%hs", name.c_str());
		instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, ADBEAudioRatePerSecond, &oneParamValueRec, paramString);
	}

	// Clear the channels dropdown
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, ADBEAudioNumChannels);

	// Populate the channels dropdown
	exOneParamValueRec oneParamValueRec2;
	for (auto iterator = configuration["audioChannels"].begin(); iterator != configuration["audioChannels"].end(); ++iterator)
	{
		json channels = *iterator;

		oneParamValueRec2.intValue = channels["id"].get<int>();

		std::string name = channels["name"].get<std::string>();
		swprintf(paramString, kPrMaxName, L"%hs", name.c_str());
		instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, ADBEAudioNumChannels, &oneParamValueRec2, paramString);
	}

#pragma endregion

#pragma region Group: Audio encoder settings

	// Label elements
	instRec->exportParamSuite->SetParamName(exID, 0, ADBEAudioCodecGroup, L"Codec settings");

	// Populate audio encoder dropdown
	PopulateParamValues(instRec, exID, groupIndex, audioEncoders);

#pragma endregion

#pragma region Group: Multiplexer

	// Label elements
	instRec->exportParamSuite->SetParamName(exID, groupIndex, FFMultiplexerTabGroup, L"Multiplexer");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, FFMultiplexerBasicGroup, L"Container");
	instRec->exportParamSuite->SetParamName(exID, groupIndex, FFMultiplexer, L"Format");

	// Clear the multiplexer dropdown
	instRec->exportParamSuite->ClearConstrainedValues(exID, groupIndex, FFMultiplexer);

	// Populate the multiplexer dropdown
	AVOutputFormat *format;
	for (auto iterator = muxers.begin(); iterator != muxers.end(); ++iterator)
	{
		json item = *iterator;
		std::string name = item["name"].get<std::string>();

		format = av_guess_format(name.c_str(), NULL, NULL);
		if (format != NULL)
		{
			tempParamValue.intValue = item["id"].get<int>();
			swprintf(paramString, kPrMaxName, L"%hs", format->long_name);
			instRec->exportParamSuite->AddConstrainedValuePair(exID, groupIndex, FFMultiplexer, &tempParamValue, paramString);
		}
	}

#pragma endregion

	return result;
}

prMALError exValidateParamChanged(exportStdParms *stdParmsP, exParamChangedRec *validateParamChangedRecP)
{
	prMALError result = malNoError;
	exParamValues tempValue;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(validateParamChangedRecP->privateData);
	Settings *settings = instRec->settings;

	csSDK_uint32 exID = validateParamChangedRecP->exporterPluginID;
	csSDK_uint32 groupIndex = validateParamChangedRecP->multiGroupIndex;

	// Get the changed value
	exParamValues changedValue;
	instRec->exportParamSuite->GetParamValue(exID, groupIndex, validateParamChangedRecP->changedParamIdentifier, &changedValue);

	// What has changed?
	const char* paramName = validateParamChangedRecP->changedParamIdentifier;

	// Is it a encoder selection element?
	if (strcmp(paramName, ADBEVideoCodec) == 0 || strcmp(paramName, ADBEAudioCodec) == 0)
	{
		json encoders;

		// Get the encoders
		if (strcmp(paramName, ADBEVideoCodec) == 0)
		{
			encoders = settings->getVideoEncoders();
		}
		else if (strcmp(paramName, ADBEAudioCodec) == 0)
		{
			encoders = settings->getAudioEncoders();
		}

		// Iterate all encoders
		for (auto itEncoder = encoders.begin(); itEncoder != encoders.end(); ++itEncoder)
		{
			const json encoder = *itEncoder;

			// Should these options be hidden?
			const bool isSelected = encoder["id"].get<int>() == changedValue.value.intValue;

			for (auto itParam = encoder["params"].begin(); itParam != encoder["params"].end(); ++itParam)
			{
				const json param = *itParam;

				const std::string name = param["name"].get<std::string>();

				// Change the visibility of an element
				instRec->exportParamSuite->GetParamValue(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);
				tempValue.hidden = isSelected ? kPrFalse : kPrTrue;
				instRec->exportParamSuite->ChangeParam(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);

				// Is this a normal dropdown element? (TODO: Improve this check)
				if (param["flags"].size() == 0 && param.find("values") != param.end())
				{
					int selectedId = tempValue.value.intValue;

					// Handle the elements suboptions
					for (auto itValue = param["values"].begin(); itValue != param["values"].end(); ++itValue)
					{
						const json value = *itValue;

						// Do we have suboptions at all?
						if (value.find("subvalues") != value.end())
						{
							// Should this suboption be visible
							const bool isValueSelected = isSelected && value["id"].get<int>() == selectedId;

							// Iterate these suboptions
							for (auto itSubvalues = value["subvalues"].begin(); itSubvalues != value["subvalues"].end(); ++itSubvalues)
							{
								const json suboption = *itSubvalues;

								const std::string name = suboption["name"].get<std::string>();

								// Change the elements visibility
								instRec->exportParamSuite->GetParamValue(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);
								tempValue.hidden = isValueSelected ? kPrFalse : kPrTrue;
								instRec->exportParamSuite->ChangeParam(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);
							}
						}
					}
				}
			}
		}
	}
	else // Dynamic created elements
	{
		// Try to find the selection in one option of the codecs
		json param = Settings::find(settings->getVideoEncoders(), "name", paramName);
		if (param.empty())
		{
			param = Settings::find(settings->getAudioEncoders(), "name", paramName);
			if (param.empty())
			{
				return result;
			}
		}

		// Iterate these options
		for (auto itValue = param["values"].begin(); itValue != param["values"].end(); ++itValue)
		{
			const json value = *itValue;

			if (value.find("subvalues") != value.end())
			{
				// Should this suboption be visible
				const bool isSelected = value["id"].get<int>() == changedValue.value.intValue;

				// Iterate these suboptions
				for (auto itSubvalue = value["subvalues"].begin(); itSubvalue != value["subvalues"].end(); ++itSubvalue)
				{
					const json subvalue = *itSubvalue;

					const std::string name = subvalue["name"].get<std::string>();

					// Change the elements visibility
					instRec->exportParamSuite->GetParamValue(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);
					tempValue.hidden = isSelected ? kPrFalse : kPrTrue;
					instRec->exportParamSuite->ChangeParam(exID, validateParamChangedRecP->multiGroupIndex, name.c_str(), &tempValue);
				}
			}
		}
	}

	return result;
}

prMALError exGetParamSummary(exportStdParms *stdParmsP, exParamSummaryRec *summaryRecP)
{
	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(summaryRecP->privateData);
	Settings *settings = instRec->settings;
	
#pragma region Video Summary

	exParamValues videoCodec;
	instRec->exportParamSuite->GetParamValue(summaryRecP->exporterPluginID, 0, ADBEVideoCodec, &videoCodec);

	// Init video encoder configuration
	json videoEncoders = instRec->settings->getVideoEncoders();
	json videoEncoder = instRec->settings->filterArrayById(videoEncoders, videoCodec.value.intValue);
	instRec->videoConfig->initFromSettings(videoEncoder);

	std::string videoName = videoEncoder["name"].get<std::string>();
	std::string videoSummary = videoName + " (" + instRec->videoConfig->getConfigAsParamString("-") + ")";
	prUTF16CharCopy(summaryRecP->videoSummary, string2wchar_t(videoSummary));

#pragma endregion

#pragma region Audio Summary

	exParamValues audioCodec;
	instRec->exportParamSuite->GetParamValue(summaryRecP->exporterPluginID, 0, ADBEAudioCodec, &audioCodec);

	// Init audio encoder configuration
	json audioEncoders = instRec->settings->getAudioEncoders();
	json audioEncoder = instRec->settings->filterArrayById(audioEncoders, audioCodec.value.intValue);
	instRec->audioConfig->initFromSettings(audioEncoder);

	std::string audioName = audioEncoder["name"].get<std::string>();
	std::string audioSummary = audioName + " (" + instRec->audioConfig->getConfigAsParamString("-") + ")";
	prUTF16CharCopy(summaryRecP->audioSummary, string2wchar_t(audioSummary));

#pragma endregion

	prUTF16CharCopy(summaryRecP->bitrateSummary, L"");

	return malNoError;
}

prMALError exValidateOutputSettings(exportStdParms *stdParmsP, exValidateOutputSettingsRec *validateOutputSettingsRec)
{
	prMALError result = malNoError;

	csSDK_uint32 exID = validateOutputSettingsRec->exporterPluginID;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(validateOutputSettingsRec->privateData);
	Settings *settings = instRec->settings;

	// Get the users multiplexer choice
	exParamValues multiplexer;
	instRec->exportParamSuite->GetParamValue(exID, 0, FFMultiplexer, &multiplexer);

	// Get selected muxer
	const json muxers = settings->getMuxers();
	const json muxer = Settings::filterArrayById(muxers, multiplexer.value.intValue);

	// Set the right extension
	if (muxer.empty())
	{
		return malUnknownError;
	}

	const std::string name = muxer["name"].get<std::string>();

	// Create encoder instance
	Encoder *encoder = new Encoder(name.c_str(), NULL);
	result = SetupEncoderInstance(instRec, exID, encoder);

	// Open the encoder
	if (encoder->open() == S_OK)
	{
		// Test successful
		encoder->close(false);
	}
	else
	{
		wchar_t buffer[4096];
		swprintf_s(buffer, L"%s\n\n%S",
			PLUGIN_ERR_COMBINATION_NOT_SUPPORTED,
			encoder->dumpConfiguration());

		// Show an error message to the user
		ShowMessageBox(instRec, buffer, PLUGIN_APPNAME, MB_OK);

		result = exportReturn_ErrLastErrorSet;
	}

	encoder->~Encoder();
	encoder = NULL;

	return result;
}

INT_PTR CALLBACK DialogProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	char *buffer;
	int len;

	switch (Msg)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_IMPORT:
			EndDialog(hWndDlg, 0);
			len = SendMessage(hWndDlg, WM_GETTEXTLENGTH, 0, 0);
			buffer = new char[len];
			SendMessage(hWndDlg, WM_GETTEXT, (WPARAM)len + 1, (LPARAM)buffer);
			//result.assign(buffer, len);
			return TRUE;

		case IDC_CANCEL:
			EndDialog(hWndDlg, 0);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(hWndDlg, 0);
		return TRUE;
	}

	return FALSE;
}

prMALError exParamButton(exportStdParms *stdParmsP, exParamButtonRec *getFilePrefsRecP)
{
	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(getFilePrefsRecP->privateData);
	
	HWND mainWnd = instRec->windowSuite->GetMainWindow();

	/*
	ConfigImportDialog dialog;
	if (dialog.show(instRec->hInstance, mainWnd))
	{
		std::string commandLine = dialog.getValue();


	}
	*/

	return malNoError;
}

void loadSettings(json *settings)
{
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE hModule = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	HRSRC hRes = FindResourceEx(hModule, MAKEINTRESOURCE(ID_TEXT), MAKEINTRESOURCE(IDR_ID_TEXT1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
	if (NULL != hRes)
	{
		HGLOBAL hData = LoadResource(hModule, hRes);
		if (NULL != hData)
		{
			const DWORD dataSize = SizeofResource(hModule, hRes);
			char *resource = new char[dataSize + 1];
			memcpy(resource, LockResource(hData), dataSize);
			resource[dataSize] = 0;

			*settings = json::parse(resource);
		}
	}
}

prMALError SetupEncoderInstance(InstanceRec *instRec, csSDK_uint32 exID, Encoder *encoder)
{
	prMALError result = malNoError;

	Settings *settings = instRec->settings;
	EncoderConfig *videoConfig = instRec->videoConfig;
	EncoderConfig *audioConfig = instRec->audioConfig;

	// Export video params
	exParamValues videoCodec, videoWidth, videoHeight, pixelAspectRatio, fieldType, tvStandard, vkdrColorSpace, vkdrColorRange, ticksPerFrame, audioCodec, channelType, audioSampleRate, audioBitrate, multipass;
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoCodec, &videoCodec);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &videoWidth);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &videoHeight);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &pixelAspectRatio);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFieldType, &fieldType);
	instRec->exportParamSuite->GetParamValue(exID, 0, VKDRTVStandard, &tvStandard);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFPS, &ticksPerFrame);
	instRec->exportParamSuite->GetParamValue(exID, 0, VKDRColorSpace, &vkdrColorSpace);
	instRec->exportParamSuite->GetParamValue(exID, 0, VKDRColorRange, &vkdrColorRange);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioCodec, &audioCodec);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioNumChannels, &channelType);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &audioSampleRate);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioBitrate, &audioBitrate);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioBitrate, &audioBitrate);

	// Find the correct fps ratio
	PrTime c = gcd(254016000000, ticksPerFrame.value.timeValue);
	int num = 254016000000 / c;
	int den = ticksPerFrame.value.timeValue / c;

#pragma region Get video encoder settings

	const int videoCodecId = videoCodec.value.intValue;

	// Get the selected codec
	json videoCodecs = settings->getVideoEncoders();
	json vEncoder = settings->filterArrayById(videoCodecs, videoCodecId);

	videoConfig->initFromSettings(vEncoder);

#pragma endregion

#pragma region Get audio encoder settings

	const int audioCodecId = audioCodec.value.intValue;

	// Get the selected codec
	json audioCodecs = settings->getAudioEncoders();
	json aEncoder = settings->filterArrayById(audioCodecs, audioCodecId);

	audioConfig->initFromSettings(aEncoder);

#pragma endregion

	// Translate the channel layout to AVLib
	csSDK_int64 channelLayout;
	switch (channelType.value.intValue)
	{
	case kPrAudioChannelType_Mono:
		channelLayout = AV_CH_LAYOUT_MONO;
		break;
	case kPrAudioChannelType_51:
		channelLayout = AV_CH_LAYOUT_5POINT1;
		break;
	default:
		channelLayout = AV_CH_LAYOUT_STEREO;
		break;
	}

	// Get the selected encoders
	const std::string videoEncoder = vEncoder["name"].get<std::string>();
	const std::string audioEncoder = aEncoder["name"].get<std::string>();

	// Get the right color range
	AVColorRange colorRange = vkdrColorRange.value.intValue == vkdrFullColorRange ? AVColorRange::AVCOL_RANGE_JPEG : AVColorRange::AVCOL_RANGE_MPEG;

	AVColorSpace colorSpace = AVColorSpace::AVCOL_SPC_UNSPECIFIED;
	AVColorPrimaries colorPrimaries = AVColorPrimaries::AVCOL_PRI_UNSPECIFIED;
	AVColorTransferCharacteristic colorTransferCharacteristic = AVColorTransferCharacteristic::AVCOL_TRC_UNSPECIFIED;

	// Color conversion values
	if (vkdrColorSpace.value.intValue == vkdrBT601)
	{
		if (tvStandard.value.intValue == vkdrPAL)
		{
			colorSpace = AVColorSpace::AVCOL_SPC_BT470BG;
			colorPrimaries = AVColorPrimaries::AVCOL_PRI_BT470BG; 
			colorTransferCharacteristic = AVColorTransferCharacteristic::AVCOL_TRC_GAMMA28;
		}
		else if (tvStandard.value.intValue == vkdrNTSC)
		{
			colorSpace = AVColorSpace::AVCOL_SPC_SMPTE170M;
			colorPrimaries = AVColorPrimaries::AVCOL_PRI_SMPTE170M;
			colorTransferCharacteristic = AVColorTransferCharacteristic::AVCOL_TRC_SMPTE170M;
		}
	}
	else if (vkdrColorSpace.value.intValue == vkdrBT709)
	{
		colorSpace = AVColorSpace::AVCOL_SPC_BT709;
		colorPrimaries = AVColorPrimaries::AVCOL_PRI_BT709;
		colorTransferCharacteristic = AVColorTransferCharacteristic::AVCOL_TRC_BT709;
	}

	int flags = 0;

	// Multipass encoding
	if (instRec->maxPasses > 1)
	{
		json parameters;

		if (instRec->currentPass == 1)
		{
			flags = AV_CODEC_FLAG_PASS1;
		}
		else
		{
			flags = AV_CODEC_FLAG_PASS2;
		}
	}

	// Configure an encoder instance
	encoder->setVideoCodec(videoEncoder, videoConfig, videoWidth.value.intValue, videoHeight.value.intValue, { den, num }, colorSpace, colorRange, colorPrimaries, colorTransferCharacteristic, flags);
	encoder->setAudioCodec(audioEncoder, audioConfig, channelLayout, (int)audioSampleRate.value.floatValue);
	
	return result;
}

prMALError RenderAndWriteAllFrames(exDoExportRec *exportInfoP, Encoder *encoder)
{
	prMALError result = malNoError;

	csSDK_uint32 exID = exportInfoP->exporterPluginID;

	InstanceRec *instRec = reinterpret_cast<InstanceRec *>(exportInfoP->privateData);
	EncoderConfig *videoConfig = instRec->videoConfig;

	// Get render parameter values
	exParamValues videoWidth, videoHeight, pixelAspectRatio, fieldType, colorSpace, colorRange, ticksPerFrame, channelType, audioSampleRate;
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &videoWidth);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &videoHeight);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &pixelAspectRatio);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFieldType, &fieldType);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFPS, &ticksPerFrame);
	instRec->exportParamSuite->GetParamValue(exID, 0, VKDRColorSpace, &colorSpace);
	instRec->exportParamSuite->GetParamValue(exID, 0, VKDRColorRange, &colorRange);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioNumChannels, &channelType);
	instRec->exportParamSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &audioSampleRate);

	// Make audio renderer
	PrTime ticksPerSample;
	instRec->timeSuite->GetTicksPerAudioSample(audioSampleRate.value.floatValue, &ticksPerSample);
	instRec->sequenceAudioSuite->MakeAudioRenderer(exID, exportInfoP->startTime, (PrAudioChannelType)channelType.value.intValue, kPrAudioSampleType_32BitFloat, audioSampleRate.value.floatValue, &instRec->audioRenderID);
	
	// Get the audio frame size we need to send to the encoder
	instRec->sequenceAudioSuite->GetMaxBlip(instRec->audioRenderID, ticksPerFrame.value.timeValue, &instRec->maxBlip);

	PrTime exportDuration = exportInfoP->endTime - exportInfoP->startTime;
	PrTime audioSamplesLeft = exportDuration / ticksPerSample;

	// Make video renderer
	instRec->sequenceRenderSuite->MakeVideoRenderer(exID, &instRec->videoRenderID, ticksPerFrame.value.timeValue);

	// Find the right pixel format
	const char* encPixelFormat = videoConfig->getPixelFormat();
	PrPixelFormat pixelFormat = GetPremierePixelFormat(encPixelFormat, fieldType.value.intValue, colorSpace.value.intValue, colorRange.value.intValue);
	const PrPixelFormat pixelFormats[] = { pixelFormat, PrPixelFormat_BGRA_4444_8u, PrPixelFormat_Any };

	// Define the render params
	SequenceRender_ParamsRec renderParms;
	renderParms.inRequestedPixelFormatArray = pixelFormats;
	renderParms.inRequestedPixelFormatArrayCount = sizeof(pixelFormats) / sizeof(pixelFormats[0]);
	renderParms.inWidth = videoWidth.value.intValue;
	renderParms.inHeight = videoHeight.value.intValue;
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatio.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatio.value.ratioValue.denominator;
	renderParms.inRenderQuality = kPrRenderQuality_High;
	renderParms.inFieldType = fieldType.value.intValue;
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = kPrRenderQuality_High;
	renderParms.inCompositeOnBlack = kPrFalse;

	// Cache the rendered frames when using multipass
	const PrRenderCacheType cacheType = instRec->maxPasses > 1 ? kRenderCacheType_RenderedStillFrames : kRenderCacheType_None;

	EncodingData encodingData = {};

	instRec->sequenceAudioSuite->ResetAudioToBeginning(instRec->audioRenderID);

	// Export loop
	PrTime videoTime = exportInfoP->startTime;
	while (videoTime <= (exportInfoP->endTime - ticksPerFrame.value.timeValue))
	{
		FrameType frameType = encoder->getNextFrameType();
		if (FrameType::VideoFrame == frameType || audioSamplesLeft <= 0)
		{
			// Render the uncompressed frame
			SequenceRender_GetFrameReturnRec renderResult;
			//result = instRec->sequenceRenderSuite->RenderVideoFrame(instRec->videoRenderID, videoTime, &renderParms, cacheType, &renderResult);
			result = instRec->sequenceRenderSuite->RenderVideoFrameAndConformToPixelFormat(instRec->videoRenderID, videoTime, &renderParms, cacheType, pixelFormat, &renderResult);
			if (result != malNoError)
			{
				ShowMessageBox(instRec, L"Error: Required pixel format has not been requested in render params.", PLUGIN_APPNAME, MB_OK);

				return result;
			}

			// Get pixel format
			PrPixelFormat format;
			result = instRec->ppixSuite->GetPixelFormat(renderResult.outFrame, &format);

			// Planar YUV 4:2:0 8bit formats
			if (format == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601 ||
				format == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601 ||
				format == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange ||
				format == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange ||
				format == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709 ||
				format == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709 ||
				format == PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange ||
				format == PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange)
			{
				encodingData.pix_fmt = "yuv420p";

				// Get planar buffers
				result = instRec->ppix2Suite->GetYUV420PlanarBuffers(
					renderResult.outFrame,
					PrPPixBufferAccess_ReadOnly,
					&encodingData.plane[0],
					&encodingData.stride[0],
					&encodingData.plane[1],
					&encodingData.stride[1],
					&encodingData.plane[2],
					&encodingData.stride[2]);

				// Send to encoder
				if (encoder->writeVideoFrame(&encodingData) != S_OK)
				{
					result = malUnknownError;
					break;
				}
			}
			else if (format == PrPixelFormat_VUYA_4444_32f ||
				format == PrPixelFormat_VUYA_4444_32f_709)
			{
				// Pixel format planes
				const csSDK_int32 planes = 4;

				// Prepare encoding data
				encodingData.pix_fmt = "yuva444p16le";
				for (int i = 0; i < planes; i++)
				{
					encodingData.stride[i] = videoWidth.value.intValue * sizeof(uint16_t);
					encodingData.plane[i] = (char *)instRec->memorySuite->NewPtr(videoHeight.value.intValue * encodingData.stride[i]);
				}

				// Get premiere rowsize
				csSDK_int32 rowBytes;
				instRec->ppixSuite->GetRowBytes(renderResult.outFrame, &rowBytes);

				// Get pixels from the renderer
				char *pixels;
				instRec->ppixSuite->GetPixels(renderResult.outFrame, PrPPixBufferAccess_ReadOnly, &pixels);

				float *frameBuffer = (float*)pixels;

				// VUYA to YUVA mapping
				const int map[4] = { 2, 1, 0, 3 };

				// Scaling factors
				const float yuv[4][4] = {
					{ -0.07306f, 1.09132f,  0.00000f, 1.00000f }, // Y
					{ -0.57143f, 0.57143f, -0.50000f, 0.50000f }, // U
					{ -0.57143f, 0.57143f, -0.50000f, 0.50000f }, // V
					{  0.00000f, 1.00000f,  0.00000f, 1.00000f }  // A
				};

				// Full color range or limited color range?
				//int fullColorOffset = (colorRange.value.intValue == vkdrFullColorRange) ? 2 : 0;

				// Convert 
				for (int r = 0; r < videoHeight.value.intValue; r++)
				{
					for (int c = 0; c < videoWidth.value.intValue; c++)
					{
						for (int plane = 0; plane < planes; plane++)
						{
							const int pos = (videoHeight.value.intValue - r) * (rowBytes / sizeof(float)) + c * planes + plane;
							const float floatValue = frameBuffer[pos];

							const int dplane = map[plane];
							const float min = yuv[dplane][0];
							const float max = yuv[dplane][1];

							// Convert float value to uint16_t
							boost::algorithm::clamp(floatValue, min, max);
							const uint16_t intValue = (uint16_t)((floatValue + abs(min)) / (abs(min) + abs(max)) * 65535.0f);

							// Write to plane buffers
							int dpos = r * encodingData.stride[dplane] + c * sizeof(uint16_t);
							encodingData.plane[dplane][dpos + 0] = intValue & 0xff;
							encodingData.plane[dplane][dpos + 1] = intValue >> 8;
						}
					}
				}

				// Send to encoder
				if (encoder->writeVideoFrame(&encodingData) != S_OK)
				{
					result = malUnknownError;
					break;
				}

				// Free buffers
				for (int i = 0; i < planes; i++)
				{
					instRec->memorySuite->PrDisposePtr((char *)encodingData.plane[i]);
				}
			}
			else
			{
				ShowMessageBox(instRec, L"Error: Rendered frame is not in a supported pixel format. This should never happen.", PLUGIN_APPNAME, MB_OK);

				result = malUnknownError;
				break;
			}

			// Dispose the rendered frame
			result = instRec->ppixSuite->Dispose(renderResult.outFrame);

			videoTime += ticksPerFrame.value.timeValue;
		}
		else if (FrameType::AudioFrame == frameType)
		{
			int chunk = audioSamplesLeft;

			// Set chunk size
			if (chunk > instRec->maxBlip)
			{
				chunk = instRec->maxBlip;
			}

			// Allocate output buffer
			float *audioBuffer[MAX_AUDIO_CHANNELS];

			for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
			{
				audioBuffer[i] = (float *)instRec->memorySuite->NewPtr(sizeof(float) * instRec->maxBlip);
			}

			// Get the sample data
			result = instRec->sequenceAudioSuite->GetAudio(instRec->audioRenderID, chunk, audioBuffer, kPrFalse);

			// Send raw data to the encoder
			if (encoder->writeAudioFrame((const uint8_t**)audioBuffer, chunk) != S_OK)
			{
				result = malUnknownError;
				break;
			}

			audioSamplesLeft -= chunk;

			// Free output buffer
			for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
			{
				instRec->memorySuite->PrDisposePtr((char *)audioBuffer[i]);
			}
		}

		// Update progress & handle export cancellation
		float offset = (1.0f / static_cast<float>(instRec->maxPasses)) * (static_cast<float>(instRec->currentPass) - 1);
		float progress = offset + static_cast<float>(videoTime - exportInfoP->startTime) / static_cast<float>(exportDuration) / static_cast<float>(instRec->maxPasses);
		result = instRec->exportProgressSuite->UpdateProgressPercent(exID, progress);
		if (result == suiteError_ExporterSuspended)
		{
			instRec->exportProgressSuite->WaitForResume(exID);
		}
		else if (result == exportReturn_Abort)
		{
			exportDuration = videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime < exportDuration ? videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime : exportDuration;
			break;
		}
	}

	// Release renderers
	instRec->sequenceRenderSuite->ReleaseVideoRenderer(exID, instRec->videoRenderID);
	instRec->sequenceAudioSuite->ReleaseAudioRenderer(exID, instRec->audioRenderID);
	
	return result;
}

PrPixelFormat GetPremierePixelFormat(const char *format, prFieldType fieldType, vkdrColorSpace colorSpace, vkdrColorRange colorRange)
{
	if (strcmp(format, "yuv422p10le") == 0 ||
		strcmp(format, "yuv444p10le") == 0)
	{
		switch (colorSpace)
		{
		case vkdrBT601:
			return PrPixelFormat_VUYA_4444_32f;

		case vkdrBT709:
			return PrPixelFormat_VUYA_4444_32f_709;
		}
	}
	else if (strcmp(format, "yuv420p") == 0)
	{
		if (fieldType == prFieldsNone)
		{
			if (colorRange == vkdrLimitedColorRange)
			{
				switch (colorSpace)
				{
				case vkdrBT601:
					return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601;

				case vkdrBT709:
					return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709;
				}
			}
			else if (colorRange == vkdrFullColorRange)
			{
				switch (colorSpace)
				{
				case vkdrBT601:
					return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange;

				case vkdrBT709:
					return PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange;
				}
			}
		}
		else
		{
			if (colorRange == vkdrLimitedColorRange)
			{
				switch (colorSpace)
				{
				case vkdrBT601:
					return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601;

				case vkdrBT709:
					return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709;
				}
			}
			else if (colorRange == vkdrFullColorRange)
			{
				switch (colorSpace)
				{
				case vkdrBT601:
					return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange;

				case vkdrBT709:
					return PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange;
				}
			}
		}
	}

	return PrPixelFormat_Any;
}