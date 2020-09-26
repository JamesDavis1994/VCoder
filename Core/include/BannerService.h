/**
 * Voukoder
 * Copyright (C) 2017-2020 Daniel Stankewitz, All Rights Reserved
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
#pragma once

#include <wx/wx.h>
#include <map>
#include <string>

#include "json.hpp"
#include "wxBanner.h"

#define VKDR_BANNER_URL "http://version.voukoder.org/banners.php"

class BannerService
{
public:
	bool LoadConfig();
	wxBanner* CreateBanner(const wxString id, wxWindow* parent);

private:
	std::map<wxString, BannerInfo> bannerInfos;
	bool GetInfo(const wxString id, BannerInfo& bannerInfo);
	wxImage* LoadImage(const wxString url);
};