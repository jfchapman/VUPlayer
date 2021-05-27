#include "WndToolbarTrackEnd.h"

#include "resource.h"

#include "Utility.h"

WndToolbarTrackEnd::WndToolbarTrackEnd( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_TRACKEND, settings, { IDI_STOPTRACKEND, IDI_FADEOUT, IDI_FADETONEXT } )
{
	CreateButtons();
}

void WndToolbarTrackEnd::Update( Output& output, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	const bool isStopAtTrackEnd = output.GetStopAtTrackEnd();
	const bool isFadeOut = output.GetFadeOut();
	const bool isFadeToNext = output.GetFadeToNext();

	SetButtonChecked( ID_CONTROL_STOPTRACKEND, isStopAtTrackEnd );
	SetButtonChecked( ID_CONTROL_FADEOUT, isFadeOut );
	SetButtonChecked( ID_CONTROL_FADETONEXT, isFadeToNext );

	const Output::State outputState = output.GetState();
	const bool isStream =	IsURL( output.GetCurrentPlaying().PlaylistItem.Info.GetFilename() );
	const bool enableFadeOut = ( !isFadeToNext && ( Output::State::Playing == outputState ) );
	const bool enableFadeToNext = ( !isFadeOut && !isStream && ( Output::State::Playing == outputState ) );

	SetButtonEnabled( ID_CONTROL_STOPTRACKEND, true );
	SetButtonEnabled( ID_CONTROL_FADEOUT, enableFadeOut );
	SetButtonEnabled( ID_CONTROL_FADETONEXT, enableFadeToNext );
}

void WndToolbarTrackEnd::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 3;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_CONTROL_STOPTRACKEND;
	buttons[ 1 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 1 ].iBitmap = 1;
	buttons[ 1 ].idCommand = ID_CONTROL_FADEOUT;
	buttons[ 2 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 2 ].iBitmap = 2;
	buttons[ 2 ].idCommand = ID_CONTROL_FADETONEXT;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarTrackEnd::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_CONTROL_STOPTRACKEND : {
			tooltip = IDS_HOTKEY_STOPATEND;
			break;
		}
		case ID_CONTROL_FADEOUT : {
			tooltip = IDS_HOTKEY_FADEOUT;
			break;
		}
		case ID_CONTROL_FADETONEXT : {
			tooltip = IDS_HOTKEY_FADETONEXT;
			break;
		}
		default : {
			break;
		}
	}
	return tooltip;
}
