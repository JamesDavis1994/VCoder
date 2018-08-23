#include "GUI.h"
#include <Windows.h>

template <class T>
static inline T gcd(T a, T b)
{
	if (a == 0) return b;
	return gcd(b % a, a);
}

GUI::GUI(csSDK_uint32 pluginId, Config *config, PluginUpdate *pluginUpdate, csSDK_int32 paramVersion) :
	pluginId(pluginId),
	config(config),
	pluginUpdate(pluginUpdate),
	paramVersion(paramVersion)
{}

prMALError GUI::createParameters(PrSDKExportParamSuite *exportParamSuite, PrSDKExportInfoSuite *exportInfoSuite, PrSDKTimeSuite *timeSuite)
{
	// Generate groups
	exportParamSuite->AddMultiGroup(pluginId, &groupIndex);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBETopParamGroup, ADBEVideoTabGroup, L"Video", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBETopParamGroup, ADBEAudioTabGroup, L"Audio", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBETopParamGroup, VKDRAdvVideoCodecTabGroup, L"Advanced", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBETopParamGroup, FilterTabGroup, L"Filters", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBETopParamGroup, VKDRMultiplexerTabGroup, L"Multiplexer", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBEVideoTabGroup, ADBEBasicVideoGroup, L"Video Settings", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBEVideoTabGroup, ADBEVideoCodecGroup, L"Encoder Options", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBEAudioTabGroup, ADBEBasicAudioGroup, L"Audio Settings", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, ADBEAudioTabGroup, ADBEAudioCodecGroup, L"Encoder Options", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, VKDRMultiplexerTabGroup, VKDRMultiplexerSettingsGroup, L"Multiplexer Settings", kPrFalse, kPrFalse, kPrFalse);
	exportParamSuite->AddParamGroup(pluginId, groupIndex, VKDRMultiplexerTabGroup, MultiplexerPipeGroup, L"Piping", kPrFalse, kPrFalse, kPrFalse);

	// Get source info
	PrParam seqWidth, seqHeight, seqPARNum, seqPARDen, seqFrameRate, seqFieldOrder, seqChannelType, seqSampleRate;
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_VideoWidth, &seqWidth);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_VideoHeight, &seqHeight);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_PixelAspectNumerator, &seqPARNum);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_PixelAspectDenominator, &seqPARDen);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_VideoFrameRate, &seqFrameRate);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_VideoFieldType, &seqFieldOrder);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_AudioSampleRate, &seqSampleRate);
	exportInfoSuite->GetExportSourceInfo(pluginId, kExportInfo_AudioChannelsType, &seqChannelType);

	if (seqWidth.mInt32 == 0)
		seqWidth.mInt32 = 1920;

	if (seqHeight.mInt32 == 0)
		seqHeight.mInt32 = 1080;

	PrTime ticksPerSecond;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);

	// Find the nearest framerate (esp. important for VFR)
	PrTime previous = ticksPerSecond / config->Framerates.front().num * config->Framerates.front().den;
	for (FramerateInfo framerateInfo : config->Framerates)
	{
		PrTime timeValue = ticksPerSecond / framerateInfo.num * framerateInfo.den;
		if (seqFrameRate.mInt64 < previous && seqFrameRate.mInt64 >= timeValue)
		{
			if ((previous - seqFrameRate.mInt64) < (seqFrameRate.mInt64 - timeValue))
			{
				seqFrameRate.mInt64 = previous;
			}
			else
			{
				seqFrameRate.mInt64 = timeValue;
			}
			break;
		}
	}

	PrTime last = ticksPerSecond / config->Framerates.back().num * config->Framerates.back().den;
	if (seqFrameRate.mInt64 < last)
	{
		seqFrameRate.mInt64 = last;
	}

	// Param: Update button
	exParamValues updateValues;
	updateValues.structVersion = 1;
	updateValues.disabled = kPrFalse;
	updateValues.hidden = pluginUpdate->isUpdateAvailable ? kPrFalse : kPrTrue;
	updateValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo updateParam;
	updateParam.structVersion = 1;
	updateParam.flags = exParamFlag_independant;
	updateParam.paramType = exParamType_button;
	updateParam.paramValues = updateValues;
	::lstrcpyA(updateParam.identifier, VKDRUpdateButton);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &updateParam);

	// Param: Video codec
	exParamValues codecValues;
	codecValues.structVersion = 1;
	codecValues.value.intValue = config->DefaultVideoEncoder;
	codecValues.disabled = kPrFalse;
	codecValues.hidden = kPrFalse;
	codecValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo codecParam;
	codecParam.structVersion = 1;
	codecParam.flags = exParamFlag_none;
	codecParam.paramType = exParamType_int;
	codecParam.paramValues = codecValues;
	::lstrcpyA(codecParam.identifier, ADBEVideoCodec);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &codecParam);

	// Param: Video settings
	exParamValues videoSettingsValues;
	videoSettingsValues.structVersion = 1;
	videoSettingsValues.disabled = kPrFalse;
	videoSettingsValues.hidden = kPrFalse;
	videoSettingsValues.optionalParamEnabled = kPrFalse;
	prUTF16CharCopy(videoSettingsValues.paramString, L"");
	exNewParamInfo videoSettingsParam;
	videoSettingsParam.structVersion = 1;
	videoSettingsParam.flags = exParamFlag_independant;
	videoSettingsParam.paramType = exParamType_string;
	videoSettingsParam.paramValues = videoSettingsValues;
	::lstrcpyA(videoSettingsParam.identifier, VKDRVideoSettings);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &videoSettingsParam);

	// Param: Frame size
	exParamValues videoSizeValues;
	videoSizeValues.structVersion = 1;
	videoSizeValues.value.intValue = 0;
	videoSizeValues.disabled = kPrTrue;
	videoSizeValues.hidden = kPrTrue;
	videoSizeValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo videoSizeParam;
	videoSizeParam.structVersion = 1;
	videoSizeParam.flags = exParamFlag_none;
	videoSizeParam.paramType = exParamType_int;
	videoSizeParam.paramValues = videoSizeValues;
	::lstrcpyA(videoSizeParam.identifier, VKDRVideoFrameSize);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &videoSizeParam);

	// Param: Video width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.value.intValue = seqWidth.mInt32;
	widthValues.rangeMin.intValue = 320;
	widthValues.rangeMax.intValue = 10240;
	widthValues.disabled = kPrFalse;
	widthValues.hidden = kPrFalse;
	widthValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo widthParam;
	widthParam.structVersion = 1;
	widthParam.flags = exParamFlag_none;
	widthParam.paramType = exParamType_int;
	widthParam.paramValues = widthValues;
	::lstrcpyA(widthParam.identifier, ADBEVideoWidth);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &widthParam);

	// Param: Video height
	exParamValues heightValues;
	heightValues.structVersion = 1;
	heightValues.value.intValue = seqHeight.mInt32;
	heightValues.rangeMin.intValue = 240;
	heightValues.rangeMax.intValue = 8192;
	heightValues.disabled = kPrFalse;
	heightValues.hidden = kPrFalse;
	heightValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo heightParam;
	heightParam.structVersion = 1;
	heightParam.flags = exParamFlag_none;
	heightParam.paramType = exParamType_int;
	heightParam.paramValues = heightValues;
	::lstrcpyA(heightParam.identifier, ADBEVideoHeight);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &heightParam);

	// Param: Pixel Aspect Ratio
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
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &PARParam);

	// Param: Video frame rate
	exParamValues frameRateValues;
	frameRateValues.structVersion = 1;
	frameRateValues.value.timeValue = seqFrameRate.mInt64;
	frameRateValues.rangeMin.timeValue = 1;
	frameRateValues.disabled = kPrFalse;
	frameRateValues.hidden = kPrFalse;
	frameRateValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo frameRateParam;
	frameRateParam.structVersion = 1;
	frameRateParam.flags = exParamFlag_none;
	frameRateParam.paramType = exParamType_ticksFrameRate;
	frameRateParam.paramValues = frameRateValues;
	::lstrcpyA(frameRateParam.identifier, ADBEVideoFPS);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &frameRateParam);

	// Param: Field order
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
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &fieldOrderParam);

	// TV standard
	exParamValues tvStandardValues;
	tvStandardValues.structVersion = 1;
	tvStandardValues.value.intValue = TVStandard::PAL;
	tvStandardValues.disabled = kPrFalse;
	tvStandardValues.hidden = kPrFalse;
	tvStandardValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo tvStandardParam;
	tvStandardParam.structVersion = 1;
	tvStandardParam.flags = exParamFlag_none;
	tvStandardParam.paramType = exParamType_int;
	tvStandardParam.paramValues = tvStandardValues;
	::lstrcpyA(tvStandardParam.identifier, VKDRTVStandard);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &tvStandardParam);

	// Color space
	exParamValues colorSpaceValues;
	colorSpaceValues.structVersion = 1;
	colorSpaceValues.value.intValue = ColorSpace::bt709;
	colorSpaceValues.disabled = kPrFalse;
	colorSpaceValues.hidden = kPrFalse;
	colorSpaceValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo colorSpaceParam;
	colorSpaceParam.structVersion = 1;
	colorSpaceParam.flags = exParamFlag_none;
	colorSpaceParam.paramType = exParamType_int;
	colorSpaceParam.paramValues = colorSpaceValues;
	::lstrcpyA(colorSpaceParam.identifier, VKDRColorSpace);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &colorSpaceParam);

	// Color range
	exParamValues colorRangeValues;
	colorRangeValues.structVersion = 1;
	colorRangeValues.value.intValue = ColorRange::Limited;
	colorRangeValues.disabled = kPrFalse;
	colorRangeValues.hidden = kPrTrue;
	colorRangeValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo colorRangeParam;
	colorRangeParam.structVersion = 1;
	colorRangeParam.flags = exParamFlag_none;
	colorRangeParam.paramType = exParamType_bool;
	colorRangeParam.paramValues = colorRangeValues;
	::lstrcpyA(colorRangeParam.identifier, VKDRColorRange);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicVideoGroup, &colorRangeParam);

	// Param: Audio Codec
	exParamValues audioCodecValues;
	audioCodecValues.structVersion = 1;
	audioCodecValues.value.intValue = config->DefaultAudioEncoder;
	audioCodecValues.disabled = kPrFalse;
	audioCodecValues.hidden = kPrFalse;
	audioCodecValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo audioCodecParam;
	audioCodecParam.structVersion = 1;
	audioCodecParam.flags = exParamFlag_none;
	audioCodecParam.paramType = exParamType_int;
	audioCodecParam.paramValues = audioCodecValues;
	::lstrcpyA(audioCodecParam.identifier, ADBEAudioCodec);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicAudioGroup, &audioCodecParam);

	// Param: Audio settings
	exParamValues audioSettingsValues;
	audioSettingsValues.structVersion = 1;
	audioSettingsValues.disabled = kPrFalse;
	audioSettingsValues.hidden = kPrFalse;
	audioSettingsValues.optionalParamEnabled = kPrFalse;
	prUTF16CharCopy(audioSettingsValues.paramString, L"");
	exNewParamInfo audioSettingsParam;
	audioSettingsParam.structVersion = 1;
	audioSettingsParam.flags = exParamFlag_independant;
	audioSettingsParam.paramType = exParamType_string;
	audioSettingsParam.paramValues = audioSettingsValues;
	::lstrcpyA(audioSettingsParam.identifier, VKDRAudioSettings);
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicAudioGroup, &audioSettingsParam);

	// Param: Audio Sample Rate
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
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicAudioGroup, &sampleRateParam);

	// Param: Audio Channels
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
	exportParamSuite->AddParam(pluginId, groupIndex, ADBEBasicAudioGroup, &channelsParam);

	// Param: Multiplexer
	exParamValues multiplexerValues;
	multiplexerValues.structVersion = 1;
	multiplexerValues.value.intValue = config->DefaultMultiplexer;
	multiplexerValues.disabled = kPrFalse;
	multiplexerValues.hidden = kPrFalse;
	multiplexerValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo multiplexerParam;
	multiplexerParam.structVersion = 1;
	multiplexerParam.flags = exParamFlag_multiLine;
	multiplexerParam.paramType = exParamType_int;
	multiplexerParam.paramValues = multiplexerValues;
	::lstrcpyA(multiplexerParam.identifier, FFMultiplexer);
	exportParamSuite->AddParam(pluginId, groupIndex, FFMultiplexerBasicGroup, &multiplexerParam);

	// Param: Pipe - Command
	exParamValues pipeCmdValues;
	pipeCmdValues.structVersion = 1;
	pipeCmdValues.disabled = kPrFalse;
	pipeCmdValues.hidden = kPrFalse;
	pipeCmdValues.optionalParamEnabled = kPrFalse;
	prUTF16CharCopy(pipeCmdValues.paramString, L"");
	exNewParamInfo pipeCmdParam;
	pipeCmdParam.structVersion = 1;
	pipeCmdParam.flags = exParamFlag_none;
	pipeCmdParam.paramType = exParamType_string;
	pipeCmdParam.paramValues = pipeCmdValues;
	::lstrcpyA(pipeCmdParam.identifier, VKDRPipeCommand);
	exportParamSuite->AddParam(pluginId, groupIndex, MultiplexerPipeGroup, &pipeCmdParam);

	// Param: Pipe - Button
	exParamValues pipeBtnValues;
	pipeBtnValues.structVersion = 1;
	pipeBtnValues.disabled = kPrFalse;
	pipeBtnValues.hidden = kPrFalse;
	pipeBtnValues.optionalParamEnabled = kPrFalse;
	exNewParamInfo pipeBtnParam;
	pipeBtnParam.structVersion = 1;
	pipeBtnParam.flags = exParamFlag_independant;
	pipeBtnParam.paramType = exParamType_button;
	pipeBtnParam.paramValues = pipeBtnValues;
	::lstrcpyA(pipeBtnParam.identifier, VKDRPipeButton);
	exportParamSuite->AddParam(pluginId, groupIndex, MultiplexerPipeGroup, &pipeBtnParam);

	// Param: Pipe - Arguments
	exParamValues pipeArgsValues;
	pipeArgsValues.structVersion = 1;
	pipeArgsValues.disabled = kPrFalse;
	pipeArgsValues.hidden = kPrFalse;
	pipeArgsValues.optionalParamEnabled = kPrFalse;
	prUTF16CharCopy(pipeArgsValues.paramString, L"");
	exNewParamInfo pipeArgsParam;
	pipeArgsParam.structVersion = 1;
	pipeArgsParam.flags = exParamFlag_none;
	pipeArgsParam.paramType = exParamType_string;
	pipeArgsParam.paramValues = pipeArgsValues;
	::lstrcpyA(pipeArgsParam.identifier, VKDRPipeArguments);
	exportParamSuite->AddParam(pluginId, groupIndex, MultiplexerPipeGroup, &pipeArgsParam);
		
	// Init encoder base dynamic params
	for (EncoderInfo encoderInfo : config->Encoders)
	{
		int selectedId = 0;
		if (encoderInfo.encoderType == EncoderType::Video)
		{
			selectedId = config->DefaultVideoEncoder;
		}
		else if (encoderInfo.encoderType == EncoderType::Audio)
		{
			selectedId = config->DefaultAudioEncoder;
		}
		createParametersFromConfig(exportParamSuite, encoderInfo, selectedId);
	}

	// Init filters
	for (FilterInfo filterInfo : config->Filters)
		createParametersFromConfig(exportParamSuite, filterInfo, -1);

	exportParamSuite->SetParamsVersion(pluginId, paramVersion);

	return malNoError;
}

