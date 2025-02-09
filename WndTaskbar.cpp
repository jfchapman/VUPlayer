#include "WndTaskbar.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <array>

WndTaskbar::WndTaskbar( HINSTANCE instance, HWND parent ) :
	m_hInst( instance ),
	m_hWnd( parent )
{
}

WndTaskbar::~WndTaskbar()
{
	if ( nullptr != m_TaskbarList ) {
		m_TaskbarList->Release();
	}
	if ( nullptr != m_ImageList ) {
		ImageList_Destroy( m_ImageList );
	}
}

void WndTaskbar::Init( Settings& settings )
{
	if ( SUCCEEDED( CoCreateInstance( CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &m_TaskbarList ) ) ) ) {
		SetToolbarButtonColour( settings );

		constexpr UINT kButtonCount = 3;
		constexpr THUMBBUTTONMASK mask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
		m_ThumbButtons.resize( kButtonCount );

		m_ThumbButtons[ 0 ].dwMask = mask;
		m_ThumbButtons[ 0 ].iId = ID_CONTROL_PREVIOUSTRACK;
		m_ThumbButtons[ 0 ].iBitmap = 2;
		m_ThumbButtons[ 0 ].dwFlags = THBF_ENABLED;
		LoadString( m_hInst, IDS_CONTROL_PREVIOUS, m_ThumbButtons[ 0 ].szTip, 260 );

		m_ThumbButtons[ 1 ].dwMask = mask;
		m_ThumbButtons[ 1 ].iId = ID_CONTROL_PLAY;
		m_ThumbButtons[ 1 ].iBitmap = 0;
		m_ThumbButtons[ 1 ].dwFlags = THBF_ENABLED;
		LoadString( m_hInst, IDS_CONTROL_PLAY, m_ThumbButtons[ 1 ].szTip, 260 );

		m_ThumbButtons[ 2 ].dwMask = mask;
		m_ThumbButtons[ 2 ].iId = ID_CONTROL_NEXTTRACK;
		m_ThumbButtons[ 2 ].iBitmap = 3;
		m_ThumbButtons[ 2 ].dwFlags = THBF_ENABLED;
		LoadString( m_hInst, IDS_CONTROL_NEXT, m_ThumbButtons[ 2 ].szTip, 260 );

		if ( FAILED( m_TaskbarList->ThumbBarAddButtons( m_hWnd, kButtonCount, m_ThumbButtons.data() ) ) ) {
			m_TaskbarList->Release();
			m_TaskbarList = nullptr;
		}
	}
	m_ProgressEnabled = settings.GetTaskbarShowProgress();
}


void WndTaskbar::Update( WndToolbarPlayback& toolbarPlayback, Output& output )
{
	UpdateToolbarButtons( toolbarPlayback );
	UpdateProgress( output );
}

void WndTaskbar::UpdateToolbarButtons( WndToolbarPlayback& toolbarPlayback )
{
	if ( nullptr != m_TaskbarList ) {
		std::vector<THUMBBUTTON> buttonsToUpdate;

		const THUMBBUTTONFLAGS previousFlags = toolbarPlayback.IsButtonEnabled( ID_CONTROL_PREVIOUSTRACK ) ? THBF_ENABLED : THBF_DISABLED;
		if ( previousFlags != m_ThumbButtons[ 0 ].dwFlags ) {
			m_ThumbButtons[ 0 ].dwFlags = previousFlags;
			m_ThumbButtons[ 0 ].dwMask = THB_FLAGS;
			buttonsToUpdate.emplace_back( m_ThumbButtons[ 0 ] );
		}

		const THUMBBUTTONFLAGS playFlags = toolbarPlayback.IsButtonEnabled( ID_CONTROL_PLAY ) ? THBF_ENABLED : THBF_DISABLED;
		const bool isPauseShown = toolbarPlayback.IsPauseShown();
		const UINT playBitmap = isPauseShown ? 1 : 0;
		const bool playFlagsChanged = ( playFlags != m_ThumbButtons[ 1 ].dwFlags );
		const bool playBitmapChanged = ( playBitmap != m_ThumbButtons[ 1 ].iBitmap );
		if ( playFlagsChanged || playBitmapChanged ) {
			m_ThumbButtons[ 1 ].dwMask = static_cast<THUMBBUTTONMASK>( 0 );
			if ( playFlagsChanged ) {
				m_ThumbButtons[ 1 ].dwFlags = playFlags;
				m_ThumbButtons[ 1 ].dwMask |= THB_FLAGS;
			}
			if ( playBitmapChanged ) {
				m_ThumbButtons[ 1 ].iBitmap = playBitmap;
				LoadString( m_hInst, isPauseShown ? IDS_CONTROL_PAUSE : IDS_CONTROL_PLAY, m_ThumbButtons[ 1 ].szTip, 260 );
				m_ThumbButtons[ 1 ].dwMask |= THB_BITMAP | THB_TOOLTIP;
			}
			buttonsToUpdate.emplace_back( m_ThumbButtons[ 1 ] );
		}

		const THUMBBUTTONFLAGS nextFlags = toolbarPlayback.IsButtonEnabled( ID_CONTROL_NEXTTRACK ) ? THBF_ENABLED : THBF_DISABLED;
		if ( nextFlags != m_ThumbButtons[ 2 ].dwFlags ) {
			m_ThumbButtons[ 2 ].dwFlags = nextFlags;
			m_ThumbButtons[ 2 ].dwMask = THB_FLAGS;
			buttonsToUpdate.emplace_back( m_ThumbButtons[ 2 ] );
		}

		if ( !buttonsToUpdate.empty() ) {
			m_TaskbarList->ThumbBarUpdateButtons( m_hWnd, static_cast<UINT>( buttonsToUpdate.size() ), buttonsToUpdate.data() );
		}
	}
}

