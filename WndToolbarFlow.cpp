#include "WndToolbarFlow.h"

#include "resource.h"
#include "Utility.h"

// Toolbar button size.
static const int s_ButtonSize = 24;

WndToolbarFlow::WndToolbarFlow( HINSTANCE instance, HWND parent ) :
	WndToolbar( instance, parent ),
	m_ImageList( nullptr ),
	m_ImageMap( {
		{ 0, IDI_RANDOM },
		{ 1, IDI_REPEAT_TRACK },
		{ 2, IDI_REPEAT_PLAYLIST }
	} )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

WndToolbarFlow::~WndToolbarFlow()
{
	ImageList_Destroy( m_ImageList );
}

void WndToolbarFlow::Update( Output& output, const Playlist::Ptr& /*playlist*/, const Playlist::Item& /*selectedItem*/ )
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

void WndToolbarFlow::CreateImageList()
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

void WndToolbarFlow::CreateButtons()
{
	CreateImageList();

	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( m_ImageList ) );
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
