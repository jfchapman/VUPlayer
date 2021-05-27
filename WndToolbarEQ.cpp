#include "WndToolbarEQ.h"

#include "resource.h"

WndToolbarEQ::WndToolbarEQ( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_EQ, settings, { IDI_EQ } )
{
	CreateButtons();
}

void WndToolbarEQ::Update( const bool eqVisible )
{
	SetButtonEnabled( ID_VIEW_EQ, true );
	SetButtonChecked( ID_VIEW_EQ, eqVisible );
}

void WndToolbarEQ::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_VIEW_EQ;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarEQ::GetTooltip( const UINT /*commandID*/ ) const
{
	const UINT tooltip = IDS_EQ;
	return tooltip;
}