void GUI::createParametersFromConfig(PrSDKExportParamSuite *exportParamSuite, IParamContainer encoderInfo, int selectedId)
{
	// Add the param groups
	for (ParamGroupInfo paramGroup : encoderInfo.groups)
	{
		exportParamSuite->AddParamGroup(
			pluginId,
			groupIndex,
			paramGroup.parent.c_str(),
			paramGroup.name.c_str(),
			wstring(paramGroup.label.begin(), paramGroup.label.end()).c_str(),
			kPrFalse,
			kPrFalse,
			kPrFalse);
	}

	// Iterate encoder params
	for (ParamInfo paramInfo : encoderInfo.params)
	{
		const exNewParamInfo newParamInfo = createParameter(
			paramInfo,
			selectedId > -1 && encoderInfo.id != selectedId);

		exportParamSuite->AddParam(
			pluginId,
			groupIndex,
			paramInfo.group.c_str(),
			&newParamInfo);

		// Add possible existing subvalues
		for (ParamValueInfo paramValueInfo : paramInfo.values)
		{
			for (ParamSubValueInfo paramSubValue : paramValueInfo.subValues)
			{
				const exNewParamInfo newParamInfo = createParameter(
					paramSubValue,
					(paramInfo.default.intValue != paramValueInfo.id) || (selectedId > -1 && encoderInfo.id != selectedId));

				exportParamSuite->AddParam(
					pluginId,
					groupIndex,
					paramInfo.group.c_str(),
					&newParamInfo);
			}
		}
	}
}

