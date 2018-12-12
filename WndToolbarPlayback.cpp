#include "WndToolbarPlayback.h"

#include "resource.h"
#include "Utility.h"

// Toolbar button size.
static const int s_ButtonSize = 24;

WndToolbarPlayback::WndToolbarPlayback( HINSTANCE instance, HWND parent ) :
	WndToolbar( instance, parent, ID_TOOLBAR_PLAYBACK ),
	m_ImageList( nullptr ),
	m_ImageMap( {
		{ 0, IDI_PLAY },
		{ 1, IDI_PAUSE },
		{ 2, IDI_STOP },
		{ 3, IDI_PREVIOUS },
		{ 4, IDI_NEXT }
	} )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

WndToolbarPlayback::~WndToolbarPlayback()
{
	ImageList_Destroy( m_ImageList );
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
			playEnabled = ( 0 != selectedItem.ID );
			if ( playlist ) {
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

void WndToolbarPlayback::CreateImageList()
{
	const float dpiScale = GetDPIScaling();
	const int cx = static_cast<int>( s_ButtonSize * dpiScale );
	const int cy = static_cast<int>( s_ButtonSize * dpiScale );
	const int imageCount = static_cast<int>( m_ImageMap.size() );
	m_ImageList = ImageList_Create( cx, cy, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ );
	for ( const auto& iter : m_ImageMap ) {
		HICON hIcon = static_cast<HICON>( LoadImage( GetInstanceHandle(), MAKEINTRESOURCE( iter.second ), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR | LR_SHARED ) );
		if ( NULL != hIcon ) {
			ImageList_ReplaceIcon( m_ImageList, -1, hIcon );
		}
	}
}

void WndToolbarPlayback::CreateButtons()
{
	CreateImageList();

	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( m_ImageList ) );
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
