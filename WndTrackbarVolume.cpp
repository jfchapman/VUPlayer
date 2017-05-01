#include "WndTrackbarVolume.h"

#include "resource.h"

#include <sstream>

WndTrackbarVolume::WndTrackbarVolume( HINSTANCE instance, HWND parent, Output& output ) :
	WndTrackbar( instance, parent, output, 0 /*minValue*/, 100 /*maxValue*/ ),
	m_Tooltip()
{
	const int volume = static_cast<int>( GetOutput().GetVolume() * 100 );
	SendMessage( GetWindowHandle(), TBM_SETPOS, TRUE /*redraw*/, volume );
}

WndTrackbarVolume::~WndTrackbarVolume()
{
}

const std::wstring& WndTrackbarVolume::GetTooltipText()
{
	const int volume = static_cast<int>( GetOutput().GetVolume() * 100 );

	const int bufSize = 32;
	WCHAR buf[ bufSize ] = {};
	LoadString( GetInstanceHandle(), IDS_VOLUME, buf, bufSize );

	std::wstringstream ss;
	ss << buf << L": " << volume << L"%";
	m_Tooltip = ss.str();
	return m_Tooltip;
}

void WndTrackbarVolume::OnPositionChanged( const int position )
{
	UpdateVolume( position );
}

void WndTrackbarVolume::OnDrag( const int position )
{
	UpdateVolume( position );
}

void WndTrackbarVolume::UpdateVolume( const int position )
{
	const float pos = static_cast<float>( position );
	const float range = static_cast<float>( GetRange() );
	if ( range > 0.0f ) {
		const float volume = pos / range;
		GetOutput().SetVolume( volume );
	}
}

void WndTrackbarVolume::Update()
{
	const int previousPosition = GetPosition();
	const int position = static_cast<int>( GetOutput().GetVolume() * GetRange() );
	if ( previousPosition != position ) {
		SetPosition( position );
	}
}