exNewParamInfo GUI::createParameter(const IParamInfo paramConfig, const csSDK_int32 hidden)
{
	// Fill param values
	exParamValues paramValues;
	paramValues.structVersion = 1;
	paramValues.disabled = kPrFalse;
	paramValues.hidden = hidden;
	paramValues.optionalParamEnabled = kPrFalse;

	// Fill paramInfo
	exNewParamInfo paramInfo;
	paramInfo.structVersion = 1;
	paramInfo.flags = exParamFlag_none;
	paramInfo.paramType = exParamType_undefined;
	::lstrcpyA(paramInfo.identifier, paramConfig.name.c_str());
	wcscpy_s(paramInfo.name, wstring(paramConfig.label.begin(), paramConfig.label.end()).c_str());

	// Data types
	if (paramConfig.type == "float")
	{
		paramInfo.paramType = exParamType_float;
		paramValues.value.floatValue = paramConfig.default.floatValue;

		if (paramConfig.minSet)
			paramValues.rangeMin.floatValue = paramConfig.min.floatValue;

		if (paramConfig.maxSet)
			paramValues.rangeMax.floatValue = paramConfig.max.floatValue;
	}
	else if (paramConfig.type == "int")
	{
		paramInfo.paramType = exParamType_int;
		paramValues.value.intValue = paramConfig.default.intValue;

		if (paramConfig.minSet)
			paramValues.rangeMin.intValue = paramConfig.min.intValue;

		if (paramConfig.maxSet)
			paramValues.rangeMax.intValue = paramConfig.max.intValue;
	}
	else if (paramConfig.type == "bool")
	{
		paramInfo.paramType = exParamType_bool;
		paramValues.value.intValue = paramConfig.default.intValue;
	}
	else if (paramConfig.type == "string")
	{
		paramInfo.paramType = exParamType_string;
		prUTF16CharCopy(paramValues.paramString, wstring(paramConfig.defaultStringValue.begin(), paramConfig.defaultStringValue.end()).c_str());
	}
	else if (paramConfig.type == "button")
	{
		paramInfo.paramType = exParamType_button;
	}

	// Flags
	for (string flag : paramConfig.flags)
	{
		if (flag == "slider")
		{
			paramInfo.flags = exParamFlag_slider;
		}
		else if (flag == "multiline")
		{
			paramInfo.flags = exParamFlag_multiLine;
		}
	}

	paramInfo.paramValues = paramValues;

	return paramInfo;
}

