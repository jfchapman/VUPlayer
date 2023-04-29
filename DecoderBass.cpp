#include "DecoderBass.h"

#include "VUPlayer.h"
#include "Utility.h"

// Default pan separation for MOD music.
static constexpr DWORD sDefaultPanSeparation = 35;

// Fade out length, in seconds.
static constexpr float sFadeOutLength = 15.0f;

// Maximum length of silence to return when a URL stream stalls, in seconds.
static constexpr float sMaxSilenceURL = 10.0f;

// Music file extensions.
static constexpr std::array sMusicFileExtensions = { L"mod", L"s3m", L"xm", L"mtm", L"mo3", L"umx" };

#include <array>
#include <fstream>
#include <optional>

DecoderBass::DecoderBass( const std::wstring& filename, const Context context ) :
	Decoder( context ),
	m_Handle( 0 ),
	m_FadeStartPosition( 0 ),
	m_FadeEndPosition( 0 ),
	m_CurrentPosition( 0 ),
	m_IsURL( IsURL( filename ) ),
	m_CurrentSilenceSamples( 0 ),
	m_StreamTitle( {} ),
	m_StreamTitleMutex()
{
	DWORD flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
	
	if ( m_IsURL ) {
		m_Handle = BASS_StreamCreateURL( filename.c_str(), 0 /*offset*/, flags, nullptr /*downloadProc*/, nullptr /*user*/ );
		if ( 0 != m_Handle ) {
			OnMetadata( m_Handle );
			BASS_ChannelSetSync( m_Handle, BASS_SYNC_META, 0 /*param*/, MetadataSyncProc, this );
		}
	} else {
		const auto fileExt = GetFileExtension( filename );
		if ( sMusicFileExtensions.end() != std::find( sMusicFileExtensions.begin(), sMusicFileExtensions.end(), fileExt ) ) {
			LoadMusic( filename );
		} else {
			// Try loading a stream.
			m_Handle = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
			if ( 0 == m_Handle ) {
				// Finally, try again with loading a music file.
				LoadMusic( filename );
			}
		}
	}

	if ( 0 != m_Handle ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( m_Handle, &info );
		SetSampleRate( static_cast<long>( info.freq ) );
		SetBPS( static_cast<long>( info.origres ) );
		SetChannels( static_cast<long>( info.chans ) );

		if ( const QWORD bytes = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE ); -1 != bytes ) {
			SetDuration( static_cast<float>( BASS_ChannelBytes2Seconds( m_Handle, bytes ) ) );
		}

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
	float seconds = 0;
	if ( QWORD bytes = BASS_ChannelSeconds2Bytes( m_Handle, position ); -1 != bytes ) {
		if ( BASS_ChannelSetPosition( m_Handle, bytes, flags ) ) {
			bytes = BASS_ChannelGetPosition( m_Handle, BASS_POS_BYTE );
			if ( -1 != bytes ) {
				seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_Handle, bytes ) );
				if ( seconds < 0 ) {
					seconds = 0;
				} else if ( m_FadeEndPosition > 0 ) {
					if ( const QWORD length = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE ); ( -1 != length ) && ( length > bytes ) ) {
						bytes = length - bytes;
						m_FadeStartPosition = bytes / ( info.chans * 4 );				
						m_FadeEndPosition = m_FadeStartPosition + BASS_ChannelSeconds2Bytes( m_Handle, sFadeOutLength ) / ( info.chans * 4 );
					}
				}
			}
		}
	}
	return seconds;
}

std::optional<float> DecoderBass::CalculateTrackGain( CanContinue canContinue, const float secondsLimit )
{
	return m_IsURL ? std::nullopt : Decoder::CalculateTrackGain( canContinue, secondsLimit );
}

void DecoderBass::OnMetadata( const DWORD channel )
{
	if ( const char* tags = BASS_ChannelGetTags( channel, BASS_TAG_META ); nullptr != tags ) {
		std::lock_guard<std::mutex> lock( m_StreamTitleMutex );
		auto& [ position, title ] = m_StreamTitle;
		position = static_cast<float>( BASS_ChannelBytes2Seconds( channel, BASS_ChannelGetPosition( channel, BASS_POS_BYTE ) ) );
		title.clear();
		
		std::wstring metadata = UTF8ToWideString( tags );
		static const std::wstring titleTag( L"StreamTitle='" );
		if ( const size_t startPos = metadata.find( titleTag ); std::wstring::npos != startPos ) {
			if ( const size_t endPos = metadata.find( L"';", startPos + titleTag.size() ); std::wstring::npos != endPos ) {
				title = metadata.substr( startPos + titleTag.size(), endPos - startPos - titleTag.size() );
			}
		}
	}
}

bool DecoderBass::SupportsStreamTitles() const
{
	return m_IsURL;
}

std::pair<float /*seconds*/, std::wstring /*title*/> DecoderBass::GetStreamTitle()
{
	return m_StreamTitle;	
}

void CALLBACK DecoderBass::MetadataSyncProc( HSYNC /*handle*/, DWORD channel, DWORD /*data*/, void *user )
{
	if ( DecoderBass* decoder = static_cast<DecoderBass*>( user ); nullptr != decoder ) {
		decoder->OnMetadata( channel );
	}
}

void DecoderBass::LoadMusic( const std::wstring& filename )
{
  VUPlayer* vuplayer = VUPlayer::Get();
  const uint32_t samplerate = ( nullptr != vuplayer ) ? vuplayer->GetApplicationSettings().GetMODSamplerate() : 48000;

  DWORD flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_MUSIC_DECODE | BASS_MUSIC_NOSAMPLE;
	m_Handle = BASS_MusicLoad( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags, samplerate );
	if ( 0 != m_Handle ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( m_Handle, &info );
		BASS_MusicFree( m_Handle );
		m_Handle = 0;
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
					if ( const QWORD bytes = BASS_ChannelGetLength( m_Handle, BASS_POS_BYTE ); -1 != bytes ) {
						m_FadeStartPosition = bytes / ( info.chans * 4 );				
						m_FadeEndPosition = m_FadeStartPosition + BASS_ChannelSeconds2Bytes( m_Handle, sFadeOutLength ) / ( info.chans * 4 );
					}
				}
			}
		}
	}
}
