#include "DecoderBass.h"

#include "VUPlayer.h"
#include "Utility.h"

// Default pan separation for MOD music.
static const DWORD sDefaultPanSeparation = 35;

// Fade out length, in seconds.
static const float sFadeOutLength = 15.0f;

// Maximum length of silence to return when a URL stream stalls, in seconds.
static const float sMaxSilenceURL = 10.0f;

DecoderBass::DecoderBass( const std::wstring& filename ) :
	Decoder(),
	m_Handle( 0 ),
	m_FadeStartPosition( 0 ),
	m_FadeEndPosition( 0 ),
	m_CurrentPosition( 0 ),
	m_IsURL( IsURL( filename ) ),
	m_CurrentSilenceSamples( 0 )
{
	DWORD flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
	
	if ( m_IsURL ) {
		m_Handle = BASS_StreamCreateURL( filename.c_str(), 0 /*offset*/, flags, 0 /*downloadProc*/, 0 /*user*/ );
	} else {
		// Try loading a stream.
		m_Handle = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
		if ( 0 == m_Handle ) {
			// Try loading a music file.
			flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_MUSIC_DECODE | BASS_MUSIC_NOSAMPLE;
			m_Handle = BASS_MusicLoad( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags, 1 /*freq*/ );
			if ( 0 != m_Handle ) {
				BASS_CHANNELINFO info = {};
				BASS_ChannelGetInfo( m_Handle, &info );
				BASS_MusicFree( m_Handle );
				m_Handle = 0;
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( ( BASS_CTYPE_MUSIC_MOD & info.ctype ) && ( nullptr != vuplayer ) ) {
					// Determine which settings to apply.
					long long modSettings = 0;
					long long mtmSettings = 0;
					long long s3mSettings = 0;
					long long xmSettings = 0;
					long long itSettings = 0;
					vuplayer->GetApplicationSettings().GetMODSettings( modSettings, mtmSettings, s3mSettings, xmSettings, itSettings );
					long long settingsValue = 0;
					switch ( info.ctype ) {
						case BASS_CTYPE_MUSIC_IT : {
							settingsValue = itSettings;
							break;
						}
						case BASS_CTYPE_MUSIC_XM : {
							settingsValue = xmSettings;
							break;
						}
						case BASS_CTYPE_MUSIC_S3M : {
							settingsValue = s3mSettings;
							break;
						}
						case BASS_CTYPE_MUSIC_MTM : {
							settingsValue = mtmSettings;
							break;
						}
						default : {
							settingsValue = modSettings;
							break;
						}
					}
					flags = static_cast<DWORD>( settingsValue & ( BASS_MUSIC_RAMP | BASS_MUSIC_RAMPS | BASS_MUSIC_SURROUND | BASS_MUSIC_SURROUND2 | BASS_MUSIC_NONINTER | BASS_MUSIC_SINCINTER | BASS_MUSIC_STOPBACK ) );
					DWORD panning = static_cast<DWORD>( settingsValue >> 32 );
					if ( ( panning < 0 ) || ( panning > 100 ) ) {
						panning = sDefaultPanSeparation;
					}
					flags |= BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_MUSIC_DECODE | BASS_MUSIC_PRESCAN;
					m_Handle = BASS_MusicLoad( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags, 1 /*freq*/ );
					if ( 0 != m_Handle ) {
						BASS_ChannelSetAttribute( m_Handle, BASS_ATTRIB_MUSIC_PANSEP, static_cast<float>( panning ) );
						if ( ( settingsValue & VUPLAYER_MUSIC_FADEOUT ) && ( info.chans > 0 ) ) {
							const QWORD bytes = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE );
							m_FadeStartPosition = bytes / ( info.chans * 4 );				
							m_FadeEndPosition = m_FadeStartPosition + BASS_ChannelSeconds2Bytes( m_Handle, sFadeOutLength ) / ( info.chans * 4 );
						}
					}
				}
			}
		}
	}

	if ( 0 != m_Handle ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( m_Handle, &info );
		SetSampleRate( static_cast<long>( info.freq ) );
		SetBPS( static_cast<long>( info.origres ) );
		SetChannels( static_cast<long>( info.chans ) );

		const QWORD bytes = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE );
		SetDuration( static_cast<float>( BASS_ChannelBytes2Seconds( m_Handle, bytes ) ) );

		float bitrate = 0;
		if ( TRUE == BASS_ChannelGetAttribute( m_Handle, BASS_ATTRIB_BITRATE, &bitrate ) ) {
			SetBitrate( bitrate );
		}
	} else {
		throw std::runtime_error( "DecoderBass could not load file" );
	}
}

