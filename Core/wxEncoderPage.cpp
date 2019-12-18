#include "wxEncoderPage.h"
#include "LanguageUtils.h"

wxDEFINE_EVENT(wxEVT_CHECKBOX_CHANGE, wxPropertyGridEvent);
wxDEFINE_EVENT(wxEVT_ENCODER_CHANGED, wxEncoderChangedEvent);
wxDEFINE_EVENT(wxEVT_APPLY_PRESET, wxApplyPresetEvent);

wxEncoderPage::wxEncoderPage(wxWindow* parent, std::vector<EncoderInfo> encoders, EncoderInfo sideData, std::vector<EncoderInfo> filterInfos, TrackSettings& settings) :
	wxPanel(parent), settings(settings)
{
	wxBoxSizer* bLayout = new wxBoxSizer(wxVERTICAL);

	wxNotebook* m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
	m_encoderPanel = new wxEncoderPanel(m_notebook, encoders);
	m_encoderPanel->Bind(wxEVT_ENCODER_CHANGED, &wxEncoderPage::OnEncoderChanged, this);
	m_encoderPanel->Bind(wxEVT_APPLY_PRESET, &wxEncoderPage::OnApplyPreset, this);
	m_notebook->AddPage(m_encoderPanel, Trans("ui.voukoder.configuration.encoder"), true);

	// Encoder config
	m_encoderOptions = new wxOptionEditor(m_notebook);
	m_notebook->AddPage(m_encoderOptions, Trans("ui.voukoder.configuration.options"), false);

	// Side data
	if (sideData.groups.size() > 0)
	{
		m_sideDataOptions = new wxOptionEditor(m_notebook, false, false);
		m_notebook->AddPage(m_sideDataOptions, Trans("ui.voukoder.configuration.sidedata"), false);
		m_sideDataOptions->Configure(sideData, settings.sideData);
	}

	// Filters
	if (filterInfos.size() > 0)
	{
		m_filterPanel = new wxFilterPanel(m_notebook, filterInfos);
		m_notebook->AddPage(m_filterPanel, Trans("ui.voukoder.configuration.filters"), false);
		m_filterPanel->Configure(settings.filters);
	}

	bLayout->Add(m_notebook, 1, wxEXPAND | wxALL, 0);

	this->SetSizer(bLayout);
	this->Layout();
	this->Centre(wxBOTH);
}

bool wxEncoderPage::SetEncoder(wxString id)
{
	bool res = m_encoderPanel->SelectEncoder(id);
	if (res)
	{
		EncoderInfo* encoderInfo = GetSelectedEncoder();
		m_encoderOptions->Configure(*encoderInfo, settings.options);
	}

	return res;
}

EncoderInfo* wxEncoderPage::GetSelectedEncoder()
{
	return m_encoderPanel->GetSelectedEncoder();
}

void wxEncoderPage::OnEncoderChanged(wxEncoderChangedEvent& event)
{
	// Get selected encoder info
	auto encoderInfo = event.GetEncoderInfo();
	if (encoderInfo == NULL)
		return;

	// Unfold param groups
	for (auto& paramGroup : encoderInfo->paramGroups)
	{
		if (settings.options.find(paramGroup.first) != settings.options.end())
		{
			wxStringTokenizer tokens(settings.options[paramGroup.first], ":");
			while (tokens.HasMoreTokens())
			{
				wxString token = tokens.GetNextToken();
				settings.options.insert(
					std::make_pair(token.BeforeFirst('='), token.AfterFirst('=')));
			}

			settings.options.erase(paramGroup.first);
		}
	}

	m_encoderOptions->Configure(*encoderInfo, settings.options);

	wxPostEvent(this, event);
}

void wxEncoderPage::OnApplyPreset(wxApplyPresetEvent& event)
{
	// Get selected preset info
	auto presetInfo = event.GetPresetInfo();
	if (presetInfo == NULL)
		return;

	// Load preset options
	settings.options.clear();
	settings.options.insert(presetInfo->options.begin(), presetInfo->options.end());

	// Apply new options
	auto encoderInfo = m_encoderPanel->GetSelectedEncoder();
	m_encoderOptions->Configure(*encoderInfo, settings.options);

	wxMessageBox(Trans("ui.voukoder.configuration.encoder.confirmation"), VKDR_APPNAME, wxICON_INFORMATION);
}

void wxEncoderPage::Change(EncoderInfo info)
{
	m_encoderOptions->Configure(info, settings.options);
}

void wxEncoderPage::ApplyChanges()
{
	// Clear all current settings
	settings.options.clear();
	settings.sideData.clear();
	settings.filters.clear();

	// Set encoder config
	if (m_encoderOptions)
		settings.options.insert(m_encoderOptions->results.begin(), m_encoderOptions->results.end());
	else
		settings.options.insert(info.defaults.begin(), info.defaults.end());

	// Set side data config
	if (m_sideDataOptions)
		settings.sideData.insert(m_sideDataOptions->results.begin(), m_sideDataOptions->results.end());

	// Filters
	if (m_filterPanel)
		m_filterPanel->GetFilterConfig(settings.filters);
}
