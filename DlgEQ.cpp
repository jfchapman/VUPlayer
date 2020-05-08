#include "DlgEQ.h"

#include "resource.h"
#include "Utility.h"

#include <iomanip>
#include <sstream>

// Slider control position scale.
static const float s_SliderScale = 60.0f / ( Settings::EQ::MaxGain - Settings::EQ::MinGain );

// Slider control tic frequency.
static const int s_SliderTicFreq = static_cast<int>( 3 * s_SliderScale );

INT_PTR CALLBACK DlgEQ::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			DlgEQ* dialog = reinterpret_cast<DlgEQ*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
			}
			break;
		}
		case WM_COMMAND : {
			DlgEQ* dialog = reinterpret_cast<DlgEQ*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				const WORD notificationCode = HIWORD( wParam );
				if ( BN_CLICKED == notificationCode ) {
					const WORD controlID = LOWORD( wParam );
					switch ( controlID ) {
						case IDC_EQ_ENABLE : {
							dialog->OnEnable();
							break;
						}
						case IDC_EQ_RESET : {
							dialog->OnReset();
							break;
						}
						default : {
							break;
						}
					}
				}
			}
			break;
		}
		case WM_VSCROLL : {
			DlgEQ* dialog = reinterpret_cast<DlgEQ*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				dialog->OnSliderChange( reinterpret_cast<HWND>( lParam ) );
			}
			break;
		}
		case WM_NOTIFY : {
			LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
			if ( ( nullptr != nmhdr ) && ( nmhdr->code == TTN_GETDISPINFO ) ) {
				DlgEQ* dialog = reinterpret_cast<DlgEQ*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
				if ( nullptr != dialog ) {
					const HWND sliderWnd = reinterpret_cast<HWND>( nmhdr->idFrom );
					const std::wstring tooltip = dialog->GetTooltip( sliderWnd );
					LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>( lParam );
					wcscpy_s( info->szText, tooltip.c_str() );			
				}
			}
			break;
		}
		case WM_CLOSE : {
			DlgEQ* dialog = reinterpret_cast<DlgEQ*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				dialog->ToggleVisibility();
			}		
			break;
		}
		case WM_DESTROY : {
			DlgEQ* dialog = reinterpret_cast<DlgEQ*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				dialog->SaveSettings();
			}
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

