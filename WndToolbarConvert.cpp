#include "WndToolbarConvert.h"

#include "resource.h"

WndToolbarConvert::WndToolbarConvert( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_CONVERT, settings, { IDI_CONVERT } ),
	m_TooltipID( 0 )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

void WndToolbarConvert::Update( const Playlist::Ptr playlist )
{
	const bool enabled = ( playlist && playlist->CanConvertAnyItems() );
	SetButtonEnabled( ID_FILE_CONVERT, enabled );
	m_TooltipID = ( playlist && Playlist::Type::CDDA == playlist->GetType() ) ? IDS_EXTRACT_TRACKS : IDS_CONVERT_TRACKS;
}

void WndToolbarConvert::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_CONVERT;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarConvert::GetTooltip( const UINT /*commandID*/ ) const
{
	return m_TooltipID;
}