prMALError GUI::updateParameters(PrSDKExportParamSuite *exportParamSuite, PrSDKTimeSuite *timeSuite)
{
	// Get ticks per second
	PrTime ticksPerSecond;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);

	wchar_t text[256];
	swprintf_s(text, L"Update available! Download Voukoder %d.%d.%d",
		pluginUpdate->version.number.major,
		pluginUpdate->version.number.minor,
		pluginUpdate->version.number.patch);

	// Show update
	exParamValues updateValue;
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRUpdateButton,	&updateValue);
	updateValue.hidden = pluginUpdate->isUpdateAvailable ? kPrFalse : kPrTrue;
	exportParamSuite->ChangeParam(pluginId, groupIndex, VKDRUpdateButton, &updateValue);

	// Labels: Video
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRUpdateButton, text);
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRUpdateButton, L"Go to the download website to get the latest Voukoder version.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEBasicVideoGroup, L"Video Settings");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoCodec, L"Video Encoder");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoCodec, L"The encoder that creates the output video.");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRVideoSettings, L"Encoder params (r/o)");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRVideoSettings, L"The raw parameters supplied to the video encoder. (read only)");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRVideoFrameSize, L"Frame size");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRVideoFrameSize, L"Width and height of the video frame.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoWidth, L"Width");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoWidth, L"Width of the video frame.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoHeight, L"Height");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoHeight, L"Height of the video frame.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoFPS, L"Frame Rate");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoFPS, L"Frame Rate of the video.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoFieldType, L"Field Order");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoFieldType, L"Field order of interlaced sources.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoAspect, L"Aspect");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEVideoAspect, L"PAR describes how the width of a pixel compares to the height of that pixel.");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRTVStandard, L"TV Standard");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRTVStandard, L"The video format. USA and Japan use NTSC. Europe, Africa and Asia use PAL.");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRColorSpace, L"Color Space");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRColorSpace, L"The range of colors used in encoding the video. BT.709 is recommended for high definition videos.");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRColorRange, L"Render at Maximum Depth");
	//exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRColorRange, L"");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEVideoCodecGroup, L"Encoder Options");

	// Labels: Advanced
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRAdvVideoCodecTabGroup, L"Advanced");

	// Labels: Audio
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEBasicAudioGroup, L"Audio Settings");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEAudioCodec, L"Audio Encoder");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEAudioCodec, L"The encoder that creates the output audio.");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRAudioSettings, L"Encoder params (r/o)");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRAudioSettings, L"The raw parameters supplied to the audio encoder. (read only)");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEAudioRatePerSecond, L"Sample Rate");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEAudioRatePerSecond, L"The sample rate of the audio.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEAudioNumChannels, L"Channels");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, ADBEAudioNumChannels, L"The number of channels in the output audio.");
	exportParamSuite->SetParamName(pluginId, groupIndex, ADBEAudioCodecGroup, L"Encoder Options");

	// Labels: Multiplexer
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRMultiplexerTabGroup, L"Multiplexer");
	exportParamSuite->SetParamName(pluginId, groupIndex, FFMultiplexerBasicGroup, L"Container");
	exportParamSuite->SetParamName(pluginId, groupIndex, FFMultiplexer, L"Format");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, FFMultiplexer, L"The multiplexer/container for the output file.");

	// Piping
	exportParamSuite->SetParamName(pluginId, groupIndex, MultiplexerPipeGroup, L"Piping");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRPipeCommand, L"Program");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRPipeCommand, L"");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRPipeButton, L"Browse");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRPipeButton, L"");
	exportParamSuite->SetParamName(pluginId, groupIndex, VKDRPipeArguments, L"Arguments");
	exportParamSuite->SetParamDescription(pluginId, groupIndex, VKDRPipeArguments, L"");

	exportParamSuite->SetParamDescription(pluginId, groupIndex, FFMultiplexer, L"The multiplexer/container for the output file.");

	// Labels: Filter
	exportParamSuite->SetParamName(pluginId, groupIndex, FilterTabGroup, L"Filters");

	// Param: FrameSizes
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, VKDRVideoFrameSize);
	exOneParamValueRec frameSize;
	for (FrameSizeInfo frameSizeInfo : config->FrameSizes)
	{
		frameSize.intValue = frameSizeInfo.width * frameSizeInfo.height;
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, VKDRVideoFrameSize, &frameSize, wstring(frameSizeInfo.text.begin(), frameSizeInfo.text.end()).c_str());
	}

	// Encoders
	fillEncoderDropdown(exportParamSuite, config->Encoders);

	// Param: Pixel Aspect Ratio
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
	exportParamSuite->ClearConstrainedValues(pluginId, 0, ADBEVideoAspect);
	exOneParamValueRec tempPAR;
	for (csSDK_int32 i = 0; i < sizeof(PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		exportParamSuite->AddConstrainedValuePair(pluginId, 0, ADBEVideoAspect, &tempPAR, PARStrings[i]);
	}

	// Param: Framerate
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEVideoFPS);
	exOneParamValueRec tempFPS;
	for (FramerateInfo framerateInfo : config->Framerates)
	{
		tempFPS.timeValue = ticksPerSecond / framerateInfo.num * framerateInfo.den;
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, ADBEVideoFPS, &tempFPS, wstring(framerateInfo.text.begin(), framerateInfo.text.end()).c_str());
	}

	// Param: Field order
	const csSDK_int32 fieldOrders[] = {
		prFieldsNone,
		prFieldsUpperFirst,
		prFieldsLowerFirst
	};
	const wchar_t* const fieldOrderStrings[] = {
		L"None (Progressive)",
		L"Upper First",
		L"Lower First"
	};
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEVideoFieldType);
	exOneParamValueRec tempOrder;
	for (csSDK_int32 i = 0; i < sizeof(fieldOrders) / sizeof(fieldOrders[0]); i++)
	{
		tempOrder.intValue = fieldOrders[i];
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, ADBEVideoFieldType, &tempOrder, fieldOrderStrings[i]);
	}

	// Param: TV Standard
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, VKDRTVStandard);
	exOneParamValueRec tempTvStandard;
	tempTvStandard.intValue = TVStandard::PAL;
	exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, VKDRTVStandard, &tempTvStandard, L"PAL");
	tempTvStandard.intValue = TVStandard::NTSC;
	exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, VKDRTVStandard, &tempTvStandard, L"NTSC");

	// Param: Color Space
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, VKDRColorSpace);
	exOneParamValueRec tempcolorSpace;
	tempcolorSpace.intValue = ColorSpace::bt601;
	exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, VKDRColorSpace, &tempcolorSpace, L"BT.601 (SD)");
	tempcolorSpace.intValue = ColorSpace::bt709;
	exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, VKDRColorSpace, &tempcolorSpace, L"BT.709 (HD)");

	// Param: Audio Sample Rate
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEAudioRatePerSecond);
	exOneParamValueRec oneParamValueRec;
	for (SamplerateInfo samplerateInfo : config->Samplerates)
	{
		oneParamValueRec.floatValue = samplerateInfo.id;
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, ADBEAudioRatePerSecond, &oneParamValueRec, wstring(samplerateInfo.text.begin(), samplerateInfo.text.end()).c_str());
	}

	// Param: Audio Channels
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEAudioNumChannels);
	exOneParamValueRec oneParamValueRec2;
	for (ChannelInfo channelInfo : config->Channels)
	{
		oneParamValueRec2.intValue = channelInfo.id;
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, ADBEAudioNumChannels, &oneParamValueRec2, wstring(channelInfo.text.begin(), channelInfo.text.end()).c_str());
	}

	// Param: Multiplexer
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, FFMultiplexer);
	exOneParamValueRec oneParamValueRec3;
	for (MultiplexerInfo multiplexerInfo : config->Multiplexers)
	{
		oneParamValueRec3.intValue = multiplexerInfo.id;
		exportParamSuite->AddConstrainedValuePair(pluginId, groupIndex, FFMultiplexer, &oneParamValueRec3, wstring(multiplexerInfo.text.begin(), multiplexerInfo.text.end()).c_str());
	}

	for (EncoderInfo encoderInfo : config->Encoders)
		updateDynamicParameters(exportParamSuite, encoderInfo);

	for (FilterInfo filterInfo : config->Filters)
		updateDynamicParameters(exportParamSuite, filterInfo);

	refreshEncoderSettings(exportParamSuite);

	return malNoError;
}