DlgEQ::DlgEQ( const HINSTANCE instance, Settings& settings, Output& output ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Settings( settings ),
	m_Output( output ),
	m_SliderPositions(),
	m_SliderFrequencies(),
	m_CurrentEQ( m_Settings.GetEQSettings() )
{
}

DlgEQ::~DlgEQ()
{
}

void DlgEQ::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;

	// Set enabled state.
	CheckDlgButton( m_hWnd, IDC_EQ_ENABLE, m_CurrentEQ.Enabled ? BST_CHECKED : BST_UNCHECKED );

	// Set the preamp range & position.
	const HWND preampWnd = GetDlgItem( m_hWnd, IDC_EQ_PREAMP );
	if ( nullptr != preampWnd ) {
		SendMessage( preampWnd, TBM_SETRANGEMIN, TRUE /*redraw*/, static_cast<int>( Settings::EQ::MinGain * s_SliderScale ) );
		SendMessage( preampWnd, TBM_SETRANGEMAX, TRUE /*redraw*/, static_cast<int>( Settings::EQ::MaxGain * s_SliderScale ) );
		SendMessage( preampWnd, TBM_SETTICFREQ, s_SliderTicFreq, 0 );

		const int position = GetPositionFromGain( m_CurrentEQ.Preamp );
		SendMessage( preampWnd, TBM_SETPOS, TRUE /*redraw*/, position );
	}

	// Set the freq labels, ranges & positions.
	const int bufSize = 8;
	WCHAR hz[ bufSize ] = {};
	LoadString( m_hInst, IDS_UNITS_HZ, hz, bufSize );
	const std::list<WORD> labels = { IDC_EQ_LABEL1, IDC_EQ_LABEL2, IDC_EQ_LABEL3, IDC_EQ_LABEL4, IDC_EQ_LABEL5, IDC_EQ_LABEL6, IDC_EQ_LABEL7, IDC_EQ_LABEL8, IDC_EQ_LABEL9 };
	const std::list<WORD> sliders = { IDC_EQ_FREQ1, IDC_EQ_FREQ2, IDC_EQ_FREQ3, IDC_EQ_FREQ4, IDC_EQ_FREQ5, IDC_EQ_FREQ6, IDC_EQ_FREQ7, IDC_EQ_FREQ8, IDC_EQ_FREQ9 };
	auto labelIter = labels.begin();
	auto sliderIter = sliders.begin();
	auto gainIter = m_CurrentEQ.Gains.begin();
	while ( ( labels.end() != labelIter ) && ( sliders.end() != sliderIter ) && ( m_CurrentEQ.Gains.end() != gainIter ) ) {
		const int freq = gainIter->first;
		const float gain = gainIter->second;

		const std::wstring label = ( freq < 1000 ) ? ( std::to_wstring( freq ) + hz ) : ( std::to_wstring( freq / 1000 ) + L"k" + hz );
		SetDlgItemText( m_hWnd, *labelIter, label.c_str() );

		const HWND sliderWnd = GetDlgItem( m_hWnd, *sliderIter );
		if ( nullptr != sliderWnd ) {
			SendMessage( sliderWnd, TBM_SETRANGEMIN, TRUE /*redraw*/, static_cast<int>( Settings::EQ::MinGain * s_SliderScale ) );
			SendMessage( sliderWnd, TBM_SETRANGEMAX, TRUE /*redraw*/, static_cast<int>( Settings::EQ::MaxGain * s_SliderScale ) );
			SendMessage( sliderWnd, TBM_SETTICFREQ, s_SliderTicFreq, 0 );

			const int position = GetPositionFromGain( gain );
			m_SliderPositions.insert( SliderValueMap::value_type( sliderWnd, position ) );
			m_SliderFrequencies.insert( SliderValueMap::value_type( sliderWnd, freq ) );
			SendMessage( sliderWnd, TBM_SETPOS, TRUE /*redraw*/, position );
		}

		++labelIter;
		++sliderIter;
		++gainIter;
	}

	// Set dialog position, and show if necessary.
	if ( ( Settings::EQ::Centred == m_CurrentEQ.X ) || ( Settings::EQ::Centred == m_CurrentEQ.Y  ) ) {
		CentreDialog( m_hWnd );
	} else {
		WINDOWPLACEMENT placement = {};
		placement.length = sizeof( WINDOWPLACEMENT );
		if ( GetWindowPlacement( m_hWnd, &placement ) ) {
			placement.showCmd = SW_HIDE;
			placement.flags = 0;
			const int width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
			const int height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
			placement.rcNormalPosition.left = m_CurrentEQ.X;
			placement.rcNormalPosition.top = m_CurrentEQ.Y;
			placement.rcNormalPosition.right = placement.rcNormalPosition.left + width;
			placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + height;
			SetWindowPlacement( m_hWnd, &placement );
		}
	}
	if ( m_CurrentEQ.Visible ) {
		ShowWindow( m_hWnd, SW_SHOW );
	}
}

bool DlgEQ::IsVisible() const
{
	return m_CurrentEQ.Visible;
}

void DlgEQ::ToggleVisibility()
{
	const int cmdShow = m_CurrentEQ.Visible ? SW_HIDE : SW_SHOW;
	ShowWindow( m_hWnd, cmdShow );
	m_CurrentEQ.Visible = !m_CurrentEQ.Visible;
}

void DlgEQ::SaveSettings()
{
	WINDOWPLACEMENT placement = {};
	placement.length = sizeof( WINDOWPLACEMENT );
	if ( GetWindowPlacement( m_hWnd, &placement ) ) {
		m_CurrentEQ.X = placement.rcNormalPosition.left;
		m_CurrentEQ.Y = placement.rcNormalPosition.top;
	}
	m_Settings.SetEQSettings( m_CurrentEQ );
}

