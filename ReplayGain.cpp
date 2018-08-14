#include "ReplayGain.h"

DWORD WINAPI ReplayGain::CalcThreadProc( LPVOID lpParam )
{
	ReplayGain* replaygain = reinterpret_cast<ReplayGain*>( lpParam );
	if ( nullptr != replaygain ) {
		replaygain->Handler();
	}
	return 0;
}

ReplayGain::ReplayGain( Library& library, const Handlers& handlers ) :
	m_Library( library ),
	m_Handlers( handlers ),
	m_AlbumQueue(),
	m_Mutex(),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Thread( NULL ),
	m_PendingCount( {} ),
	m_GainAnalysis()
{
	if ( ( NULL != m_StopEvent ) && ( NULL != m_WakeEvent ) ) {
		m_Thread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, CalcThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

ReplayGain::~ReplayGain()
{
	Stop();
}

void ReplayGain::Calculate( const MediaInfo::List& mediaList )
{
	if ( !mediaList.empty() ) {
		std::lock_guard<std::mutex> lock( m_Mutex );
		for ( const auto& mediaInfo : mediaList ) {
			AddPending( mediaInfo );
		}
		SetEvent( m_WakeEvent );
	}
}

void ReplayGain::Stop()
{
	if ( NULL != m_Thread ) {
		SetEvent( m_StopEvent );
		WaitForSingleObject( m_Thread, INFINITE );
		CloseHandle( m_StopEvent );
		m_StopEvent = NULL;
		CloseHandle( m_WakeEvent );
		m_WakeEvent = NULL;
		CloseHandle( m_Thread );
		m_Thread = NULL;
	}
}

void ReplayGain::AddPending( const MediaInfo& mediaInfo )
{
	const long channels = mediaInfo.GetChannels();
	const long samplerate = mediaInfo.GetSampleRate();
	const std::wstring& album = mediaInfo.GetAlbum();
	const AlbumKey albumKey = { channels, samplerate, album };
	auto albumIter = m_AlbumQueue.find( albumKey );
	if ( m_AlbumQueue.end() == albumIter ) {
		if ( ( 1 == channels ) || ( 2 == channels ) ) {
			albumIter = m_AlbumQueue.insert( AlbumMap::value_type( albumKey, MediaInfo::List() ) ).first;
		}
	}
	if ( m_AlbumQueue.end() != albumIter ) {
		bool addTrack = true;
		MediaInfo::List& trackList = albumIter->second;
		for ( auto trackIter = trackList.begin(); addTrack && ( trackList.end() != trackIter ); trackIter++ ) {
			addTrack = ( mediaInfo.GetFilename() != trackIter->GetFilename() );
		}
		trackList.push_back( mediaInfo );
		++m_PendingCount;
	}
}

void ReplayGain::Handler()
{
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		AlbumKey albumKey = {};
		MediaInfo::List mediaList;
		{
			std::lock_guard<std::mutex> lock( m_Mutex );
			const auto iter = m_AlbumQueue.begin();
			if ( m_AlbumQueue.end() == iter ) {
				ResetEvent( m_WakeEvent );
			} else {
				albumKey = iter->first;
				mediaList = iter->second;
				m_AlbumQueue.erase( iter );
			}
		}

		if ( !mediaList.empty() ) {
			const long channels = std::get< 0 >( albumKey );
			const long samplerate = std::get< 1 >( albumKey );
			const std::wstring& album = std::get< 2 >( albumKey );

			if ( m_GainAnalysis.InitGainAnalysis( samplerate ) ) {
				const long scale = 1 << 15;
				const long sampleSize = 4096;
				float* buffer = new float[ sampleSize * channels ];
				float* leftSamples = new float[ sampleSize ];
				float* rightSamples = ( 1 == channels ) ? nullptr : new float[ sampleSize ];
				float albumPeak = 0;

				for ( auto mediaInfo = mediaList.begin(); ( mediaList.end() != mediaInfo ) && CanContinue(); mediaInfo++ ) {
					Decoder::Ptr decoder = m_Handlers.OpenDecoder( mediaInfo->GetFilename() );
					if ( decoder ) {
						float trackPeak = 0;
						long samplesRead = decoder->Read( buffer, sampleSize );
						while ( ( samplesRead > 0 ) && CanContinue() ) {
							for ( long index = 0; index < samplesRead; index++ ) {
								const float sampleValue = buffer[ index * channels ];
								leftSamples[ index ] = sampleValue * scale;
								if ( sampleValue > trackPeak ) {
									trackPeak = sampleValue;
								}
							}
							if ( nullptr != rightSamples ) {
								for ( long index = 0; index < samplesRead; index++ ) {
									const float sampleValue = buffer[ index * channels + 1 ];
									rightSamples[ index ] = sampleValue * scale;
									if ( sampleValue > trackPeak ) {
										trackPeak = sampleValue;
									}
								}
							}
							m_GainAnalysis.AnalyzeSamples( leftSamples, rightSamples, samplesRead, channels );
							samplesRead = decoder->Read( buffer, sampleSize );
						}

						if ( CanContinue() ) {
							const float trackGain = m_GainAnalysis.GetTitleGain();
							if ( ( GAIN_NOT_ENOUGH_SAMPLES != trackGain ) ) {
								if ( trackPeak > albumPeak ) {
									albumPeak = trackPeak;
								}
								if ( ( trackGain != mediaInfo->GetGainTrack() ) || ( trackPeak != mediaInfo->GetPeakTrack() ) ) {
									MediaInfo previousMediaInfo( *mediaInfo );
									mediaInfo->SetGainTrack( trackGain );
									mediaInfo->SetPeakTrack( trackPeak );
									m_Library.UpdateMediaTags( previousMediaInfo, *mediaInfo );
								}
							}
							--m_PendingCount;
						}
					}
				}

				if ( CanContinue() && !album.empty() ) {
					const float albumGain = m_GainAnalysis.GetAlbumGain();
					if ( ( GAIN_NOT_ENOUGH_SAMPLES != albumGain ) ) {
						for ( auto mediaInfo = mediaList.begin(); ( mediaList.end() != mediaInfo ) && CanContinue(); mediaInfo++ ) {
							if ( ( albumGain != mediaInfo->GetGainAlbum() ) || ( albumPeak != mediaInfo->GetPeakAlbum() ) ) {
								MediaInfo previousMediaInfo( *mediaInfo );
								mediaInfo->SetGainAlbum( albumGain );
								mediaInfo->SetPeakAlbum( albumPeak );
								m_Library.UpdateMediaTags( previousMediaInfo, *mediaInfo );
							}
						}
					}
				}

				delete [] leftSamples;
				delete [] rightSamples;
				delete [] buffer;
			}
		}
	}
}

bool ReplayGain::CanContinue() const
{
	const bool canContinue = ( WAIT_OBJECT_0 != WaitForSingleObject( m_StopEvent, 0 ) );
	return canContinue;
}

int ReplayGain::GetPendingCount() const
{
	return m_PendingCount.load();
}
