#include "WndToolbarVolume.h"

#include "resource.h"
#include "VUPlayer.h"

WndToolbarVolume::WndToolbarVolume( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_VOLUME, settings, { IDI_VOLUME, IDI_VOLUME_MUTE, IDI_PITCH, IDI_BALANCE } )
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
			case WndTrackbar::Type::Volume: {
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
			case WndTrackbar::Type::Pitch: {
				if ( ID_CONTROL_PITCHRESET != button.idCommand ) {
					SendMessage( GetWindowHandle(), TB_SETCMDID, 0, ID_CONTROL_PITCHRESET );
				}
				if ( 2 != button.iBitmap ) {
					SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_PITCHRESET, 2 );
				}
				SetButtonEnabled( button.idCommand, ( output.GetPitch() != 1.0f ) ? true : false );
				break;
			}
			case WndTrackbar::Type::Balance: {
				if ( ID_CONTROL_BALANCERESET != button.idCommand ) {
					SendMessage( GetWindowHandle(), TB_SETCMDID, 0, ID_CONTROL_BALANCERESET );
				}
				if ( 3 != button.iBitmap ) {
					SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_CONTROL_BALANCERESET, 3 );
				}
				SetButtonEnabled( button.idCommand, ( output.GetBalance() != 0 ) ? true : false );
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
	switch ( trackbarType ) {
		case WndTrackbar::Type::Pitch: {
			buttons[ 0 ].iBitmap = 2;
			buttons[ 0 ].idCommand = ID_CONTROL_PITCHRESET;
			break;
		}
		case WndTrackbar::Type::Balance: {
			buttons[ 0 ].iBitmap = 3;
			buttons[ 0 ].idCommand = ID_CONTROL_BALANCERESET;
			break;
		}
		default: {
			buttons[ 0 ].iBitmap = 0;
			buttons[ 0 ].idCommand = ID_CONTROL_MUTE;
			break;
		}
	}
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarVolume::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_CONTROL_MUTE: {
			tooltip = IDS_MUTE;
			break;
		}
		case ID_CONTROL_PITCHRESET: {
			tooltip = IDS_HOTKEY_PITCHRESET;
			break;
		}
		case ID_CONTROL_BALANCERESET: {
			tooltip = IDS_HOTKEY_BALANCERESET;
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