void DlgEQ::OnSliderChange( const HWND slider )
{
	bool updateOutput = false;

	const int sliderPosition = static_cast<int>( SendMessage( slider, TBM_GETPOS, 0, 0 ) );
	const WORD sliderID = static_cast<WORD>( GetWindowLongPtr( slider, GWL_ID ) );
	if ( IDC_EQ_PREAMP == sliderID ) {
		const float gain = GetGainFromPosition( sliderPosition );
		if ( gain != m_CurrentEQ.Preamp ) {
			m_CurrentEQ.Preamp = gain;
			updateOutput = true;
		}
	} else {
		auto sliderPositionIter = m_SliderPositions.find( slider );
		if ( m_SliderPositions.end() != sliderPositionIter ) {
			if ( sliderPosition != sliderPositionIter->second ) {
				sliderPositionIter->second = sliderPosition;
				auto sliderFrequencyIter = m_SliderFrequencies.find( slider );
				if ( m_SliderFrequencies.end() != sliderFrequencyIter ) {
					auto gainIter = m_CurrentEQ.Gains.find( sliderFrequencyIter->second );
					if ( m_CurrentEQ.Gains.end() != gainIter ) {
						const float gain = GetGainFromPosition( sliderPosition );
						gainIter->second = gain;
						updateOutput = true;
					}
				}
			}
		}
	}

	if ( updateOutput ) {
		m_Output.UpdateEQ( m_CurrentEQ );
	}
}

float DlgEQ::GetGainFromPosition( const int position ) const
{
	const float gain = static_cast<float>( -position ) / s_SliderScale;
	return gain;
}

int DlgEQ::GetPositionFromGain( const float gain ) const
{
	const int position = static_cast<int>( gain * -s_SliderScale );
	return position;
}

void DlgEQ::OnEnable()
{
	const bool enabled = ( BST_CHECKED ==  IsDlgButtonChecked( m_hWnd, IDC_EQ_ENABLE ) );
	if ( enabled != m_CurrentEQ.Enabled ) {
		m_CurrentEQ.Enabled = enabled;
		m_Output.UpdateEQ( m_CurrentEQ );
	}
}

void DlgEQ::OnReset()
{
	bool updateOutput = false;

	const HWND preampWnd = GetDlgItem( m_hWnd, IDC_EQ_PREAMP );
	if ( nullptr != preampWnd ) {
		const int sliderPosition = static_cast<int>( SendMessage( preampWnd, TBM_GETPOS, 0, 0 ) );
		if ( 0 != sliderPosition ) {
			SendMessage( preampWnd, TBM_SETPOS, TRUE /*redraw*/, 0 );
			m_CurrentEQ.Preamp = 0;
			updateOutput = true;
		}
	}

	for ( auto& sliderPositionIter : m_SliderPositions ) {
		if ( 0 != sliderPositionIter.second ) {
			sliderPositionIter.second = 0;
			const HWND sliderWnd = sliderPositionIter.first;
			SendMessage( sliderWnd, TBM_SETPOS, TRUE /*redraw*/, 0 );
			auto sliderFrequencyIter = m_SliderFrequencies.find( sliderWnd );
			if ( m_SliderFrequencies.end() != sliderFrequencyIter ) {
				auto gainIter = m_CurrentEQ.Gains.find( sliderFrequencyIter->second );
				if ( m_CurrentEQ.Gains.end() != gainIter ) {
					gainIter->second = 0;
					updateOutput = true;
				}			
			}
		}
	}

	if ( updateOutput ) {
		m_Output.UpdateEQ( m_CurrentEQ );
	}
}

std::wstring DlgEQ::GetTooltip( const HWND slider ) const
{
	const int sliderPosition = static_cast<int>( SendMessage( slider, TBM_GETPOS, 0, 0 ) );
	const float gain = GetGainFromPosition( sliderPosition );
	const int bufSize = 16;
	WCHAR db[ bufSize ] = {};
	LoadString( m_hInst, IDS_UNITS_DB, db, bufSize );
	std::wstringstream ss;
	ss << std::fixed << std::setprecision( 1 ) << std::showpos << gain << L" " << db;
	const std::wstring tooltip = ss.str();
	return tooltip;
}

void DlgEQ::Init( const HWND parent )
{
	CreateDialogParam( m_hInst, MAKEINTRESOURCE( IDD_EQ ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}