DecoderBass::~DecoderBass()
{
	if ( FALSE == BASS_StreamFree( m_Handle ) ) {
		BASS_MusicFree( m_Handle );
	}
}

long DecoderBass::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	const long channels = GetChannels();
	if ( ( channels > 0 ) && ( sampleCount > 0 ) ) {
		samplesRead = static_cast<long>( BASS_ChannelGetData( m_Handle, buffer, static_cast<DWORD>( sampleCount * channels * 4 ) ) );
		if ( samplesRead < 0 ) {
			samplesRead = 0;
		} else {
			samplesRead /= ( channels * 4 );
		}
	}

	if ( m_FadeStartPosition > 0 ) {
		if ( m_CurrentPosition > m_FadeEndPosition ) {
			samplesRead = 0;
		} else if ( m_CurrentPosition > m_FadeStartPosition ) {
			for ( long pos = 0; pos < samplesRead; pos++ ) {
				float scale = static_cast<float>( m_FadeEndPosition - m_CurrentPosition - pos ) / ( m_FadeEndPosition - m_FadeStartPosition );
				if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
					scale = 0;
				}
				for ( long channel = 0; channel < channels; channel++ ) {
					buffer[ pos * channels + channel ] *= scale;
				}
			}
		}
		m_CurrentPosition += samplesRead;
	}

	if ( m_IsURL ) {
		if ( 0 == samplesRead ) {
			const long sampleRate = GetSampleRate();
			if ( ( sampleRate > 0 ) && ( static_cast<float>( m_CurrentSilenceSamples ) / sampleRate < sMaxSilenceURL ) ) {
				for ( int i = 0; i < sampleCount * channels; i++ ) {
					buffer[ i ] = 0;
				}
				samplesRead = sampleCount;
				m_CurrentSilenceSamples += sampleCount;
			}
		} else {
			m_CurrentSilenceSamples = 0;
		}
	}

	return samplesRead;
}

float DecoderBass::Seek( const float position )
{
	DWORD flags = BASS_POS_BYTE;
	BASS_CHANNELINFO info = {};
	BASS_ChannelGetInfo( m_Handle, &info );
	if ( BASS_CTYPE_MUSIC_MOD & info.ctype ) {
		flags |= BASS_POS_DECODETO;
	}
	QWORD bytes = BASS_ChannelSeconds2Bytes( m_Handle, position );
	BASS_ChannelSetPosition( m_Handle, bytes, flags );
	bytes = BASS_ChannelGetPosition( m_Handle, BASS_POS_BYTE );
	const float seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_Handle, bytes ) );

	if ( m_FadeEndPosition > 0 ) {
		bytes = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE ) - bytes;
		m_FadeStartPosition = bytes / ( info.chans * 4 );				
		m_FadeEndPosition = m_FadeStartPosition + BASS_ChannelSeconds2Bytes( m_Handle, sFadeOutLength ) / ( info.chans * 4 );
	}

	return seconds;
}

float DecoderBass::CalculateTrackGain( CanContinue canContinue, const float secondsLimit )
{
	return m_IsURL ? NAN : Decoder::CalculateTrackGain( canContinue, secondsLimit );
}
