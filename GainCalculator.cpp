#include "GainCalculator.h"

#include "Utility.h"

#include "ebur128.h"

DWORD WINAPI GainCalculator::CalcThreadProc( LPVOID lpParam )
{
	GainCalculator* gainCalculator = reinterpret_cast<GainCalculator*>( lpParam );
	if ( nullptr != gainCalculator ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		gainCalculator->Handler();
		CoUninitialize();
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

void GainCalculator::Calculate( const Playlist::Items& items )
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
		albumIter = m_AlbumQueue.insert( AlbumMap::value_type( albumKey, Playlist::Items() ) ).first;
	}
	if ( m_AlbumQueue.end() != albumIter ) {
		bool addTrack = true;
		Playlist::Items& itemList = albumIter->second;
		for ( auto itemIter = itemList.begin(); addTrack && ( itemList.end() != itemIter ); itemIter++ ) {
			addTrack = std::tie( item.Info.GetFilename(), item.Info.GetCueStart(), item.Info.GetCueEnd() ) != std::tie( itemIter->Info.GetFilename(), itemIter->Info.GetCueStart(), itemIter->Info.GetCueEnd() );
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
		Playlist::Items pendingItems;
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
			std::reverse( pendingItems.begin(), pendingItems.end() );

			const std::wstring& album = std::get< 2 >( albumKey );
			const bool calculateAlbumGain = !album.empty();

			std::mutex itemMutex;
			std::mutex r128StatesMutex;

			std::vector<ebur128_state*> r128States;
			if ( calculateAlbumGain ) {
				r128States.reserve( pendingItems.size() );
			}

			Decoder::CanContinue canContinue( [ stopEvent = m_StopEvent ] ()
				{
					return ( WAIT_OBJECT_0 != WaitForSingleObject( stopEvent, 0 ) );
				} );

			// Ensure item information is up to date before modifying.
			for ( auto& item : pendingItems ) {
				if ( !canContinue() ) {
					break;
				} else if ( MediaInfo mediaInfo( item.Info ); m_Library.GetMediaInfo( mediaInfo ) ) {
					item.Info = mediaInfo;
				}
			}

			// Yes, this is the way!
			if ( !canContinue() )
				continue;

			// Update track gain for all items.
			Playlist::Items processedItems;
			const size_t threadCount = std::min<size_t>( pendingItems.size(), std::max<size_t>( 1, std::thread::hardware_concurrency() ) );
			std::list<std::thread> threads;
			for ( size_t threadIndex = 0; threadIndex < threadCount; threadIndex++ ) {
				threads.push_back( std::thread( [ &pendingItems, &processedItems, &itemMutex, &r128States, &r128StatesMutex, canContinue, calculateAlbumGain, this ] ()
					{
						Playlist::Item item = {};
						{
							std::lock_guard<std::mutex> lock( itemMutex );
							if ( !pendingItems.empty() ) {
								item = pendingItems.back();
								pendingItems.pop_back();
							}
						}

						while ( 0 != item.ID ) {
							Decoder::Ptr decoder = OpenDecoder( item );
							if ( decoder ) {
								const unsigned int channels = static_cast<unsigned int>( decoder->GetChannels() );
								const unsigned long samplerate = static_cast<unsigned long>( decoder->GetSampleRate() );
								ebur128_state* r128State = ebur128_init( channels, samplerate, EBUR128_MODE_I );
								if ( nullptr != r128State ) {
									bool destroyState = true;

									const long sampleSize = 4096;
									std::vector<float> buffer( sampleSize * channels );

									int errorState = EBUR128_SUCCESS;
									long samplesRead = decoder->ReadSamples( buffer.data(), sampleSize );
									while ( ( EBUR128_SUCCESS == errorState ) && ( samplesRead > 0 ) && canContinue() ) {
										errorState = ebur128_add_frames_float( r128State, buffer.data(), static_cast<size_t>( samplesRead ) );
										samplesRead = decoder->ReadSamples( buffer.data(), sampleSize );
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

											if ( calculateAlbumGain ) {
												std::lock_guard<std::mutex> lock( r128StatesMutex );
												r128States.push_back( r128State );
												destroyState = false;
											}
										}
									}
									if ( destroyState ) {
										ebur128_destroy( &r128State );
									}
								}
							}

							item = {};
							if ( canContinue() ) {
								std::lock_guard<std::mutex> lock( itemMutex );
								if ( !pendingItems.empty() ) {
									item = pendingItems.back();
									pendingItems.pop_back();
								}
							}

							--m_PendingCount;
						}
					} ) );
			}
			for ( auto& thread : threads ) {
				thread.join();
			}

			if ( canContinue() && calculateAlbumGain ) {
				// Update album gain for all items.
				double loudness = 0;
				int errorState = ebur128_loudness_global_multiple( r128States.data(), r128States.size(), &loudness );
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
	Decoder::Ptr decoder;
	if ( !IsURL( item.Info.GetFilename() ) ) {
		decoder = m_Handlers.OpenDecoder( item.Info, Decoder::Context::Temporary );
		if ( !decoder ) {
			auto duplicate = item.Duplicates.begin();
			while ( !decoder && ( item.Duplicates.end() != duplicate ) ) {
				MediaInfo duplicateInfo( item.Info );
				duplicateInfo.SetFilename( *duplicate );
				decoder = m_Handlers.OpenDecoder( duplicateInfo, Decoder::Context::Temporary );
				++duplicate;
			}
		}
	}
	return decoder;
}

std::optional<float> GainCalculator::CalculateTrackGain( const Playlist::Item& item, const Handlers& handlers, Decoder::CanContinue canContinue )
{
	std::optional<float> gain;
	if ( ( nullptr != canContinue ) && !IsURL( item.Info.GetFilename() ) ) {
		const Decoder::Ptr decoder = handlers.OpenDecoder( item.Info, Decoder::Context::Temporary );
		if ( decoder ) {
			gain = decoder->CalculateTrackGain( canContinue );
		}
	}
	return gain;
}
