#include "Voukoder.h"
#include "Callback.h"
#include "EncoderUtils.h"
#include "MuxerUtils.h"

void Voukoder::Init()
{
	LoadResources(GetCurrentModule(), MAKEINTRESOURCE(ID_ENCODER));
	LoadResources(GetCurrentModule(), MAKEINTRESOURCE(ID_MUXER));
}

const Configuration* Voukoder::GetConfiguration()
{
	return &configuration;
}

BOOL Voukoder::EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)
{
	const HRSRC hRes = FindResourceEx(hModule, lpType, lpName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
	if (NULL != hRes)
	{
		HGLOBAL hData = LoadResource(hModule, hRes);
		if (NULL != hData)
		{
			const DWORD dataSize = SizeofResource(hModule, hRes);
			char *resource = new char[dataSize + 1];
			memcpy(resource, LockResource(hData), dataSize);
			resource[dataSize] = 0;
			FreeResource(hData);

			try
			{
				json jsonResource = json::parse(resource);
				string id = jsonResource["id"].get<string>();

				// Create configs and store them
				if (lpType == MAKEINTRESOURCE(ID_ENCODER))
				{
					EncoderInfo encoderInfo;
					if (EncoderUtils::Create(encoderInfo, jsonResource))
					{
						configuration.encoderInfos.push_back(encoderInfo);
					}
				}
				else if (lpType == MAKEINTRESOURCE(ID_MUXER))
				{
					MuxerInfo muxerInfo;
					if (MuxerUtils::Create(muxerInfo, jsonResource))
					{
						configuration.muxerInfos.push_back(muxerInfo);
					}
				}
			}
			catch (json::parse_error p)
			{
				OutputDebugStringA(p.what());
				return TRUE;
			}
		}
	}

	return TRUE;
}

bool Voukoder::LoadResources(HMODULE hModule, LPTSTR lpType)
{
	// Create C conform callback
	Callback<BOOL(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)>::func = bind(&Voukoder::EnumNamesFunc, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);

	return EnumResourceNames(hModule, lpType, static_cast<ENUMRESNAMEPROC>(Callback<BOOL(HMODULE hModule, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam)>::callback), NULL);
}