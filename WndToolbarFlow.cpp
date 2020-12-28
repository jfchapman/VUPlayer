#include "WndToolbarFlow.h"

#include "resource.h"

WndToolbarFlow::WndToolbarFlow( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_FLOW, settings, { IDI_RANDOM, IDI_REPEAT_TRACK, IDI_REPEAT_PLAYLIST } )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

void WndToolbarFlow::Update( Output& output, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	SetButtonEnabled( ID_CONTROL_RANDOMPLAY, true );
	SetButtonEnabled( ID_CONTROL_REPEATTRACK, true );
	SetButtonEnabled( ID_CONTROL_REPEATPLAYLIST, true );

	const bool random = output.GetRandomPlay();
	const bool repeatTrack = output.GetRepeatTrack();
	const bool repeatPlaylist = output.GetRepeatPlaylist();
	SetButtonChecked( ID_CONTROL_RANDOMPLAY, random );
	SetButtonChecked( ID_CONTROL_REPEATTRACK, repeatTrack );
	SetButtonChecked( ID_CONTROL_REPEATPLAYLIST, repeatPlaylist );
}

void WndToolbarFlow::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 3;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_CONTROL_RANDOMPLAY;
	buttons[ 1 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 1 ].iBitmap = 1;
	buttons[ 1 ].idCommand = ID_CONTROL_REPEATTRACK;
	buttons[ 2 ].fsStyle = TBSTYLE_CHECK;
	buttons[ 2 ].iBitmap = 2;
	buttons[ 2 ].idCommand = ID_CONTROL_REPEATPLAYLIST;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarFlow::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_CONTROL_RANDOMPLAY : {
			tooltip = IDS_RANDOM;
			break;
		}
		case ID_CONTROL_REPEATTRACK : {
			tooltip = IDS_REPEAT_TRACK;
			break;
		}
		case ID_CONTROL_REPEATPLAYLIST : {
			tooltip = IDS_REPEAT_PLAYLIST;
			break;
		}
		default : {
			break;
		}
	}
	return tooltip;
}
