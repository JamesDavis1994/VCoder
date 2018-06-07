#pragma once

#include <windows.h>
#include "json.hpp"
#include "resource.h"
#include "EncoderInfo.h"
#include "FramerateInfo.h"
#include "SamplerateInfo.h"
#include "ChannelInfo.h"
#include "MultiplexerInfo.h"
#include "FilterInfo.h"
#include "FrameSizeInfo.h"

#include <curl/curl.h>
#pragma comment(lib, "libcurld.lib")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wldap32.lib")

#define VKDR_UPDATE_CHECK_URL "https://www.voukoder.org/version.php?ver=%d.%d.%d"

using json = nlohmann::json;

using namespace std;

namespace LibVKDR
{
	union Version
	{
		struct number
		{
			uint8_t build;
			uint8_t patch;
			uint8_t minor;
			uint8_t major;
		} number;
		uint32_t code = 0;
	};

	struct PluginUpdate
	{
		Version version;
		string url;
		bool isUpdateAvailable;
	};

	class Config
	{
	public:
		Config();
		int DefaultVideoEncoder = -1;
		int DefaultAudioEncoder = -1;
		int DefaultMultiplexer = -1;
		vector<EncoderInfo> Encoders;
		vector<FramerateInfo> Framerates;
		vector<SamplerateInfo> Samplerates;
		vector<ChannelInfo> Channels;
		vector<MultiplexerInfo> Multiplexers;
		vector<FilterInfo> Filters;
		vector<FrameSizeInfo> FrameSizes;
		static int CheckForUpdate(Version version, PluginUpdate *pluginUpdate);

	private:
		ParamInfo createParamInfo(json json);
		ParamValueInfo createValueInfo(json json);
		ParamSubValueInfo createSubValue(json json);
		bool checkEncoderAvailability(const string encoderName);
		bool initEncoders(const json resources);
		bool initFixedParams(const json resources);
		bool initMultiplexers(const json resources);
		bool initFilters(const json resources);
		bool loadResources(HMODULE hModule, LPTSTR lpType, map<string, json> *resources);
	};
}