void WndTaskbar::UpdateProgress( Output& output )
{
	if ( nullptr != m_TaskbarList ) {
		const Output::Item item = output.GetCurrentPlaying();
		const auto position = item.Position;
		const auto duration = item.PlaylistItem.Info.GetDuration();
		const auto outputState = output.GetState();

		TBPFLAG progressState = TBPF_NOPROGRESS;
		if ( m_ProgressEnabled && ( duration > 0 ) && ( Output::State::Stopped != outputState ) ) {
			progressState = ( Output::State::Paused == outputState ) ? TBPF_PAUSED : TBPF_NORMAL;
		}
		if ( progressState != m_ProgressState ) {
			m_ProgressState = progressState;
			m_TaskbarList->SetProgressState( m_hWnd, m_ProgressState );
		}

		constexpr unsigned long long kCounterSteps = 200;
		unsigned long long counter = 0;
		if ( ( duration > 0 ) && ( TBPF_NOPROGRESS != m_ProgressState ) ) {
			counter = std::clamp( static_cast<unsigned long long>( kCounterSteps * position / duration ), 1ull, kCounterSteps );
		}
		if ( counter != m_ProgressCounter ) {
			m_ProgressCounter = counter;
			m_TaskbarList->SetProgressValue( m_hWnd, m_ProgressCounter, kCounterSteps );
		}
	}
}

void WndTaskbar::UpdateProgress( const float progress )
{
	if ( nullptr != m_TaskbarList ) {
		const TBPFLAG progressState = m_ProgressEnabled ? TBPF_NORMAL : TBPF_NOPROGRESS;
		if ( progressState != m_ProgressState ) {
			m_ProgressState = progressState;
			m_TaskbarList->SetProgressState( m_hWnd, m_ProgressState );
		}

		constexpr unsigned long long kCounterSteps = 200;
		unsigned long long counter = 0;
		if ( TBPF_NOPROGRESS != m_ProgressState ) {
			counter = std::clamp( static_cast<unsigned long long>( kCounterSteps * progress ), 1ull, kCounterSteps );
		}
		if ( counter != m_ProgressCounter ) {
			m_ProgressCounter = counter;
			m_TaskbarList->SetProgressValue( m_hWnd, m_ProgressCounter, kCounterSteps );
		}
	}
}

void WndTaskbar::SetToolbarButtonColour( Settings& settings )
{
	if ( nullptr != m_TaskbarList ) {
		constexpr std::array kIcons = { IDI_PLAY, IDI_PAUSE, IDI_PREVIOUS, IDI_NEXT };
		constexpr size_t kIconCount = kIcons.size();

		const int buttonWidth = GetSystemMetrics( SM_CXICON );
		const int buttonHeight = GetSystemMetrics( SM_CYICON );

		const HIMAGELIST imageList = ImageList_Create( buttonWidth, buttonHeight, ILC_COLOR32, 0 /*initial*/, kIconCount /*grow*/ );
		if ( nullptr != imageList ) {
			const COLORREF colour = settings.GetTaskbarButtonColour();
			for ( const auto& iconID : kIcons ) {
				if ( const HBITMAP bitmap = CreateColourBitmap( m_hInst, iconID, buttonWidth, buttonHeight, colour ); nullptr != bitmap ) {
					ImageList_Add( imageList, bitmap, nullptr );
				}
			}

			m_TaskbarList->ThumbBarSetImageList( m_hWnd, imageList );

			if ( nullptr != m_ImageList ) {
				ImageList_Destroy( m_ImageList );
			}
			m_ImageList = imageList;
		}
	}
}

void WndTaskbar::EnableProgressBar( const bool enable )
{
	m_ProgressEnabled = enable;
}
