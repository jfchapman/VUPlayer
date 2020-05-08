#include "GainCalculator.h"

#include "ebur128.h"

DWORD WINAPI GainCalculator::CalcThreadProc( LPVOID lpParam )
{
	GainCalculator* gainCalculator = reinterpret_cast<GainCalculator*>( lpParam );
	if ( nullptr != gainCalculator ) {
		gainCalculator->Handler();
	}
	return 0;
}

GainCalculator::GainCalculator( Library& library, const Handlers& handlers ) :
	m_Library( library ),
	m_Handlers( handlers ),
	m_AlbumQueue(),
	m_Mutex(),
	m_StopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_WakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_Thread( NULL ),
	m_PendingCount( {} )
{
	if ( ( NULL != m_StopEvent ) && ( NULL != m_WakeEvent ) ) {
		m_Thread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, CalcThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

GainCalculator::~GainCalculator()
{
	Stop();
}

void GainCalculator::Calculate( const Playlist::ItemList& items )
{
	if ( !items.empty() ) {
		std::lock_guard<std::mutex> lock( m_Mutex );
		for ( const auto& item : items ) {
			AddPending( item );
		}
		SetEvent( m_WakeEvent );
	}
}

void GainCalculator::Stop()
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

void GainCalculator::AddPending( const Playlist::Item& item )
{
	const long channels = item.Info.GetChannels();
	const long samplerate = item.Info.GetSampleRate();
	const std::wstring& album = item.Info.GetAlbum();
	const AlbumKey albumKey = { channels, samplerate, album };
	auto albumIter = m_AlbumQueue.find( albumKey );
	if ( m_AlbumQueue.end() == albumIter ) {
		albumIter = m_AlbumQueue.insert( AlbumMap::value_type( albumKey, Playlist::ItemList() ) ).first;
	}
	if ( m_AlbumQueue.end() != albumIter ) {
		bool addTrack = true;
		Playlist::ItemList& itemList = albumIter->second;
		for ( auto itemIter = itemList.begin(); addTrack && ( itemList.end() != itemIter ); itemIter++ ) {
			addTrack = ( item.Info.GetFilename() != itemIter->Info.GetFilename() );
		}
		if ( addTrack ) {
			itemList.push_back( item );
			++m_PendingCount;
		}
	}
}

void GainCalculator::Handler()
{
	HANDLE eventHandles[ 2 ] = { m_StopEvent, m_WakeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		AlbumKey albumKey = {};
		Playlist::ItemList pendingItems;
		{
			std::lock_guard<std::mutex> lock( m_Mutex );
			const auto iter = m_AlbumQueue.begin();
			if ( m_AlbumQueue.end() == iter ) {
				ResetEvent( m_WakeEvent );
			} else {
				albumKey = iter->first;
				pendingItems = iter->second;
				m_AlbumQueue.erase( iter );
			}
		}

		if ( !pendingItems.empty() ) {
			std::mutex itemMutex;
			std::mutex r128StatesMutex;

			std::vector<ebur128_state*> r128States;
			r128States.reserve( pendingItems.size() );

			CanContinue canContinue( [ stopEvent = m_StopEvent ] ()
			{
				return ( WAIT_OBJECT_0 != WaitForSingleObject( stopEvent, 0 ) );
			} );

			// Update track gain for all items.
			Playlist::ItemList processedItems;
			const size_t threadCount = min( pendingItems.size(), max( 1, static_cast<size_t>( std::thread::hardware_concurrency() ) ) );
			std::list<std::thread> threads;
			for ( size_t threadIndex = 0; threadIndex < threadCount; threadIndex++ ) {
				threads.push_back( std::thread( [ &pendingItems, &processedItems, &itemMutex, &r128States, &r128StatesMutex, canContinue, this ]() 
				{
					Playlist::Item item = {};
					{
						std::lock_guard<std::mutex> lock( itemMutex );
						if ( !pendingItems.empty() ) {
							item = pendingItems.front();
							pendingItems.pop_front();
						}
					}

					while ( 0 != item.ID ) {					
						Decoder::Ptr decoder = OpenDecoder( item );
						if ( decoder ) {
							const unsigned int channels = static_cast<unsigned int>( decoder->GetChannels() );
							const unsigned long samplerate = static_cast<unsigned long>( decoder->GetSampleRate() );
							ebur128_state* r128State = ebur128_init( channels, samplerate, EBUR128_MODE_I );
							if ( nullptr != r128State ) {
								const long sampleSize = 4096;
								std::vector<float> buffer( sampleSize * channels );

								int errorState = EBUR128_SUCCESS;
								long samplesRead = decoder->Read( &buffer[ 0 ], sampleSize );
								while ( ( EBUR128_SUCCESS == errorState ) && ( samplesRead > 0 ) && canContinue() ) {
									errorState = ebur128_add_frames_float( r128State, &buffer[ 0 ], static_cast<size_t>( samplesRead ) );
									samplesRead = decoder->Read( &buffer[ 0 ], sampleSize );
								}
								decoder.reset();

								if ( ( EBUR128_SUCCESS == errorState ) && canContinue() ) {
									double loudness = 0;
									errorState = ebur128_loudness_global( r128State, &loudness );
									if ( EBUR128_SUCCESS == errorState ) {
										const float trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
										if ( trackGain != item.Info.GetGainTrack() ) {
											MediaInfo previousMediaInfo( item.Info );
											item.Info.SetGainTrack( trackGain );
											m_Library.UpdateMediaTags( previousMediaInfo, item.Info );

											for ( const auto& duplicate : item.Duplicates ) {
												previousMediaInfo.SetFilename( duplicate );
												MediaInfo updatedMediaInfo( item.Info );
												updatedMediaInfo.SetFilename( duplicate );
												m_Library.UpdateMediaTags( previousMediaInfo, updatedMediaInfo );
											}
										}
										std::lock_guard<std::mutex> itemLock( itemMutex );
										processedItems.push_back( item );
										std::lock_guard<std::mutex> lock( r128StatesMutex );
										r128States.push_back( r128State );
									}
								}
							}
						}

						item = {};
						if ( canContinue() ) {
							std::lock_guard<std::mutex> lock( itemMutex );
							if ( !pendingItems.empty() ) {
								item = pendingItems.front();
								pendingItems.pop_front();
							}
						}

						--m_PendingCount;
					}
				}	) );
			}
			for ( auto& thread : threads ) {
				thread.join();
			}

			const std::wstring& album = std::get< 2 >( albumKey );
			if ( canContinue() && !album.empty() ) {
				// Update album gain for all items.
				double loudness = 0;
				int errorState = ebur128_loudness_global_multiple( &r128States[ 0 ], r128States.size(), &loudness );
				if ( EBUR128_SUCCESS == errorState ) {
					const float albumGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
					for ( auto item = processedItems.begin(); ( processedItems.end() != item ) && canContinue(); item++ ) {
						if ( albumGain != item->Info.GetGainAlbum() ) {
							MediaInfo previousMediaInfo( item->Info );
							item->Info.SetGainAlbum( albumGain );
							m_Library.UpdateMediaTags( previousMediaInfo, item->Info );

							for ( const auto& duplicate : item->Duplicates ) {
								previousMediaInfo.SetFilename( duplicate );
								MediaInfo updatedMediaInfo( item->Info );
								updatedMediaInfo.SetFilename( duplicate );
								m_Library.UpdateMediaTags( previousMediaInfo, updatedMediaInfo );
							}
						}
					}
				}
			}

			for ( const auto& state : r128States ) {
				ebur128_state* r128State = state;
				ebur128_destroy( &r128State );
			}
		}
	}
}

int GainCalculator::GetPendingCount() const
{
	return m_PendingCount.load();
}

Decoder::Ptr GainCalculator::OpenDecoder( const Playlist::Item& item ) const
{
	Decoder::Ptr decoder = m_Handlers.OpenDecoder( item.Info.GetFilename() );
	if ( !decoder ) {
		auto duplicate = item.Duplicates.begin();
		while ( !decoder && ( item.Duplicates.end() != duplicate ) ) {
			decoder = m_Handlers.OpenDecoder( *duplicate );
			++duplicate;
		}
	}
	return decoder;
}

float GainCalculator::CalculateTrackGain( const std::wstring& filename, const Handlers& handlers, CanContinue canContinue )
{
	float gain = NAN;
	if ( nullptr != canContinue ) {
		const Decoder::Ptr decoder = handlers.OpenDecoder( filename );
		if ( decoder ) {
			const unsigned int channels = static_cast<unsigned int>( decoder->GetChannels() );
			const unsigned long samplerate = static_cast<unsigned long>( decoder->GetSampleRate() );
			ebur128_state* r128State = ebur128_init( channels, samplerate, EBUR128_MODE_I );
			if ( nullptr != r128State ) {
				const long sampleSize = 4096;
				std::vector<float> buffer( sampleSize * channels );

				int errorState = EBUR128_SUCCESS;
				long samplesRead = decoder->Read( &buffer[ 0 ], sampleSize );
				while ( ( EBUR128_SUCCESS == errorState ) && ( samplesRead > 0 ) && canContinue() ) {
					errorState = ebur128_add_frames_float( r128State, &buffer[ 0 ], static_cast<size_t>( samplesRead ) );
					samplesRead = decoder->Read( &buffer[ 0 ], sampleSize );
				}

				if ( ( EBUR128_SUCCESS == errorState ) && canContinue() ) {
					double loudness = 0;
					errorState = ebur128_loudness_global( r128State, &loudness );
					if ( EBUR128_SUCCESS == errorState ) {
						gain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
					}
				}
				ebur128_destroy( &r128State );
			}
		}
	}
	return gain;
}
