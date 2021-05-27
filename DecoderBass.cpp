#include "DecoderBass.h"

#include "VUPlayer.h"
#include "Utility.h"

// Default pan separation for MOD music.
static const DWORD sDefaultPanSeparation = 35;

// Fade out length, in seconds.
static const float sFadeOutLength = 15.0f;

// Maximum length of silence to return when a URL stream stalls, in seconds.
static const float sMaxSilenceURL = 10.0f;

#include <array>
#include <fstream>
#include <optional>

// A class to weed out problematic Impulse Tracker files for BASS v2.4.15.0 (resolved in the next release).
class CheckIT
{
public:
	// Returns true if 'path' is an Impulse Tracker file where the last sample is compressed & truncated.
	static bool IsLastSampleCompressedAndTruncated( const std::filesystem::path& path )
	{
		bool lastSampleTruncated = false;
		if ( std::ifstream stream( path, std::ios::binary | std::ios::in ); stream.is_open() ) {
			if ( const auto impm = ReadString( stream, 4 ); impm.has_value() && ( "IMPM" == impm.value() ) ) {
				stream.seekg( 0, std::ios_base::end );
				const unsigned long long filesize = unsigned long long( stream.tellg() );
				stream.seekg( 0, std::ios_base::beg );

				stream.seekg( 0x20, std::ios_base::beg );
				const auto ordNum = ReadWord( stream );
				const auto insNum = ReadWord( stream );
				const auto smpNum = ReadWord( stream );
				if ( ordNum.has_value() && insNum.has_value() && smpNum.has_value() ) {
					unsigned long offset = 0xc0 + ordNum.value() + insNum.value() * 4;
					stream.seekg( offset, std::ios_base::beg );
					std::set<unsigned long> sampleHeaders;
					for ( unsigned long smpIdx = 0; smpIdx < smpNum; smpIdx++ ) {
						if ( const auto sampleHeader = ReadLong( stream ); sampleHeader.has_value() ) {
							sampleHeaders.insert( sampleHeader.value() );
						}
					}

					using SampleInfo = std::tuple<unsigned long /*samplePointer*/, unsigned long /*sampleCount*/, bool /*16-bit*/, bool /*stereo*/, bool /*compressed*/>; 
					std::set<SampleInfo> samples;
					for ( const auto sampleHeader : sampleHeaders ) {
						stream.seekg( sampleHeader, std::ios_base::beg );
						if ( const auto imps = ReadString( stream, 4 ); imps.has_value() && ( "IMPS" == imps.value() ) ) {
							stream.seekg( 0x12 + sampleHeader, std::ios_base::beg );
							if ( const auto flags = ReadByte( stream ); flags.has_value() ) {
								stream.seekg( 0x30 + sampleHeader, std::ios_base::beg );
								if ( const auto sampleCount = ReadLong( stream ); sampleCount.has_value() && sampleCount.value() > 0 ) {
									stream.seekg( 0x48 + sampleHeader, std::ios_base::beg );
									if ( const auto samplePointer = ReadLong( stream ); samplePointer.has_value() ) {
										samples.insert( std::make_tuple( samplePointer.value(), sampleCount.value(), 0x02 & flags.value(), 0x04 & flags.value(), 0x08 & flags.value() ) );
									}
								}
							}
						}
					}

					if ( !samples.empty() ) {
						const auto& [ samplePointer, sampleCount, is16Bit, stereo, compressed ] = *samples.rbegin();
						if ( compressed ) {
							const unsigned long samplesPerBlock = is16Bit ? 0x4000 : 0x8000;
							const unsigned long totalSamples = stereo ? ( 2 * sampleCount ) : sampleCount;
							unsigned long samplesRead = 0;
							stream.seekg( samplePointer, std::ios_base::beg );
							while ( !lastSampleTruncated && ( samplesRead < totalSamples ) ) {
								if ( const auto blockSize = ReadWord( stream ); blockSize.has_value() ) {
									const unsigned long long streamPos = unsigned long long( stream.tellg() );
									lastSampleTruncated = streamPos + blockSize.value() > filesize;
									if ( !lastSampleTruncated ) {
										samplesRead += samplesPerBlock;
										stream.seekg( blockSize.value(), std::ios_base::cur );
									}               
								} else {
									lastSampleTruncated = true;
								}                
							}
						}
					}
				}
			}
		}
		return lastSampleTruncated;
	}

private:
	static std::optional<unsigned char> ReadByte( std::ifstream& stream )
	{
		std::optional<unsigned char> result;
		char b;
		stream.read( &b, 1 );
		if ( stream.good() ) {
			result = unsigned char( b );    
		}
		return result;
	}

	static std::optional<unsigned long> ReadWord( std::ifstream& stream )
	{
		std::optional<unsigned long> result;
		std::array<char, 2> b;
		stream.read( b.data(), 2 );
		if ( stream.good() ) {
			result = unsigned long( unsigned char( b[ 1 ] ) << 8 | unsigned char( b[ 0 ] ) );    
		}
		return result;
	}

	static std::optional<unsigned long> ReadLong( std::ifstream& stream )
	{
		std::optional<unsigned long> result;
		std::array<char, 4> b;
		stream.read( b.data(), 4 );
		if ( stream.good() ) {
			result = unsigned long( unsigned char( b[ 3 ] ) << 24 | unsigned char( b[ 2 ] ) << 16 | unsigned char( b[ 1 ] ) << 8 | unsigned char( b[ 0 ] ) );
		}
		return result;
	}

	static std::optional<std::string> ReadString( std::ifstream& stream, const unsigned long length )
	{
		std::optional<std::string> result;
		std::vector<char> b( length );
		stream.read( b.data(), length );
		if ( stream.good() ) {
			result = std::string( b.begin(), b.end() );
		}
		return result;
	}
};

DecoderBass::DecoderBass( const std::wstring& filename ) :
	Decoder(),
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
		// Try loading a stream.
		m_Handle = BASS_StreamCreateFile( FALSE /*mem*/, filename.c_str(), 0 /*offset*/, 0 /*length*/, flags );
		if ( ( 0 == m_Handle ) && !CheckIT::IsLastSampleCompressedAndTruncated( filename ) ) {
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