void GUI::updateDynamicParameters(PrSDKExportParamSuite *exportParamSuite, IParamContainer encoderInfo)
{
	const bool isCacheValid = exportParamSuite->GetParamsVersion(pluginId) == paramVersion;

	for (ParamGroupInfo paramGroup : encoderInfo.groups)
	{
		exportParamSuite->SetParamName(
			pluginId,
			groupIndex,
			paramGroup.name.c_str(),
			wstring(paramGroup.label.begin(), paramGroup.label.end()).c_str());
	}

	// Iterate all encoder options
	for (ParamInfo paramInfo : encoderInfo.params)
	{
		updateSingleDynamicParameter(exportParamSuite, &paramInfo, isCacheValid);
	}
}

void GUI::updateSingleDynamicParameter(PrSDKExportParamSuite *exportParamSuite, IParamInfo *paramInfo, const bool isCacheValid)
{
	exportParamSuite->SetParamName(
		pluginId,
		groupIndex,
		paramInfo->name.c_str(),
		wstring(paramInfo->label.begin(), paramInfo->label.end()).c_str());

	exportParamSuite->SetParamDescription(
		pluginId,
		groupIndex,
		paramInfo->name.c_str(),
		wstring(paramInfo->description.begin(), paramInfo->description.end()).c_str());

	exParamValues paramValues;
	exportParamSuite->GetParamValue(
		pluginId,
		groupIndex,
		paramInfo->name.c_str(),
		&paramValues);

	if (paramInfo->minSet)
	{
		if (paramInfo->type == "float")
		{
			paramValues.rangeMin.floatValue = paramInfo->min.floatValue;
		}
		else if (paramInfo->type == "int")
		{
			paramValues.rangeMin.intValue = paramInfo->min.intValue;
		}
	}

	if (paramInfo->maxSet)
	{
		if (paramInfo->type == "float")
		{
			paramValues.rangeMax.floatValue = paramInfo->max.floatValue;
		}
		else if (paramInfo->type == "int")
		{
			paramValues.rangeMax.intValue = paramInfo->max.intValue;
		}
	}

	exportParamSuite->ChangeParam(pluginId, groupIndex, paramInfo->name.c_str(), &paramValues);

	if (!paramInfo->isSubValue)
	{
		ParamInfo parameterInfo = *static_cast<ParamInfo*>(paramInfo);

		// It is a simple dropdown?
		if (parameterInfo.values.size() > 0)
		{
			exportParamSuite->ClearConstrainedValues(
				pluginId,
				groupIndex,
				parameterInfo.name.c_str());

			// Populate new values
			exOneParamValueRec oneParamValueRec;
			for (ParamValueInfo paramValueInfo : parameterInfo.values)
			{
				oneParamValueRec.intValue = paramValueInfo.id;

				exportParamSuite->AddConstrainedValuePair(
					pluginId,
					groupIndex,
					parameterInfo.name.c_str(),
					&oneParamValueRec,
					wstring(paramValueInfo.name.begin(), paramValueInfo.name.end()).c_str());

				for (ParamSubValueInfo paramSubValueInfo : paramValueInfo.subValues)
				{
					updateSingleDynamicParameter(exportParamSuite, &paramSubValueInfo, isCacheValid);
				}
			}
		}
	}
}

void GUI::fillEncoderDropdown(PrSDKExportParamSuite *exportParamSuite, vector<EncoderInfo> encoderInfos)
{
	// Clear existing values
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEVideoCodec);
	exportParamSuite->ClearConstrainedValues(pluginId, groupIndex, ADBEAudioCodec);

	// Populate encoders
	for (EncoderInfo encoderInfo : encoderInfos)
	{
		exOneParamValueRec oneParamValueRec;
		oneParamValueRec.intValue = encoderInfo.id;

		if (encoderInfo.encoderType == EncoderType::Video)
		{
			exportParamSuite->AddConstrainedValuePair(
				pluginId,
				groupIndex,
				ADBEVideoCodec,
				&oneParamValueRec,
				wstring(encoderInfo.text.begin(), encoderInfo.text.end()).c_str());
		}
		else if (encoderInfo.encoderType == EncoderType::Audio)
		{
			exportParamSuite->AddConstrainedValuePair(
				pluginId,
				groupIndex,
				ADBEAudioCodec,
				&oneParamValueRec,
				wstring(encoderInfo.text.begin(), encoderInfo.text.end()).c_str());
		}
	}
}

