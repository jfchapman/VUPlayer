#include "WndTrackbarSeek.h"

#include "resource.h"
#include "Utility.h"

#include <sstream>

// Trackbar maximum position.
static const int s_RangeMax = 2000;

WndTrackbarSeek::WndTrackbarSeek( HINSTANCE instance, HWND parent, Output& output ) :
	WndTrackbar( instance, parent, output, 0 /*minValue*/, s_RangeMax, Type::Seek ),
	m_Tooltip(),
	m_IsDragging( false ),
	m_OutputItem()
{
}

WndTrackbarSeek::~WndTrackbarSeek()
{
}

const std::wstring& WndTrackbarSeek::GetTooltipText()
{
	if ( IsDragging() ) {
		const float duration = m_OutputItem.PlaylistItem.Info.GetDuration();
		const float seconds = GetPosition() * duration / s_RangeMax;
		m_Tooltip = DurationToString( GetInstanceHandle(), seconds, false /*colonDelimited*/ );
	} else {
		m_Tooltip.clear();
	}
	return m_Tooltip;
}

void WndTrackbarSeek::OnPositionChanged( const int position )
{
	m_IsDragging = false;
	const float duration = m_OutputItem.PlaylistItem.Info.GetDuration();
	if ( duration > 0.0f ) {
		const float seekPosition = position * duration / s_RangeMax;
		if ( m_Playlist ) {
			GetOutput().SetPlaylist( m_Playlist );
		}
		GetOutput().Play( m_OutputItem.PlaylistItem.ID, seekPosition );
	}
}

void WndTrackbarSeek::OnDrag( const int /*position*/ )
{
	m_IsDragging = true;
}

bool WndTrackbarSeek::IsDragging() const
{
	return m_IsDragging;
}

void WndTrackbarSeek::Update( Output& output, const Playlist::Ptr& playlist, const Playlist::Item& selectedItem )
{
	if ( !IsDragging() ) {
		const Output::State state = output.GetState();
		switch ( state ) {
			case Output::State::Playing : {
				m_OutputItem = output.GetCurrentPlaying();
				m_Playlist.reset();
				break;
			}
			case Output::State::Paused : {
				m_OutputItem = output.GetCurrentPlaying();
				m_Playlist.reset();
				break;
			}
			case Output::State::Stopped : {
				m_OutputItem = {};
				m_Playlist = playlist;
				if ( selectedItem.ID > 0 ) {
					m_OutputItem.PlaylistItem = selectedItem;
				}
				break;
			}
			default : {
				break;
			}
		}
		const float duration = m_OutputItem.PlaylistItem.Info.GetDuration();
		SetEnabled( duration > 0.0f );
		if ( duration > 0.0f ) {
			const float position = m_OutputItem.Position;
			const int seekPos = static_cast<int>( position * s_RangeMax / duration );
			SetPosition( seekPos );
		}
	}
}
