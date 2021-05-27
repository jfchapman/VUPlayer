#include "WndToolbarFavourites.h"

#include "resource.h"

WndToolbarFavourites::WndToolbarFavourites( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_FAVOURITES, settings, { IDI_FAVOURITES } )
{
	CreateButtons();
}

void WndToolbarFavourites::Update( Output& /*output*/, const Playlist::Ptr playlist, const Playlist::Item& selectedItem )
{
	const bool enabled = ( playlist && ( Playlist::Type::Favourites != playlist->GetType() ) && ( Playlist::Type::CDDA != playlist->GetType() ) && ( selectedItem.ID > 0 ) );
	SetButtonEnabled( ID_FILE_ADDTOFAVOURITES, enabled );
}

void WndToolbarFavourites::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
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