prSuiteError GUI::setParameterVisibility(PrSDKExportParamSuite *exportParamSuite, IParamInfo *info, const bool hidden)
{
	exParamValues value;
	exportParamSuite->GetParamValue(pluginId, groupIndex, info->name.c_str(), &value);
	value.hidden = hidden ? kPrTrue : kPrFalse;

	return exportParamSuite->ChangeParam(pluginId, groupIndex, info->name.c_str(), &value);
}

prMALError GUI::onParamChange(PrSDKExportParamSuite *exportParamSuite, PrSDKWindowSuite *windowSuite, exParamChangedRec *paramRecP)
{
	exParamValues paramValue;
	exportParamSuite->GetParamValue(
		pluginId,
		groupIndex,
		paramRecP->changedParamIdentifier,
		&paramValue);

	const string paramName = paramRecP->changedParamIdentifier;
	/*
	if (paramName == VKDRVideoFrameSize)
	{
		if (paramValue.value.intValue == 0)
		{
			exParamValues tempValue;
			exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoWidth, &tempValue);
			tempValue.disabled = kPrTrue;
			exportParamSuite->ChangeParam(pluginId, groupIndex, ADBEVideoWidth, &tempValue);

			exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoHeight, &tempValue);
			tempValue.disabled = kPrTrue;
			exportParamSuite->ChangeParam(pluginId, groupIndex, ADBEVideoHeight, &tempValue);
		}
		else
		{
			for (FrameSizeInfo frameSizeInfo : config->FrameSizes)
			{
				if (frameSizeInfo.width * frameSizeInfo.height == paramValue.value.intValue)
				{
					exParamValues tempValue;
					exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoWidth, &tempValue);
					tempValue.value.intValue = frameSizeInfo.width;
					tempValue.disabled = kPrFalse;
					exportParamSuite->ChangeParam(pluginId,	groupIndex, ADBEVideoWidth,	&tempValue);

					exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoHeight, &tempValue);
					tempValue.value.intValue = frameSizeInfo.height;
					tempValue.disabled = kPrFalse;
					exportParamSuite->ChangeParam(pluginId, groupIndex, ADBEVideoHeight, &tempValue);

					break;
				}
			}
		}
	}
	else */if (paramName == ADBEVideoCodec || paramName == ADBEAudioCodec)
	{
		for (const EncoderInfo selectedEncoderInfo : config->Encoders)
		{
			if (selectedEncoderInfo.id != paramValue.value.intValue)
				continue;

			for (EncoderInfo encoderInfo : config->Encoders)
			{
				if (encoderInfo.encoderType != selectedEncoderInfo.encoderType)
					continue;

				for (ParamInfo paramInfo : encoderInfo.params)
				{
					setParameterVisibility(exportParamSuite, &paramInfo, encoderInfo.id != selectedEncoderInfo.id);

					exParamValues tempValue;
					exportParamSuite->GetParamValue(pluginId, groupIndex, paramInfo.name.c_str(), &tempValue);

					const int selectedId = tempValue.value.intValue;

					for (ParamValueInfo paramValueInfo : paramInfo.values)
					{
						for (ParamSubValueInfo paramSubValueInfo : paramValueInfo.subValues)
						{
							setParameterVisibility(exportParamSuite, &paramSubValueInfo, encoderInfo.id != selectedEncoderInfo.id || paramValueInfo.id != selectedId);
						}
					}
				}
			}
			break;
		}
	}
	else
	{
		for (EncoderInfo encoderInfo : config->Encoders)
		{
			for (ParamInfo paramInfo : encoderInfo.params)
			{
				if (paramInfo.name == paramName)
				{
					for (ParamValueInfo paramValueInfo : paramInfo.values)
					{
						for (ParamSubValueInfo paramSubValueInfo : paramValueInfo.subValues)
						{
							setParameterVisibility(exportParamSuite, &paramSubValueInfo, paramValueInfo.id != paramValue.value.intValue);
						}
					}

					goto finish;
				}
			}
		}
	}

finish:

	refreshEncoderSettings(exportParamSuite);

	return malNoError;
}

bool GUI::getCurrentFilterSettings(PrSDKExportParamSuite *exportParamSuite, ExportSettings *exportSettings)
{
	FilterSettings filterSettings;

	for (const FilterInfo filterInfo : config->Filters)
	{
		filterSettings.name = filterInfo.name;

		for (ParamInfo paramInfo : filterInfo.params)
		{
			exParamValues paramValue;
			exportParamSuite->GetParamValue(
				pluginId,
				groupIndex,
				paramInfo.name.c_str(),
				&paramValue);

			filterSettings.addParams(
				paramInfo,
				paramInfo.parameters,
				paramValue);

			for (ParamValueInfo paramValueInfo : paramInfo.values)
			{
				if (paramValueInfo.id == paramValue.value.intValue)
				{
					filterSettings.addParams(
						paramInfo,
						paramValueInfo.parameters,
						paramValue);

					for (ParamSubValueInfo paramSubValueInfo : paramValueInfo.subValues)
					{
						exParamValues paramValue;
						exportParamSuite->GetParamValue(
							pluginId,
							groupIndex,
							paramSubValueInfo.name.c_str(),
							&paramValue);

						filterSettings.addParams(
							paramInfo,
							paramSubValueInfo.parameters,
							paramValue);
					}
					break;
				}
			}
		}

		// Special filter specific handling
		if (filterSettings.name == "zscale")
		{
			if (filterSettings.params.find("f") == filterSettings.params.end())
				break;

			if (filterSettings.params.find("w") != filterSettings.params.end())
				exportSettings->resizeWidth = std::stoi(filterSettings.params["w"]);

			if (filterSettings.params.find("h") != filterSettings.params.end())
				exportSettings->resizeHeight = std::stoi(filterSettings.params["h"]);
		}

		exportSettings->videoFilters.push_back(filterSettings.toString());
	}

	return true;
}

