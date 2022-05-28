#include "WndToolbarPlayback.h"

#include "resource.h"

WndToolbarPlayback::WndToolbarPlayback( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_PLAYBACK, settings, { IDI_PLAY, IDI_PAUSE, IDI_STOP, IDI_PREVIOUS, IDI_NEXT } )
{
	CreateButtons();
}

void WndToolbarPlayback::Update( Output& output, const Playlist::Ptr playlist, const Playlist::Item& selectedItem )
{
	bool playEnabled = false;
	bool stopEnabled = false;
	bool previousEnabled = false;
	bool nextEnabled = false;
	bool showPause = false;
	const Output::State state = output.GetState();
	switch ( state ) {
		case Output::State::Playing : {
			Playlist::Ptr outputPlaylist = output.GetPlaylist();
			if ( outputPlaylist ) {
				playEnabled = true;
				stopEnabled = true;
				showPause = true;
				previousEnabled = nextEnabled = ( outputPlaylist->GetCount() > 0 );
			}
			break;
		}
		case Output::State::Paused : {
			Playlist::Ptr outputPlaylist = output.GetPlaylist();
			if ( outputPlaylist ) {
				playEnabled = true;
				stopEnabled = true;
				previousEnabled = nextEnabled = ( outputPlaylist->GetCount() > 0 );
			}
			break;
		}
		case Output::State::Stopped : {
			if ( playlist ) {
				playEnabled = ( playlist->GetCount() > 0 );
				previousEnabled = nextEnabled = ( 0 != selectedItem.ID );
			}
			break;
		}
		default : {
			break;
		}
	}
	SetPaused( showPause );
	SetButtonEnabled( ID_CONTROL_PLAY, playEnabled );
	SetButtonEnabled( ID_CONTROL_STOP, stopEnabled );
	SetButtonEnabled( ID_CONTROL_PREVIOUSTRACK, previousEnabled );
	SetButtonEnabled( ID_CONTROL_NEXTTRACK, nextEnabled );
}

void WndToolbarPlayback::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 4;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_CONTROL_PLAY;
	buttons[ 1 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 1 ].iBitmap = 2;
	buttons[ 1 ].idCommand = ID_CONTROL_STOP;
	buttons[ 2 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 2 ].iBitmap = 3;
	buttons[ 2 ].idCommand = ID_CONTROL_PREVIOUSTRACK;
	buttons[ 3 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 3 ].iBitmap = 4;
	buttons[ 3 ].idCommand = ID_CONTROL_NEXTTRACK;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

void WndToolbarPlayback::SetPaused( const bool paused ) const
{
	if ( IsPauseShown() != paused ) {
		const int imageIndex = paused ? 1 : 0;
		SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_PLAY, imageIndex );
	}
}

bool WndToolbarPlayback::IsPauseShown() const
{
	TBBUTTONINFO info = {};
	info.cbSize = sizeof( TBBUTTONINFO );
	info.dwMask = TBIF_IMAGE;
	SendMessage( GetWindowHandle(), TB_GETBUTTONINFO, ID_CONTROL_PLAY, reinterpret_cast<LPARAM>( &info ) );
	const bool isPauseShown = ( 1 == info.iImage );
	return isPauseShown;
}

UINT WndToolbarPlayback::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_CONTROL_PLAY : {
			tooltip = IsPauseShown() ? IDS_CONTROL_PAUSE : IDS_CONTROL_PLAY;
			break;
		}
		case ID_CONTROL_STOP : {
			tooltip = IDS_CONTROL_STOP;
			break;
		}
		case ID_CONTROL_PREVIOUSTRACK : {
			tooltip = IDS_CONTROL_PREVIOUS;
			break;
		}
		case ID_CONTROL_NEXTTRACK : {
			tooltip = IDS_CONTROL_NEXT;
			break;
		}
		default : {
			break;
		}
	}
	return tooltip;
}
