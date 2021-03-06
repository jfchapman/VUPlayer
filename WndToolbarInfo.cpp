#include "WndToolbarInfo.h"

#include "resource.h"

WndToolbarInfo::WndToolbarInfo( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_INFO, settings, { IDI_INFO } )
{
	CreateButtons();
}

void WndToolbarInfo::Update( Output& /*output*/, const Playlist::Ptr /*playlist*/, const Playlist::Item& selectedItem )
{
	const bool enabled = ( 0 != selectedItem.ID );
	SetButtonEnabled( ID_VIEW_TRACKINFORMATION, enabled );
}

void WndToolbarInfo::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_VIEW_TRACKINFORMATION;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarInfo::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_TRACKINFO;
	return tooltip;
}
