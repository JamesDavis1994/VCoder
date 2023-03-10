/**
 * Voukoder
 * Copyright (C) 2017-2022 Daniel Stankewitz, All Rights Reserved
 * https://www.voukoder.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <map>
#include <sstream>
#include "OptionResourceUtils.h"
#include "LanguageUtils.h"

bool OptionResourceUtils::CreateOptionInfo(EncoderOptionInfo &optionInfo, const json resource)
{
	optionInfo.id = resource["id"].get<std::string>();
	optionInfo.name = Trans(optionInfo.id, "label");
	optionInfo.description = Trans(optionInfo.id, "description");

	// Optional: Is a multiplication factor set?
	if (resource.find("multiplicationFactor") != resource.end())
		optionInfo.multiplicationFactor = resource["multiplicationFactor"].get<int>();

	// Optional: Is it a forced parameter?
	if (resource.find("forced") != resource.end())
		optionInfo.isForced = resource["forced"].get<bool>();

	// Optional: Is this property assigned to a parameter name?
	if (resource.find("parameter") != resource.end())
		optionInfo.parameter = resource["parameter"].get<std::string>();

	// Optional: 
	if (resource.find("extraOptions") != resource.end())
	{
		for (auto& el : resource["extraOptions"].items())
			optionInfo.extraOptions.insert_or_assign(el.key(), el.value().get<std::string>());
	}

	// Optional: 
	if (resource.find("prependNoIfFalse") != resource.end())
		optionInfo.prependNoIfFalse = resource["prependNoIfFalse"].get<bool>();

	// Get the control type
	std::string type = resource["control"]["type"].get<std::string>();

	// Parse data types
	if (type == "integer")
	{
		optionInfo.control.type = EncoderOptionType::Integer;

		if (resource["control"].find("minimum") != resource["control"].end())
			optionInfo.control.minimum.intValue = resource["control"]["minimum"].get<int>();
		else
			optionInfo.control.minimum.intValue = INT_MIN;

		if (resource["control"].find("maximum") != resource["control"].end())
			optionInfo.control.maximum.intValue = resource["control"]["maximum"].get<int>();
		else
			optionInfo.control.maximum.intValue = INT_MAX;

		optionInfo.control.singleStep.intValue = resource["control"]["singleStep"].get<int>();
		optionInfo.control.value.intValue = resource["control"]["value"].get<int>();

		std::wstringstream valid;
		valid << "\n\n" << Trans("ui.generic.minimum") << ": ";
		if (optionInfo.control.minimum.intValue - 1 <= INT_MIN)
			valid << "-";
		else
			valid << optionInfo.control.minimum.intValue;
		valid << "\n" << Trans("ui.generic.maximum") << ": ";
		if (optionInfo.control.maximum.intValue >= INT_MAX)
			valid << "-";
		else
			valid << optionInfo.control.maximum.intValue;
		valid << "\n" << Trans("ui.generic.default") << ": " << optionInfo.control.value.intValue;
		optionInfo.description += valid.str();
	}
	else if (type == "float")
	{
		optionInfo.control.type = EncoderOptionType::Float;

		if (resource["control"].find("minimum") != resource["control"].end())
			optionInfo.control.minimum.floatValue = resource["control"]["minimum"].get<float>();
		else
			optionInfo.control.minimum.floatValue = float(INT_MIN);

		if (resource["control"].find("maximum") != resource["control"].end())
			optionInfo.control.maximum.floatValue = resource["control"]["maximum"].get<float>();
		else
			optionInfo.control.maximum.floatValue = float(INT_MAX);

		optionInfo.control.singleStep.floatValue = resource["control"]["singleStep"].get<float>();
		optionInfo.control.value.floatValue = resource["control"]["value"].get<float>();

		std::wstringstream valid;
		valid << std::fixed;
		valid.precision(3);
		valid << "\n\n" << Trans("ui.generic.minimum") << ": ";
		if (optionInfo.control.minimum.floatValue - 1 <= float(INT_MIN))
			valid << "-";
		else
			valid << optionInfo.control.minimum.floatValue;
		valid << "\n" << Trans("ui.generic.maximum") << ": ";
		if (optionInfo.control.maximum.floatValue >= float(INT_MAX))
			valid << "-";
		else
			valid << optionInfo.control.maximum.floatValue;
		valid << "\n" << Trans("ui.generic.default") << ": " << optionInfo.control.value.floatValue;
		optionInfo.description += valid.str();
	}
	else if (type == "boolean")
	{
		optionInfo.control.type = EncoderOptionType::Boolean;
		optionInfo.control.value.boolValue = resource["control"]["value"].get<bool>();

		std::wstringstream valid;
		valid << "\n\n" << Trans("ui.generic.default") << ": ";
		if (optionInfo.control.value.boolValue)
			valid << Trans("ui.encoderconfig.true");
		else
			valid << Trans("ui.encoderconfig.false");
		optionInfo.description += valid.str();
	}
	else if (type == "string")
	{
		optionInfo.control.type = EncoderOptionType::String;
		optionInfo.control.value.stringValue = resource["control"]["value"].get<std::string>();

		if (resource["control"].find("regex") != resource["control"].end())
			optionInfo.control.regex = resource["control"]["regex"].get<std::string>();

		std::wstringstream valid;
		valid << "\n\n" << Trans("ui.generic.default") << ": " << optionInfo.control.value.stringValue;
		optionInfo.description += valid.str();
	}
	else if (type == "combobox")
	{
		optionInfo.control.type = EncoderOptionType::ComboBox;

		// Default selected item
		optionInfo.control.selectedIndex = resource["control"]["selectedIndex"].get<int>();

		std::wstringstream valid;
		valid << "\n\n" << Trans("ui.generic.default") << ": " << optionInfo.control.value.stringValue;

		// All items
		int idx = 0;
		for (json item : resource["control"]["items"])
		{
			// Create combo item
			EncoderOptionInfo::ComboItem comboItem;
			comboItem.id = optionInfo.id + "._item_" + std::to_string(idx);
			comboItem.name = Trans(comboItem.id);

			if (item.find("value") != item.end())
			{
				comboItem.value = item["value"].get<std::string>();
			}
			
			if (item.find("extraOptions") != item.end())
			{
				for (auto& el : item["extraOptions"].items())
					comboItem.extraOptions.insert_or_assign(el.key(), el.value().get<std::string>());
			}

			// Parse filters
			CreateOptionFilterInfos(comboItem.filters, item, optionInfo.id);

			optionInfo.control.items.push_back(comboItem);

			if (optionInfo.control.selectedIndex == idx)
				valid << comboItem.name;

			idx++;
		}

		optionInfo.description += valid.str();
	}

	// Parse filters
	CreateOptionFilterInfos(optionInfo.filters, resource, optionInfo.id);

	return true;
}

bool OptionResourceUtils::CreateOptionFilterInfos(std::vector<OptionFilterInfo> &filters, json resource, std::string id)
{
	if (resource.find("filters") != resource.end())
	{
		for (json filterDefinition : resource["filters"])
		{
			OptionFilterInfo optionFilterInfo;
			if (CreateOptionFilterInfo(optionFilterInfo, filterDefinition, id))
			{
				filters.push_back(optionFilterInfo);
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

bool OptionResourceUtils::CreateOptionFilterInfo(OptionFilterInfo &optionFilterInfo, const json resource, const std::string ownerId)
{
	optionFilterInfo.name = resource["filter"].get<std::string>();
	optionFilterInfo.ownerId = ownerId;

	OptionFilterInfo::Params params;

	for (auto param : resource["params"].items())
	{
		std::vector<OptionFilterInfo::Arguments> options;

		for (auto& item : param.value())
		{
			OptionFilterInfo::Arguments option;

			for (auto b : item.items())
			{
				json data = b.value();

				OptionValue value;
				if (data.is_string())
				{
					value.stringValue = data.get<std::string>();
				}
				else if (data.is_number_integer())
				{
					value.intValue = data.get<int>();
				}
				else if (data.is_number_float())
				{
					value.floatValue = data.get<float>();
				}
				else if (data.is_boolean())
				{
					value.boolValue = data.get<bool>();
				}

				option.insert(make_pair(b.key(), value));
			}

			options.push_back(option);
		}

		optionFilterInfo.params.insert(make_pair(param.key(), options));
	}

	return true;
}
