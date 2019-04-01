#pragma once

#include <wx\wx.h>

#define LOG_BUFFER_SIZE 65535

class Log
{
private:
	wxString _log;
	Log();

public:
	static Log* instance();
	void AddLine(wxString line);
	void Clear();
	wxString GetAsString();
};