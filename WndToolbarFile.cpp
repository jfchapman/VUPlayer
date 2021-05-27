#include "WndToolbarFile.h"

#include "resource.h"

WndToolbarFile::WndToolbarFile( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_FILE, settings, { IDI_NEW_PLAYLIST } )
{
	CreateButtons();
}

void WndToolbarFile::Update( Output& /*output*/, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	SetButtonEnabled( ID_FILE_NEWPLAYLIST, true );
}

void WndToolbarFile::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_NEWPLAYLIST;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarFile::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_NEW_PLAYLIST;
	return tooltip;
}
