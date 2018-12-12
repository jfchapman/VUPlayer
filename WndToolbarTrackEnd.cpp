#include "WndToolbarTrackEnd.h"

#include "resource.h"
#include "Utility.h"

// Toolbar button size.
static const int s_ButtonSize = 24;

WndToolbarTrackEnd::WndToolbarTrackEnd( HINSTANCE instance, HWND parent ) :
	WndToolbar( instance, parent, ID_TOOLBAR_TRACKEND ),
	m_ImageList( nullptr ),
	m_ImageMap( {
		{ 0, IDI_STOPTRACKEND },
		{ 1, IDI_FADEOUT },
		{ 2, IDI_FADETONEXT }
	} )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

WndToolbarTrackEnd::~WndToolbarTrackEnd()
{
	ImageList_Destroy( m_ImageList );
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
	const bool enableFadeOut = ( !isFadeToNext && ( Output::State::Playing == outputState ) );
	const bool enableFadeToNext = ( !isFadeOut && ( Output::State::Playing == outputState ) );

	SetButtonEnabled( ID_CONTROL_STOPTRACKEND, true );
	SetButtonEnabled( ID_CONTROL_FADEOUT, enableFadeOut );
	SetButtonEnabled( ID_CONTROL_FADETONEXT, enableFadeToNext );
}

void WndToolbarTrackEnd::CreateImageList()
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

void WndToolbarTrackEnd::CreateButtons()
{
	CreateImageList();

	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( m_ImageList ) );
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
