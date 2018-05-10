#pragma once

#include <string>
#include <regex>
#include <map>
#include "..\LibVKDR\LibVKDR.h"

using namespace std;
using namespace LibVKDR;

struct FilterSettings
{
	string name;
	map<string, string> params;

	void addParams(ParamInfo paramInfo, map<string, string> parameters, exParamValues paramValue)
	{
		char buffer[256];

		for (pair<string, string> parameter : parameters)
		{
			size_t l = 0;

			if (paramInfo.type == "float")
			{
				if ((fabs(paramInfo.default.floatValue - paramValue.value.floatValue) < 1e-2) && !paramInfo.useDefaultValue)
				{
					return;
				}

				l = sprintf_s(buffer, parameter.second.c_str(), paramValue.value.floatValue);
			}
			else if (paramInfo.type == "int")
			{
				if (paramInfo.default.intValue == paramValue.value.intValue && !paramInfo.useDefaultValue)
				{
					return;
				}

				l = sprintf_s(buffer, parameter.second.c_str(), paramValue.value.intValue);
			}
			else if (paramInfo.type == "bool")
			{
				if ((paramInfo.default.intValue == paramValue.value.intValue && !paramInfo.useDefaultValue) || paramValue.value.intValue == 0)
				{
					return;
				}

				l = sprintf_s(buffer, "%d", paramValue.value.intValue);
			}
			else if (paramInfo.type == "string")
			{
				const wstring defaultValue(paramInfo.defaultStringValue.begin(), paramInfo.defaultStringValue.end());
				wstring currentValue(paramValue.paramString);

				if (defaultValue == currentValue && !paramInfo.useDefaultValue)
				{
					return;
				}

				// Make this more nice
				currentValue = std::regex_replace(currentValue, std::wregex(L":"), L"\\\\:");
				currentValue = std::regex_replace(currentValue, std::wregex(L"="), L"\\\\=");

				wcstombs_s(&l, buffer, sizeof(buffer), currentValue.c_str(), currentValue.length());
				l -= 1;
			}

			if (l > 0)
			{
				params.insert(make_pair(parameter.first, string(buffer, l + 1)));
			}
		}
	};

	string toString()
	{
		stringstream stream;
		for (pair<string, string> kv : params)
		{
			string key = kv.first;
			key.erase(remove(key.begin(), key.end(), '\0'), key.end());

			string value = kv.second;
			value.erase(remove(value.begin(), value.end(), '\0'), value.end());

			if (stream.tellp() > 0)
			{
				stream << ":";
			}

			stream << key;
			stream << "=";
			stream << value;
		}

		return name + "=" + stream.str();
	};
};