bool GUI::getCurrentEncoderSettings(PrSDKExportParamSuite *exportParamSuite, prFieldType fieldType, EncoderType encoderType, EncoderSettings *encoderSettings)
{
	exParamValues encoderValue;
	if (encoderType == EncoderType::Video)
	{
		exportParamSuite->GetParamValue(
			pluginId,
			groupIndex,
			ADBEVideoCodec,
			&encoderValue);
	}
	else if (encoderType == EncoderType::Audio)
	{
		exportParamSuite->GetParamValue(
			pluginId,
			groupIndex,
			ADBEAudioCodec,
			&encoderValue);
	}
	else
	{
		return false;
	}
	
	for (const EncoderInfo encoderInfo : config->Encoders)
	{
		if (encoderInfo.id == encoderValue.value.intValue)
		{
			encoderSettings->passes = 1;
			encoderSettings->name = encoderInfo.name;
			encoderSettings->text = encoderInfo.text;
			encoderSettings->pixelFormat = encoderInfo.defaultPixelFormat;

			ParamInfo ilaceParamInfo;
			ilaceParamInfo.type = "int";
			ilaceParamInfo.default.intValue = 0;
			ilaceParamInfo.useDefaultValue = true;

			exParamValues boolValue;
			boolValue.value.intValue = 1;

			if (fieldType == prFieldsLowerFirst)
			{
				encoderSettings->addParams(
					ilaceParamInfo,
					encoderInfo.interlaced.bottomFrameFirst,
					boolValue,
					encoderInfo.paramGroups
				);
			}
			else if (fieldType == prFieldsUpperFirst)
			{
				encoderSettings->addParams(
					ilaceParamInfo,
					encoderInfo.interlaced.topFrameFirst,
					boolValue,
					encoderInfo.paramGroups
				);
			}
			else if (fieldType == prFieldsNone)
			{
				encoderSettings->addParams(
					ilaceParamInfo,
					encoderInfo.interlaced.progressive,
					boolValue,
					encoderInfo.paramGroups
				);
			}

			exParamValues paramValue;

			for (ParamInfo paramInfo : encoderInfo.params)
			{
				exportParamSuite->GetParamValue(
					pluginId, 
					groupIndex, 
					paramInfo.name.c_str(),
					&paramValue);

				if (encoderInfo.multipassParameter.length() > 0 &&
					paramInfo.name == encoderInfo.multipassParameter)
					encoderSettings->passes = paramValue.value.intValue;

				encoderSettings->addParams(
					paramInfo,
					paramInfo.parameters,
					paramValue,
					encoderInfo.paramGroups);

				for (ParamValueInfo paramValueInfo : paramInfo.values)
				{
					if (paramValueInfo.id == paramValue.value.intValue)
					{
						if (paramValueInfo.pixelFormat.size() > 0)
							encoderSettings->pixelFormat = paramValueInfo.pixelFormat;

						if (encoderInfo.multipassParameter.length() > 0 &&
							paramValueInfo.name == encoderInfo.multipassParameter)
								encoderSettings->passes = paramValue.value.intValue;

						encoderSettings->addParams(
							paramInfo,
							paramValueInfo.parameters,
							paramValue,
							encoderInfo.paramGroups);

						for (ParamSubValueInfo paramSubValueInfo : paramValueInfo.subValues)
						{
							exportParamSuite->GetParamValue(
								pluginId, 
								groupIndex, 
								paramSubValueInfo.name.c_str(), 
								&paramValue);

							if (paramSubValueInfo.pixelFormat.size() > 0)
								encoderSettings->pixelFormat = paramSubValueInfo.pixelFormat;

							if (encoderInfo.multipassParameter.length() > 0 && 
								paramSubValueInfo.name == encoderInfo.multipassParameter)
									encoderSettings->passes = paramValue.value.intValue;

							encoderSettings->addParams(
								paramSubValueInfo,
								paramSubValueInfo.parameters,
								paramValue,
								encoderInfo.paramGroups);
						}
						break;
					}
				}
			}
		}
	}

	return true;
}

