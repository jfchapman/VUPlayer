#include "WndToolbarOptions.h"

#include "resource.h"

WndToolbarOptions::WndToolbarOptions( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_OPTIONS, settings, { IDI_OPTIONS } )
{
	CreateButtons();
}

void WndToolbarOptions::Update( Output& /*output*/, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	SetButtonEnabled( ID_FILE_OPTIONS, true );
}

void WndToolbarOptions::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_OPTIONS;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarOptions::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_OPTIONS_TITLE;
	return tooltip;
}
