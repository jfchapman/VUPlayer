#include "WndToolbarCrossfade.h"

#include "resource.h"

WndToolbarCrossfade::WndToolbarCrossfade( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_CROSSFADE, settings, { IDI_CROSSFADE } )
{
	CreateButtons();
}

void WndToolbarCrossfade::Update( Output& output, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	SetButtonEnabled( ID_CONTROL_CROSSFADE, true );
	SetButtonChecked( ID_CONTROL_CROSSFADE, output.GetCrossfade() );
}

void WndToolbarCrossfade::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_CONTROL_CROSSFADE;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarCrossfade::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_CROSSFADE;
	return tooltip;
}