void GUI::getExportSettings(PrSDKExportParamSuite *exportParamSuite, ExportSettings *exportSettings)
{
	exParamValues videoWidth, videoHeight, pixelAspect, audioChannelType, audioSampleRate, ticksPerFrame, fieldType, multiplexer, tvStandard, colorSpace, colorRange;
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoWidth, &videoWidth);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoHeight, &videoHeight);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEAudioNumChannels, &audioChannelType);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEAudioRatePerSecond, &audioSampleRate);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoFPS, &ticksPerFrame);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoFieldType, &fieldType);
	exportParamSuite->GetParamValue(pluginId, groupIndex, ADBEVideoAspect, &pixelAspect);
	exportParamSuite->GetParamValue(pluginId, groupIndex, FFMultiplexer, &multiplexer);
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRTVStandard, &tvStandard);
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRColorSpace, &colorSpace);
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRColorRange, &colorRange);

	// Find FPS rate
	PrTime c = gcd(254016000000, ticksPerFrame.value.timeValue);
	int den = (int)(254016000000 / c);
	int num = (int)(ticksPerFrame.value.timeValue / c);

	EncoderSettings videoEncoderSettings;
	getCurrentEncoderSettings(exportParamSuite, (prFieldType)fieldType.value.intValue, EncoderType::Video, &videoEncoderSettings);

	EncoderSettings audioEncoderSettings;
	getCurrentEncoderSettings(exportParamSuite, prFieldsInvalid, EncoderType::Audio, &audioEncoderSettings);

	getCurrentFilterSettings(exportParamSuite, exportSettings);

	string multiplexerName;
	for (MultiplexerInfo multiplexerInfo : config->Multiplexers)
	{
		if (multiplexerInfo.id == multiplexer.value.intValue)
		{
			multiplexerName = multiplexerInfo.name;
			break;
		}
	}

	exportSettings->width = (int)videoWidth.value.intValue;
	exportSettings->height = (int)videoHeight.value.intValue;
	exportSettings->pixelFormat = videoEncoderSettings.pixelFormat;
	exportSettings->muxerName = multiplexerName;
	exportSettings->passes = videoEncoderSettings.passes;
	exportSettings->videoCodecName = videoEncoderSettings.name;
	exportSettings->videoTimebase.num = num;
	exportSettings->videoTimebase.den = den;
	exportSettings->videoSar.num = pixelAspect.value.ratioValue.numerator;
	exportSettings->videoSar.den = pixelAspect.value.ratioValue.denominator;
	exportSettings->videoOptions = videoEncoderSettings.toString();
	exportSettings->audioCodecName = audioEncoderSettings.name;
	exportSettings->audioTimebase.num = 1;
	exportSettings->audioTimebase.den = (int)audioSampleRate.value.floatValue;
	exportSettings->audioOptions = audioEncoderSettings.toString();

	switch (audioChannelType.value.intValue)
	{
	case kPrAudioChannelType_Mono:
		exportSettings->audioChannelLayout = AV_CH_LAYOUT_MONO;
		break;

	case kPrAudioChannelType_51:
		exportSettings->audioChannelLayout = AV_CH_LAYOUT_5POINT1_BACK;
		break;

	default:
		exportSettings->audioChannelLayout = AV_CH_LAYOUT_STEREO;
		break;
	}

	switch (fieldType.value.intValue)
	{
	case prFieldsLowerFirst:
		exportSettings->fieldOrder = AVFieldOrder::AV_FIELD_BB;
		break;

	case prFieldsUpperFirst:
		exportSettings->fieldOrder = AVFieldOrder::AV_FIELD_TT;
		break;

	default:
		exportSettings->fieldOrder = AVFieldOrder::AV_FIELD_PROGRESSIVE;
	}

	switch (tvStandard.value.intValue)
	{
	case TVStandard::PAL:
		exportSettings->tvStandard = "pal";
		break;

	case TVStandard::NTSC:
		exportSettings->tvStandard = "ntsc";
		break;
	}

	exportSettings->colorRange = AVColorRange::AVCOL_RANGE_MPEG;

	if (colorSpace.value.intValue == ColorSpace::bt601)
	{
		if (tvStandard.value.intValue == TVStandard::PAL)
		{
			exportSettings->colorSpace = AVColorSpace::AVCOL_SPC_BT470BG;
			exportSettings->colorPrimaries = AVColorPrimaries::AVCOL_PRI_BT470BG;
			exportSettings->colorTRC = AVColorTransferCharacteristic::AVCOL_TRC_GAMMA28;
		}
		else if (tvStandard.value.intValue == TVStandard::NTSC)
		{
			exportSettings->colorSpace = AVColorSpace::AVCOL_SPC_SMPTE170M;
			exportSettings->colorPrimaries = AVColorPrimaries::AVCOL_PRI_SMPTE170M;
			exportSettings->colorTRC = AVColorTransferCharacteristic::AVCOL_TRC_SMPTE170M;
		}
	}
	else if (colorSpace.value.intValue == ColorSpace::bt709)
	{
		exportSettings->colorSpace = AVColorSpace::AVCOL_SPC_BT709;
		exportSettings->colorPrimaries = AVColorPrimaries::AVCOL_PRI_BT709;
		exportSettings->colorTRC = AVColorTransferCharacteristic::AVCOL_TRC_BT709;
	}

	// Get pipe values
	exParamValues pipeProgram, pipeArguments;
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRPipeCommand, &pipeProgram);
	exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRPipeArguments, &pipeArguments);

	// Build pipe command line
	exportSettings->pipe = wcslen(pipeProgram.paramString) > 0;
	if (exportSettings->pipe)
	{
		exportSettings->pipeCommand = pipeProgram.paramString;
		if (wcslen(pipeArguments.paramString) > 0)
		{
			exportSettings->pipeCommand.append(L" ");
			exportSettings->pipeCommand.append(pipeArguments.paramString);
		}
	}
}

void GUI::refreshEncoderSettings(PrSDKExportParamSuite *exportParamSuite)
{
	EncoderSettings videoEncoderSettings;
	getCurrentEncoderSettings(exportParamSuite, prFieldsInvalid, EncoderType::Video, &videoEncoderSettings);
	string config = videoEncoderSettings.toString();

	exParamValues videoValues;
	exportParamSuite->GetParamValue(
		pluginId,
		groupIndex,
		VKDRVideoSettings,
		&videoValues);

	prUTF16CharCopy(videoValues.paramString, wstring(config.begin(), config.end()).c_str());

	exportParamSuite->ChangeParam(
		pluginId,
		groupIndex,
		VKDRVideoSettings,
		&videoValues);

	EncoderSettings audioEncoderSettings;
	getCurrentEncoderSettings(exportParamSuite, prFieldsInvalid, EncoderType::Audio, &audioEncoderSettings);
	config = audioEncoderSettings.toString();

	exParamValues audioValues;
	exportParamSuite->GetParamValue(
		pluginId,
		groupIndex,
		VKDRAudioSettings,
		&audioValues);

	prUTF16CharCopy(audioValues.paramString, wstring(config.begin(), config.end()).c_str());

	exportParamSuite->ChangeParam(
		pluginId,
		groupIndex,
		VKDRAudioSettings,
		&audioValues);
}

prMALError GUI::onButtonPress(exParamButtonRec *paramButtonRecP, PrSDKExportParamSuite *exportParamSuite, PrSDKWindowSuite *windowSuite)
{
	if (strcmp(paramButtonRecP->buttonParamIdentifier, VKDRUpdateButton) == 0)
	{
		ShellExecuteA(0, 0, pluginUpdate->url.c_str(), 0, 0, SW_SHOW);
	}
	else if (strcmp(paramButtonRecP->buttonParamIdentifier, VKDRPipeButton) == 0)
	{
		exParamValues pathValue;
		wchar_t filepath_string[sizeof(pathValue.paramString) / sizeof(pathValue.paramString[0])];
		exportParamSuite->GetParamValue(pluginId, groupIndex, VKDRPipeCommand, &pathValue);
		prUTF16CharCopy(filepath_string, pathValue.paramString);

		OPENFILENAMEW ofn = { 0 };
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = windowSuite->GetMainWindow();
		ofn.hInstance = NULL;
		ofn.lpstrFilter = { L"Executables (*.exe)\0*.exe\0\0" };
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = filepath_string;
		ofn.nMaxFile = sizeof(filepath_string) / sizeof(filepath_string[0]);
		ofn.lpstrFileTitle = L"lpstrFileTitle";
		ofn.lpstrInitialDir = NULL;
		ofn.lpstrTitle = L"Browse for executable";
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.nFileOffset = 0;
		ofn.nFileExtension = 0;
		ofn.lpstrDefExt = NULL;

		if (GetOpenFileNameW(&ofn)) 
		{
			prUTF16CharCopy(pathValue.paramString, filepath_string);
			exportParamSuite->ChangeParam(pluginId, groupIndex, VKDRPipeCommand, &pathValue);
		}
	}

	return malNoError;
}

int GUI::showDialog(PrSDKWindowSuite *windowSuite, string text, string caption, uint32_t type)
{
	HWND mainWnd = windowSuite->GetMainWindow();

	return MessageBoxA(GetLastActivePopup(mainWnd), text.c_str(), caption.c_str(), type);
}
