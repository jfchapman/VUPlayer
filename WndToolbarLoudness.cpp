#include "WndToolbarLoudness.h"

#include "resource.h"

WndToolbarLoudness::WndToolbarLoudness( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_LOUDNESS, settings, { IDI_LOUDNESS } )
{
	CreateButtons();
}

void WndToolbarLoudness::Update( Output& output, const Playlist::Ptr /*playlist*/, const Playlist::Item& /*selectedItem*/ )
{
	SetButtonEnabled( ID_CONTROL_LOUDNESS, true );
	SetButtonChecked( ID_CONTROL_LOUDNESS, output.GetLoudnessNormalisation() );
}

void WndToolbarLoudness::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_CONTROL_LOUDNESS;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarLoudness::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_LOUDNESS;
	return tooltip;
}
