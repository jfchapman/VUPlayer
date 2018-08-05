#include "WndToolbarFavourites.h"

#include "resource.h"
#include "Utility.h"

// Toolbar button size.
static const int s_ButtonSize = 24;

WndToolbarFavourites::WndToolbarFavourites( HINSTANCE instance, HWND parent ) :
	WndToolbar( instance, parent ),
	m_ImageList( nullptr )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

WndToolbarFavourites::~WndToolbarFavourites()
{
	ImageList_Destroy( m_ImageList );
}

void WndToolbarFavourites::Update( Output& /*output*/, const Playlist::Ptr playlist, const Playlist::Item& selectedItem )
{
	const bool enabled = ( playlist && ( Playlist::Type::Favourites != playlist->GetType() ) && ( Playlist::Type::CDDA != playlist->GetType() ) && ( selectedItem.ID > 0 ) );
	SetButtonEnabled( ID_FILE_ADDTOFAVOURITES, enabled );
}

void WndToolbarFavourites::CreateImageList()
{
	const float dpiScale = GetDPIScaling();
	const int cx = static_cast<int>( s_ButtonSize * dpiScale );
	const int cy = static_cast<int>( s_ButtonSize * dpiScale );
	const int imageCount = 1;
	m_ImageList = ImageList_Create( cx, cy, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ );
	HICON hIcon = static_cast<HICON>( LoadImage( GetInstanceHandle(), MAKEINTRESOURCE( IDI_FAVOURITES ), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR | LR_SHARED ) );
	if ( NULL != hIcon ) {
		ImageList_ReplaceIcon( m_ImageList, -1, hIcon );
	}
}

void WndToolbarFavourites::CreateButtons()
{
	CreateImageList();

	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( m_ImageList ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_ADDTOFAVOURITES;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarFavourites::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_ADDTOFAVOURITES;
	return tooltip;
}
