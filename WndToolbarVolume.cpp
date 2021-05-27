#include "WndToolbarVolume.h"

#include "resource.h"
#include "VUPlayer.h"

WndToolbarVolume::WndToolbarVolume( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_VOLUME, settings, { IDI_VOLUME, IDI_VOLUME_MUTE, IDI_PITCH } )
{
	WndTrackbar::Type trackbarType = WndTrackbar::Type::Volume;
	if ( settings.GetOutputControlType() == static_cast<int>( WndTrackbar::Type::Pitch ) ) {
		trackbarType = WndTrackbar::Type::Pitch;
	}
	CreateButtons( trackbarType );
}

void WndToolbarVolume::Update( const Output& output, const WndTrackbar::Type trackbarType )
{
	if ( TBBUTTON button = {}; SendMessage( GetWindowHandle(), TB_GETBUTTON, 0, reinterpret_cast<LPARAM>( &button ) ) ) {
		switch ( trackbarType ) {
			case WndTrackbar::Type::Volume : {
				if ( ID_CONTROL_MUTE != button.idCommand ) {
					SendMessage( GetWindowHandle(), TB_SETCMDID, 0, ID_CONTROL_MUTE );
				}
				if ( output.GetMuted() ) {
					if ( 1 != button.iBitmap ) {
						SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_MUTE, 1 );
					}
				} else {
					if ( 0 != button.iBitmap ) {
						SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_MUTE, 0 );
					}
				}
				SetButtonEnabled( button.idCommand, true );
				break;
			}
			case WndTrackbar::Type::Pitch : {
				if ( ID_CONTROL_PITCHRESET != button.idCommand ) {
					SendMessage( GetWindowHandle(), TB_SETCMDID, 0, ID_CONTROL_PITCHRESET );
				}
				if ( 2 != button.iBitmap ) {
					SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_PITCHRESET, 2 );
				}
				SetButtonEnabled( button.idCommand, ( output.GetPitch() != 1.0f ) ? true : false );
				break;
			}
		}
	}
}

void WndToolbarVolume::CreateButtons( const WndTrackbar::Type trackbarType )
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 1;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = ( WndTrackbar::Type::Pitch == trackbarType ) ? 2 : 0;
	buttons[ 0 ].idCommand = ( WndTrackbar::Type::Pitch == trackbarType ) ? ID_CONTROL_PITCHRESET : ID_CONTROL_MUTE;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarVolume::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_CONTROL_MUTE : {
			tooltip = IDS_MUTE;
			break;
		}
		case ID_CONTROL_PITCHRESET : {
			tooltip = IDS_HOTKEY_PITCHRESET;
			break;
		}
	}
	return tooltip;
}

bool WndToolbarVolume::CanHide() const
{
	return false;
}

bool WndToolbarVolume::HasDivider() const
{
	return false;
}

bool WndToolbarVolume::ShowContextMenu( const POINT& position )
{
	if ( VUPlayer* vuplayer = VUPlayer::Get(); nullptr != vuplayer ) {
		vuplayer->ShowVolumeControlContextMenu( position );
	}
	return true;
}
