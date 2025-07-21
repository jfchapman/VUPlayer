#include "Output.h"

#include "GainCalculator.h"
#include "Utility.h"
#include "VUPlayer.h"

#include "opus.h"

#include "bassasio.h"
#include "bassmix.h"
#include "basswasapi.h"

#include <cmath>

// Output buffer length, in seconds.
constexpr float s_BufferLength = 0.5f;

// Cutoff point, in seconds, after which previous track replays the current track from the beginning.
constexpr float s_PreviousTrackCutoff = 5.0f;

// Amount of time to devote to estimating a gain value, in seconds.
constexpr float s_GainPrecalcTime = 0.25f;

// Minimum allowed gain adjustment, in dB.
constexpr float s_GainMin = -20.0f;

// Maximum allowed gain adjustment, in dB.
constexpr float s_GainMax = 20.0f;

// Fade out duration, in seconds.
constexpr float s_FadeOutDuration = 5.0f;

// The relative volume at which to set the crossfade position on a track.
constexpr float s_CrossfadeVolume = 1 / 3.0f;

// The fade to next duration, in seconds.
constexpr float s_FadeToNextDuration = 3.0f;

// Indicates when fading to the next track.
constexpr long s_ItemIsFadingToNext = -1;

// The fade out duration when stopping a track (in standard mode), in milliseconds.
constexpr DWORD s_StopFadeDuration = 20;

// Maximum number of playlist items to skip when trying to switch decoder streams.
constexpr size_t s_MaxSkipItems = 20;

// Define to output debug timing for testing.
#undef DEBUG_TIMING

DWORD CALLBACK Output::StreamProc( HSTREAM handle, void *buf, DWORD length, void *user )
{
	DWORD bytesRead = 0;
	Output* output = static_cast<Output*>( user );
	if ( nullptr != output ) {
#ifdef DEBUG_TIMING
		LARGE_INTEGER perfFreq, perfCount1, perfCount2;
		QueryPerformanceFrequency( &perfFreq );
		QueryPerformanceCounter( &perfCount1 );
#endif

		float* sampleBuffer = static_cast<float*>( buf );
		bytesRead = output->ApplyLeadIn( sampleBuffer, length, handle );
		length -= bytesRead;
		if ( length > 0 ) {
			sampleBuffer += bytesRead / 4;
			bytesRead += output->ReadSampleData( sampleBuffer, length, handle );
		}
		if ( 0 == bytesRead ) {
			bytesRead = BASS_STREAMPROC_END;
			output->SetOutputStreamFinished( true );
		}

#ifdef DEBUG_TIMING
		QueryPerformanceCounter( &perfCount2 );
		const float msec = 1000 * static_cast<float>( perfCount2.QuadPart - perfCount1.QuadPart ) / perfFreq.QuadPart;
		if ( msec >= 1.0f ) {
			const std::wstring debugStr = L"VUPlayer::Output::StreamProc: " + std::to_wstring( msec ) + L"ms\r\n";
			OutputDebugString( debugStr.c_str() );
		}
#endif
	}
	return bytesRead;
}

DWORD CALLBACK Output::WasapiProc( void *buffer, DWORD length, void *user )
{
	DWORD bytesRead = 0;
	Output* output = static_cast<Output*>( user );
	if ( nullptr != output ) {
		if ( output->m_WASAPIPaused ) {
			float* sampleBuffer = static_cast<float*>( buffer );
			if ( nullptr != sampleBuffer ) {
				std::fill( sampleBuffer, sampleBuffer + length / 4, 0.0f );
			}
		} else {
			bytesRead = BASS_ChannelGetData( output->m_MixerStream, buffer, length );
			if ( 0 == bytesRead ) {
				if ( 0 == BASS_WASAPI_GetData( nullptr, BASS_DATA_AVAILABLE ) ) {
					BASS_WASAPI_Stop( TRUE /*reset*/ );
					bytesRead = BASS_STREAMPROC_END;
					output->SetOutputStreamFinished( true );
				}
			}
		}
	}
	return bytesRead;
}

void CALLBACK Output::WasapiNotifyProc( DWORD notify, DWORD device, void *user )
{
	const DWORD currentDevice = BASS_WASAPI_GetDevice();
	if ( currentDevice == device ) {
		switch ( notify ) {
			case BASS_WASAPI_NOTIFY_DISABLED:
			case BASS_WASAPI_NOTIFY_FAIL: {
				Output* output = static_cast<Output*>( user );
				if ( nullptr != output ) {
					output->m_WASAPIFailed = true;
				}
				break;
			}
		}
	}
}

void CALLBACK Output::AsioNotifyProc( DWORD notify, void *user )
{
	Output* output = static_cast<Output*>( user );
	if ( nullptr != output ) {
		if ( BASS_ASIO_NOTIFY_RESET == notify ) {
			output->m_ResetASIO = true;
		}
	}
}

void CALLBACK Output::SyncEnd( HSYNC /*handle*/, DWORD /*channel*/, DWORD /*data*/, void *user )
{
	Output* output = static_cast<Output*>( user );
	if ( nullptr != output ) {
		output->OnSyncEnd();
	}
}

void CALLBACK Output::SyncSlideStop( HSYNC /*handle*/, DWORD /*channel*/, DWORD /*data*/, void *user )
{
	HANDLE stoppedEvent = static_cast<HANDLE>( user );
	if ( nullptr != stoppedEvent ) {
		SetEvent( stoppedEvent );
	}
}

DWORD WINAPI Output::CrossfadeThreadProc( LPVOID lpParam )
{
	Output* output = static_cast<Output*>( lpParam );
	if ( nullptr != output ) {
		output->CalculateCrossfadeHandler();
	}
	return 0;
}

DWORD WINAPI Output::LoudnessPrecalcThreadProc( LPVOID lpParam )
{
	Output* output = static_cast<Output*>( lpParam );
	if ( nullptr != output ) {
		output->LoudnessPrecalcHandler();
	}
	return 0;
}

DWORD WINAPI Output::PreloadDecoderProc( LPVOID lpParam )
{
	Output* output = static_cast<Output*>( lpParam );
	if ( nullptr != output ) {
		output->PreloadDecoderHandler();
	}
	return 0;
}

Output::Output( const HINSTANCE instance, const HWND hwnd, Handlers& handlers, Settings& settings ) :
	m_hInst( instance ),
	m_Parent( hwnd ),
	m_Handlers( handlers ),
	m_Settings( settings ),
	m_Playlist(),
	m_CurrentItemDecoding( {} ),
	m_SoftClipStateDecoding(),
	m_DecoderStream(),
	m_DecoderSampleRate( 0 ),
	m_OutputStream( 0 ),
	m_MixerStream( 0 ),
	m_PlaylistMutex(),
	m_QueueMutex(),
	m_Volume( 1.0f ),
	m_Pitch( 1.0f ),
	m_Balance( 0 ),
	m_OutputQueue(),
	m_RestartItemID( 0 ),
	m_RandomPlay( false ),
	m_RepeatTrack( false ),
	m_RepeatPlaylist( false ),
	m_Crossfade( false ),
	m_CurrentSelectedPlaylistItem( {} ),
	m_GainMode( Settings::GainMode::Disabled ),
	m_LimitMode( Settings::LimitMode::None ),
	m_GainPreamp( 0 ),
	m_LoudnessNormalisation( m_Settings.GetLoudnessNormalisation() ),
	m_RetainStopAtTrackEnd( m_Settings.GetRetainStopAtTrackEnd() ),
	m_StopAtTrackEnd( m_RetainStopAtTrackEnd ? m_Settings.GetStopAtTrackEnd() : false ),
	m_Muted( false ),
	m_FadeOut( false ),
	m_FadeToNext( false ),
	m_SwitchToNext( false ),
	m_FadeOutStartPosition( 0 ),
	m_LastTransitionPosition( 0 ),
	m_CrossfadePosition( 0 ),
	m_CrossfadeItem( {} ),
	m_CrossfadeThread( nullptr ),
	m_CrossfadeStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_LoudnessPrecalcThread( nullptr ),
	m_LoudnessPrecalcStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_PreloadDecoderThread( nullptr ),
	m_PreloadDecoderStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_PreloadDecoderWakeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_CrossfadingStream(),
	m_CrossfadingStreamMutex(),
	m_CurrentItemCrossfading( {} ),
	m_CrossfadingItemID( 0 ),
	m_SoftClipStateCrossfading(),
	m_CrossfadeSeekOffset( 0 ),
	m_GainEstimateMap(),
	m_CurrentEQ( m_Settings.GetEQSettings() ),
	m_FX(),
	m_EQEnabled( m_CurrentEQ.Enabled ),
	m_EQPreamp( m_CurrentEQ.Preamp ),
	m_OutputMode( Settings::OutputMode::Standard ),
	m_OutputDevice(),
	m_WASAPIFailed( false ),
	m_WASAPIPaused( false ),
	m_ResetASIO( false ),
	m_OutputStreamFinished( false ),
	m_MixerStreamHasEndSync( false ),
	m_LeadInSeconds( 0 ),
	m_PausedOnStartup( false ),
	m_PreloadedDecoder( {} ),
	m_PreloadedDecoderMutex(),
	m_StreamTitleQueue(),
	m_StreamTitleMutex(),
	m_FollowTrackSelection( m_Settings.GetFollowTrackSelection() ),
	m_FollowPlaylistInformation( {} ),
	m_FollowPlaylistInformationMutex(),
	m_OnPlaylistChangeCallback( nullptr ),
	m_OnSelectFollowedTrackCallback( nullptr ),
	m_OnPreBufferFinishedCallback( [ this ] ( const long id )
		{
			if ( id != m_CrossfadingItemID ) {
				std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
				if ( m_PreloadedDecoder.decoder ) {
					m_PreloadedDecoder.decoder->PreBuffer( m_OnPreBufferFinishedCallback );
				}
			}
		}
	)
{
	InitialiseOutput();

	SetVolume( m_Settings.GetVolume() );
	if ( m_Settings.GetRetainPitchBalance() ) {
		const auto [pitch, balance] = m_Settings.GetPitchBalance();
		SetPitch( pitch );
		SetBalance( balance );
	}

	m_Settings.GetGainSettings( m_GainMode, m_LimitMode, m_GainPreamp );
	bool crossfade = false;
	m_Settings.GetPlaybackSettings( m_RandomPlay, m_RepeatTrack, m_RepeatPlaylist, crossfade );
	m_Crossfade = crossfade;

	StartPreloadDecoderThread();
}

Output::~Output()
{
	StopCrossfadeCalculationThread();
	CloseHandle( m_CrossfadeStopEvent );

	StopLoudnessPrecalcThread();
	CloseHandle( m_LoudnessPrecalcStopEvent );

	StopPreloadDecoderThread();
	CloseHandle( m_PreloadDecoderStopEvent );
	CloseHandle( m_PreloadDecoderWakeEvent );

	Stop();
	if ( -1 != BASS_ASIO_GetDevice() ) {
		BASS_ASIO_Free();
	}

	BASS_Free();
}

void Output::Play( const long playlistID, const double seek )
{
	Stop();

	Playlist::Item item( { playlistID, MediaInfo() } );
	if ( ( 0 == item.ID ) && m_Playlist ) {
		const Playlist::Items items = m_Playlist->GetItems();
		if ( !items.empty() ) {
			item.ID = items.front().ID;
		}
	}

	if ( m_Playlist && m_Playlist->GetItem( item ) ) {
		m_DecoderStream = OpenOutputDecoder( item );
		if ( m_DecoderStream ) {

			const DWORD outputBufferSize = static_cast<DWORD>( 1000 * ( ( ( MediaInfo::Source::CDDA ) == item.Info.GetSource() ) ? ( 2 * s_BufferLength ) : s_BufferLength ) );
			const DWORD previousOutputBufferSize = BASS_GetConfig( BASS_CONFIG_BUFFER );
			if ( previousOutputBufferSize != outputBufferSize ) {
				BASS_SetConfig( BASS_CONFIG_BUFFER, outputBufferSize );
			}

			EstimateGain( item );

			m_DecoderSampleRate = m_DecoderStream->GetOutputSampleRate();
			const DWORD freq = static_cast<DWORD>( m_DecoderSampleRate );
			double seekPosition = seek;
			if ( 0.0f != seekPosition ) {
				if ( seekPosition < 0 ) {
					seekPosition = item.Info.GetDuration() + seekPosition;
					if ( seekPosition < 0 ) {
						seekPosition = 0;
					}
				}
				seekPosition = m_DecoderStream->Seek( seekPosition );
			} else if ( GetCrossfade() ) {
				m_DecoderStream->SkipSilence();
			}

			if ( !IsURL( item.Info.GetFilename() ) ) {
				m_DecoderStream->PreBuffer( m_OnPreBufferFinishedCallback );
			}

			if ( CreateOutputStream( item.Info ) ) {
				m_CurrentItemDecoding = item;
				UpdateOutputVolume();
				if ( 1.0f != m_Pitch ) {
					BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_FREQ, freq * m_Pitch );
				}
				if ( 0 != m_Balance ) {
					BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_PAN, m_Balance );
				}
				UpdateEQ( m_CurrentEQ );

				const State state = StartOutput();
				if ( State::Playing == state ) {
					Queue queue = GetOutputQueue();
					queue.push_back( { item, 0, seekPosition } );
					SetOutputQueue( queue );
					if ( GetCrossfade() ) {
						StartCrossfadeCalculationThread( item, seekPosition );
					}
					StartLoudnessPrecalcThread();
					PreloadNextDecoder( item );
				} else {
					Stop();
				}
			}
		}
	}
}

void Output::Stop()
{
	if ( 0 != m_OutputStream ) {
		if ( Settings::OutputMode::Standard == m_OutputMode ) {
			if ( BASS_ACTIVE_PLAYING == BASS_ChannelIsActive( m_OutputStream ) ) {
				const HANDLE slideFinishedEvent = CreateEvent( nullptr /*attributes*/, FALSE /*manualReset*/, FALSE /*initial*/, L"" /*name*/ );
				if ( nullptr != slideFinishedEvent ) {
					const HSYNC syncSlide = BASS_ChannelSetSync( m_OutputStream, BASS_SYNC_SLIDE | BASS_SYNC_ONETIME, 0 /*param*/, SyncSlideStop, slideFinishedEvent );
					if ( 0 != syncSlide ) {
						if ( BASS_ChannelSlideAttribute( m_OutputStream, BASS_ATTRIB_VOL, -1 /*fadeOutAndStop*/, s_StopFadeDuration ) ) {
							WaitForSingleObject( slideFinishedEvent, 2 * s_StopFadeDuration );
						}
					}
					CloseHandle( slideFinishedEvent );
				}
			}
			if ( BASS_ACTIVE_STOPPED != BASS_ChannelIsActive( m_OutputStream ) ) {
				BASS_ChannelStop( m_OutputStream );
			}
		}

		if ( -1 != BASS_WASAPI_GetDevice() ) {
			if ( BASS_WASAPI_IsStarted() ) {
				BASS_WASAPI_Stop( TRUE /*reset*/ );
			}
			BASS_WASAPI_SetNotify( nullptr, nullptr );
			BASS_WASAPI_Free();
		}

		if ( -1 != BASS_ASIO_GetDevice() ) {
			if ( BASS_ASIO_IsStarted() ) {
				BASS_ASIO_Stop();
				if ( BASS_ASIO_ACTIVE_PAUSED == BASS_ASIO_ChannelIsActive( FALSE /*input*/, 0 /*channel*/ ) ) {
					BASS_ASIO_ChannelReset( FALSE /*input*/, -1 /*channel*/, BASS_ASIO_RESET_PAUSE );
				}
			}
		}

		BASS_StreamFree( m_OutputStream );
		m_OutputStream = 0;

		if ( 0 != m_MixerStream ) {
			BASS_StreamFree( m_MixerStream );
			m_MixerStream = 0;
		}
	}

	m_FX.clear();
	m_DecoderSampleRate = 0;
	m_DecoderStream.reset();
	m_CrossfadingStream.reset();
	m_CurrentItemDecoding = {};
	m_SoftClipStateDecoding.clear();
	m_CurrentItemCrossfading = {};
	m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
	m_SoftClipStateCrossfading.clear();
	m_RestartItemID = 0;
	SetOutputQueue( {} );
	m_FadeOut = false;
	m_FadeToNext = false;
	m_SwitchToNext = false;
	m_FadeOutStartPosition = 0;
	m_LastTransitionPosition = 0;
	m_WASAPIFailed = false;
	m_WASAPIPaused = false;
	m_OutputStreamFinished = false;
	m_MixerStreamHasEndSync = false;
	m_PausedOnStartup = false;
	StopCrossfadeCalculationThread();
	StopLoudnessPrecalcThread();
	std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
	m_PreloadedDecoder = {};
	SetStreamTitleQueue( {} );
}

void Output::Pause()
{
	if ( m_PausedOnStartup ) {
		m_PausedOnStartup = false;
		if ( const auto queue = GetOutputQueue(); !queue.empty() )
			Play( queue.front().PlaylistItem.ID, queue.front().InitialSeek );
		return;
	}

	const State state = GetState();
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard: {
			if ( State::Paused == state ) {
				BASS_ChannelPlay( m_OutputStream, FALSE /*restart*/ );
			} else if ( State::Playing == state ) {
				BASS_ChannelPause( m_OutputStream );
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive: {
			if ( ( -1 != BASS_WASAPI_GetDevice() ) && BASS_WASAPI_IsStarted() ) {
				m_WASAPIPaused = !m_WASAPIPaused;
			}
			break;
		}
		case Settings::OutputMode::ASIO: {
			if ( -1 != BASS_ASIO_GetDevice() ) {
				if ( BASS_ASIO_ACTIVE_PAUSED == BASS_ASIO_ChannelIsActive( FALSE /*input*/, 0 /*channel*/ ) ) {
					if ( TRUE != BASS_ASIO_ChannelReset( FALSE /*input*/, -1 /*channel*/, BASS_ASIO_RESET_PAUSE ) ) {
						Stop();
					}
				} else if ( BASS_ASIO_IsStarted() ) {
					BASS_ASIO_ChannelPause( FALSE /*input*/, static_cast<DWORD>( -1 ) /*channel*/ );
				}
			}
			break;
		}
	}
}

void Output::Previous( const bool forcePrevious, const float seek )
{
	const State state = GetState();
	if ( m_Playlist && ( ( State::Paused == state ) || ( State::Playing == state ) ) ) {
		const Output::Item outputItem = GetCurrentPlaying();
		const Playlist::Item currentItem = outputItem.PlaylistItem;
		if ( currentItem.ID > 0 ) {
			if ( forcePrevious || ( outputItem.Position < s_PreviousTrackCutoff ) ) {
				Playlist::Item previousItem = {};

				if ( GetFollowTrackSelection() ) {
					if ( const auto nextTrackToFollow = GetTrackToFollow( currentItem, true /*previous*/ ) ) {
						const auto& [playlist, nextTrack, selectTrack] = *nextTrackToFollow;
						previousItem = nextTrack;
						ChangePlaylist( playlist );
					}
				} else {
					if ( GetRandomPlay() ) {
						std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
						if ( ( m_PreloadedDecoder.item.ID > 0 ) && ( m_PreloadedDecoder.item.ID != currentItem.ID ) && m_Playlist->ContainsItem( { m_PreloadedDecoder.item } ) ) {
							previousItem = m_PreloadedDecoder.item;
						} else {
							previousItem = m_Playlist->GetRandomItem( currentItem );
						}
					} else {
						m_Playlist->GetPreviousItem( currentItem, previousItem, GetRepeatPlaylist() );
					}
				}

				if ( previousItem.ID > 0 ) {
					Play( previousItem.ID, seek );
				}
			} else {
				Play( currentItem.ID, seek );
			}
		}
	}
}

void Output::Next()
{
	const State state = GetState();
	if ( m_Playlist && ( ( State::Paused == state ) || ( State::Playing == state ) ) ) {
		const Output::Item outputItem = GetCurrentPlaying();
		const Playlist::Item currentItem = outputItem.PlaylistItem;
		Playlist::Item nextItem = {};

		if ( GetFollowTrackSelection() ) {
			if ( const auto nextTrackToFollow = GetTrackToFollow( currentItem ) ) {
				const auto& [playlist, nextTrack, selectTrack] = *nextTrackToFollow;
				nextItem = nextTrack;
				ChangePlaylist( playlist );
			}
		} else {
			if ( GetRandomPlay() ) {
				std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
				if ( ( m_PreloadedDecoder.item.ID > 0 ) && ( m_PreloadedDecoder.item.ID != currentItem.ID ) && m_Playlist->ContainsItem( { m_PreloadedDecoder.item } ) ) {
					nextItem = m_PreloadedDecoder.item;
				} else {
					nextItem = m_Playlist->GetRandomItem( currentItem );
				}
			} else {
				m_Playlist->GetNextItem( currentItem, nextItem, GetRepeatPlaylist() );
			}
		}

		if ( nextItem.ID > 0 ) {
			Play( nextItem.ID );
		}
	}
}

void Output::Play( const Playlist::Ptr playlist, const long startID, const float seek )
{
	Stop();
	ChangePlaylist( playlist );
	Play( startID, seek );
}

void Output::Startup( const Playlist::Ptr playlist, const long startID, const float seek, const bool paused )
{
	if ( playlist && paused && ( seek > 0.0f ) ) {
		Playlist::Item item( { startID, MediaInfo() } );
		if ( playlist->GetItem( item ) ) {
			ChangePlaylist( playlist );
			const Queue queue = { { item, 0, seek } };
			SetOutputQueue( queue );
			m_PausedOnStartup = true;
		}
	}	else {
		Play( playlist, startID, seek );
	}
}

Playlist::Ptr Output::GetPlaylist()
{
	return m_Playlist;
}

Output::State Output::GetState()
{
	if ( m_PausedOnStartup )
		return State::Paused;

	State state = State::Stopped;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard: {
			const DWORD channelState = BASS_ChannelIsActive( m_OutputStream );
			switch ( channelState ) {
				case BASS_ACTIVE_PLAYING: {
					state = State::Playing;
					break;
				}
				case BASS_ACTIVE_STALLED: {
					if ( m_OutputStreamFinished && !m_MixerStreamHasEndSync ) {
						Stop();
					} else {
						state = State::Playing;
					}
					break;
				}
				case BASS_ACTIVE_PAUSED: {
					state = State::Paused;
					break;
				}
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive: {
			if ( -1 != BASS_WASAPI_GetDevice() ) {
				if ( BASS_WASAPI_IsStarted() ) {
					state = m_WASAPIPaused ? State::Paused : State::Playing;
				} else {
					BASS_WASAPI_SetNotify( nullptr, nullptr );
					BASS_WASAPI_Free();
				}
			}
			break;
		}
		case Settings::OutputMode::ASIO: {
			if ( -1 != BASS_ASIO_GetDevice() ) {
				if ( BASS_ASIO_IsStarted() ) {
					if ( BASS_ASIO_ACTIVE_PAUSED == BASS_ASIO_ChannelIsActive( FALSE /*input*/, 0 /*channel*/ ) ) {
						state = State::Paused;
					} else {
						const DWORD channelState = BASS_ChannelIsActive( m_OutputStream );
						if ( BASS_ACTIVE_PLAYING == channelState ) {
							state = State::Playing;
						}
					}
				}
			}
			break;
		}
	}
	return state;
}

Output::Item Output::GetCurrentPlaying()
{
	Item currentItem = {};
	const State state = GetState();
	if ( State::Stopped != state ) {
		const float seconds = GetOutputPosition();
		const Queue queue = GetOutputQueue();
		for ( auto iter = queue.rbegin(); iter != queue.rend(); iter++ ) {
			const Item& item = *iter;
			if ( item.Position <= seconds ) {
				currentItem.PlaylistItem = item.PlaylistItem;
				currentItem.Position = seconds - item.Position + item.InitialSeek;
				break;
			}
		}
		const auto streamTitleQueue = GetStreamTitleQueue();
		for ( auto iter = streamTitleQueue.rbegin(); iter != streamTitleQueue.rend(); iter++ ) {
			const auto& [titlePosition, title] = *iter;
			if ( titlePosition <= seconds ) {
				currentItem.StreamTitle = title;
				break;
			}
		}
	}
	return currentItem;
}

DWORD Output::ReadSampleData( float* buffer, const DWORD byteCount, HSTREAM handle )
{
	// Read sample data into the output buffer.
	DWORD bytesRead = 0;
	if ( ( nullptr != buffer ) && ( byteCount > 0 ) && m_DecoderStream ) {
		const long channels = m_DecoderStream->GetOutputChannels();
		if ( channels > 0 ) {
			long samplesToRead = static_cast<long>( byteCount ) / ( channels * 4 );

			if ( GetCrossfade() && !GetFadeOut() && !GetFadeToNext() && !GetStopAtTrackEnd() ) {
				const double crossfadePosition = GetCrossfadePosition();
				if ( crossfadePosition > 0 ) {
					bool checkCrossFade = ( GetRandomPlay() || GetRepeatTrack() );
					if ( !checkCrossFade ) {
						if ( GetFollowTrackSelection() ) {
							checkCrossFade = GetTrackToFollow( m_CurrentItemDecoding ).has_value();
						} else {
							std::lock_guard<std::mutex> lock( m_PlaylistMutex );
							Playlist::Item nextItem = {};
							checkCrossFade = m_Playlist->GetNextItem( m_CurrentItemDecoding, nextItem, GetRepeatPlaylist() /*wrap*/ );
						}
					}
					if ( checkCrossFade ) {
						// Ensure we don't read past the crossfade point.
						const long sampleRate = m_DecoderStream->GetOutputSampleRate();
						if ( sampleRate > 0 ) {
							const double trackPos = GetDecodePosition() - m_LastTransitionPosition - m_LeadInSeconds;
							const double secondsTillCrossfade = crossfadePosition - trackPos;
							const long samplesTillCrossfade = static_cast<long>( secondsTillCrossfade * sampleRate );
							if ( samplesTillCrossfade < samplesToRead ) {
								samplesToRead = samplesTillCrossfade;
								if ( samplesToRead <= 0 ) {
									samplesToRead = 0;
									// Hold on the the decoder, and indicate its fade out position.
									std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
									m_CrossfadingStream = m_DecoderStream;
									m_CurrentItemCrossfading = m_CurrentItemDecoding;
									m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
									m_SoftClipStateCrossfading = m_SoftClipStateDecoding;
								}
							}
						}
					}
				}
			} else if ( GetFadeToNext() && m_SwitchToNext ) {
				ToggleFadeToNext();
				samplesToRead = 0;
				std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
				m_CrossfadingStream = m_DecoderStream;
				m_CurrentItemCrossfading = m_CurrentItemDecoding;
				m_CurrentItemCrossfading.ID = s_ItemIsFadingToNext;
				m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
				m_SoftClipStateCrossfading = m_SoftClipStateDecoding;
			}

			bytesRead = static_cast<DWORD>( m_DecoderStream->Read( buffer, samplesToRead ) * channels * 4 );
		}

		if ( m_DecoderStream->SupportsStreamTitles() ) {
			auto streamTitleQueue = GetStreamTitleQueue();
			const auto [seconds, displayTitle] = m_DecoderStream->GetStreamTitle();
			if ( m_StreamTitleQueue.empty() || ( seconds != m_StreamTitleQueue.back().first ) ) {
				streamTitleQueue.push_back( { seconds, displayTitle } );
				SetStreamTitleQueue( streamTitleQueue );
			}
		}
	}

	// Check if we need to switch to the next decoder stream.
	if ( 0 == bytesRead ) {
		SetCrossfadePosition( 0 );
		m_LastTransitionPosition = 0;

		if ( ( GetStopAtTrackEnd() && ( m_CrossfadingItemID != s_ItemIsFadingToNext ) ) || GetFadeOut() ) {
			// Set a sync on the output stream, so that the states can be toggled when playback actually finishes.
			m_RestartItemID = {};
			SetEndSync( handle );
		} else if ( m_DecoderStream && ( 0 != BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE ) ) && !IsURL( m_CurrentItemDecoding.Info.GetFilename() ) ) {
			// Create a stream from the next playlist item, but not if there has been an error starting playback, or if the previous or next stream is a URL.
			Playlist::Item nextItem = m_CurrentItemDecoding;
			auto nextDecoder = GetNextDecoder( nextItem );
			if ( nextDecoder && !IsURL( nextItem.Info.GetFilename() ) ) {
				EstimateGain( nextItem );
				const long channels = m_DecoderStream->GetOutputChannels();
				const long sampleRate = m_DecoderStream->GetOutputSampleRate();
				if ( ( nextDecoder->GetOutputChannels() == channels ) && ( nextDecoder->GetOutputSampleRate() == sampleRate ) ) {
					if ( GetCrossfade() || GetFadeToNext() ) {
						nextDecoder->SkipSilence();
					}

					const long sampleCount = static_cast<long>( byteCount ) / ( channels * 4 );
					bytesRead = static_cast<DWORD>( nextDecoder->Read( buffer, sampleCount ) * channels * 4 );
					if ( bytesRead > 0 ) {
						m_LastTransitionPosition = GetDecodePosition() - m_LeadInSeconds;
						Queue queue = GetOutputQueue();
						queue.push_back( { nextItem, m_LastTransitionPosition } );
						SetOutputQueue( queue );

						if ( GetCrossfade() && ( 0 != bytesRead ) ) {
							StartCrossfadeCalculationThread( nextItem );
						}
					} else {
						nextItem = {};
						nextDecoder.reset();
					}
				}
			}

			m_DecoderStream = nextDecoder;
			m_CurrentItemDecoding = nextItem;

			if ( ( 0 == bytesRead ) && ( nextItem.ID > 0 ) ) {
				// Signal that playback should be restarted from the next playlist item.
				m_RestartItemID = nextItem.ID;
				SetEndSync( handle );

				std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
				if ( m_CrossfadingStream ) {
					m_CrossfadingStream.reset();
					m_CurrentItemCrossfading = {};
					m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
					m_SoftClipStateCrossfading.clear();
				}
			}
		}
	}

	if ( 0 != bytesRead ) {
		const long currentDecodingChannels = OutputDecoder::GetOutputChannels( m_CurrentItemDecoding.Info );
		if ( currentDecodingChannels > 0 ) {
			ApplyGain( buffer, static_cast<long>( bytesRead / ( currentDecodingChannels * 4 ) ), m_CurrentItemDecoding, m_SoftClipStateDecoding );
		}

		std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
		if ( m_CrossfadingStream ) {
			// Decode and fade out the crossfading stream and mix with the final output buffer.
			const long channels = m_Resample ? m_Resample->second : OutputDecoder::GetOutputChannels( m_CurrentItemCrossfading.Info );
			const long samplerate = m_Resample ? m_Resample->first : m_CurrentItemCrossfading.Info.GetSampleRate();
			if ( ( channels > 0 ) && ( samplerate > 0 ) ) {
				const long samplesToRead = static_cast<long>( bytesRead ) / ( channels * 4 );
				std::vector<float> crossfadingBuffer( bytesRead / 4 );
				const long crossfadingBytesRead = m_CrossfadingStream->Read( crossfadingBuffer.data(), samplesToRead ) * channels * 4;
				ApplyGain( crossfadingBuffer.data(), crossfadingBytesRead / ( channels * 4 ), m_CurrentItemCrossfading, m_SoftClipStateCrossfading );
				if ( crossfadingBytesRead <= static_cast<long>( bytesRead ) ) {
					long crossfadingSamplesRead = crossfadingBytesRead / ( channels * 4 );

					if ( s_ItemIsFadingToNext == m_CurrentItemCrossfading.ID ) {
						// Fade to next track.
						const float currentPos = GetDecodePosition();
						if ( ( currentPos > m_FadeOutStartPosition ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
							if ( ( currentPos - m_FadeOutStartPosition ) > GetFadeOutDuration() ) {
								crossfadingSamplesRead = 0;
							} else {
								const float fadeOutEndPosition = m_FadeOutStartPosition + GetFadeOutDuration();
								for ( long sampleIndex = 0; sampleIndex < crossfadingSamplesRead; sampleIndex++ ) {
									const float pos = static_cast<float>( sampleIndex ) / samplerate;
									float scale = static_cast<float>( fadeOutEndPosition - currentPos - pos ) / GetFadeOutDuration();
									if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
										scale = 0;
									}
									for ( long channel = 0; channel < channels; channel++ ) {
										crossfadingBuffer[ sampleIndex * channels + channel ] *= scale;
									}
								}
							}
						}
					} else {
						// Crossfade.
						const float trackPos = GetDecodePosition() - m_LastTransitionPosition - m_LeadInSeconds;
						if ( ( crossfadingBytesRead > 0 ) && ( trackPos < GetFadeOutDuration() ) ) {
							for ( long sampleIndex = 0; sampleIndex < crossfadingSamplesRead; sampleIndex++ ) {
								const float pos = static_cast<float>( sampleIndex ) / samplerate;
								float scale = static_cast<float>( GetFadeOutDuration() - trackPos - pos ) / GetFadeOutDuration();
								if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
									scale = 0;
								}
								for ( long channel = 0; channel < channels; channel++ ) {
									crossfadingBuffer[ sampleIndex * channels + channel ] *= scale;
								}
							}
						} else {
							crossfadingSamplesRead = 0;
						}
					}

					if ( 0 == crossfadingSamplesRead ) {
						m_CrossfadingStream.reset();
						m_CurrentItemCrossfading = {};
						m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
						m_SoftClipStateCrossfading.clear();
					} else {
						for ( long sample = 0; sample < crossfadingSamplesRead * channels; sample++ ) {
							buffer[ sample ] += crossfadingBuffer[ sample ];
						}
					}
				}
			}
		}
	}

	// Apply fade out on the currently decoding track, if necessary.
	if ( ( GetFadeOut() || GetFadeToNext() ) && ( 0 != bytesRead ) && ( m_FadeOutStartPosition > 0 ) && m_DecoderStream ) {
		const float currentPos = GetDecodePosition();
		const long channels = m_DecoderStream->GetOutputChannels();
		const long samplerate = m_DecoderStream->GetOutputSampleRate();
		if ( ( currentPos > m_FadeOutStartPosition ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
			if ( ( currentPos - m_FadeOutStartPosition ) > GetFadeOutDuration() ) {
				bytesRead = 0;
				// Set a sync on the output stream, so that the 'fade out' state can be toggled when playback actually finishes.
				m_RestartItemID = {};
				SetEndSync( handle );
			} else {
				const long sampleCount = static_cast<long>( bytesRead ) / ( channels * 4 );
				const float fadeOutEndPosition = m_FadeOutStartPosition + GetFadeOutDuration();
				for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
					const float pos = static_cast<float>( sampleIndex ) / samplerate;
					float scale = static_cast<float>( fadeOutEndPosition - currentPos - pos ) / GetFadeOutDuration();
					if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
						scale = 0;
					}
					for ( long channel = 0; channel < channels; channel++ ) {
						buffer[ sampleIndex * channels + channel ] *= scale;
					}
				}

				if ( GetFadeToNext() && ( currentPos > ( m_FadeOutStartPosition + GetFadeToNextDuration() ) ) ) {
					m_SwitchToNext = true;
				}
			}
		}
	}

	return bytesRead;
}

float Output::GetVolume() const
{
	return m_Volume;
}

void Output::SetVolume( const float volume )
{
	if ( volume != m_Volume ) {
		m_Volume = std::clamp( volume, 0.0f, 1.0f );
		if ( m_Muted ) {
			m_Muted = false;
		}
		UpdateOutputVolume();
	}
}

float Output::GetPitch() const
{
	return m_Pitch;
}

void Output::SetPitch( const float pitch )
{
	float pitchValue = pitch;
	const float pitchRange = GetPitchRange();
	if ( pitchValue < ( 1.0f - pitchRange ) ) {
		pitchValue = 1.0f - pitchRange;
	} else if ( pitchValue > ( 1.0f + pitchRange ) ) {
		pitchValue = 1.0f + pitchRange;
	}
	if ( pitchValue != m_Pitch ) {
		m_Pitch = pitchValue;
		const long sampleRate = m_DecoderSampleRate;
		if ( ( 0 != sampleRate ) && ( 0 != m_OutputStream ) ) {
			BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_FREQ, sampleRate * m_Pitch );
		}
	}
}

float Output::GetBalance() const
{
	return m_Balance;
}

void Output::SetBalance( const float balance )
{
	const float clampedBalance = std::clamp( balance, -1.0f, 1.0f );
	if ( clampedBalance != m_Balance ) {
		m_Balance = clampedBalance;
		BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_PAN, m_Balance );
	}
}

void Output::GetLevels( float& left, float& right )
{
	left = right = 0;
	if ( 0 != m_OutputStream ) {
		float levels[ 2 ] = {};
		const float length = 0.05f;
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard: {
				if ( TRUE == BASS_ChannelGetLevelEx( m_OutputStream, levels, length, BASS_LEVEL_STEREO ) ) {
					left = levels[ 0 ];
					right = levels[ 1 ];
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive: {
				if ( TRUE == BASS_WASAPI_GetLevelEx( levels, length, BASS_LEVEL_STEREO ) ) {
					left = levels[ 0 ];
					right = levels[ 1 ];
				}
				break;
			}
			case Settings::OutputMode::ASIO: {
				if ( TRUE == BASS_Mixer_ChannelGetLevelEx( m_OutputStream, levels, length, BASS_LEVEL_STEREO ) ) {
					left = levels[ 0 ];
					right = levels[ 1 ];
				}
				break;
			}
		}
	}
}

void Output::GetSampleData( const long sampleCount, std::vector<float>& samples, long& channels )
{
	if ( 0 != m_OutputStream ) {
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard:
			case Settings::OutputMode::ASIO: {
				BASS_CHANNELINFO channelInfo = {};
				if ( BASS_ChannelGetInfo( m_OutputStream, &channelInfo ) ) {
					channels = static_cast<long>( channelInfo.chans );
					const long sampleSize = channels * sampleCount;
					if ( sampleSize > 0 ) {
						samples.resize( sampleSize );
						if ( Settings::OutputMode::Standard == m_OutputMode ) {
							BASS_ChannelGetData( m_OutputStream, samples.data(), sampleSize * sizeof( float ) );
						} else {
							BASS_Mixer_ChannelGetData( m_OutputStream, samples.data(), sampleSize * sizeof( float ) );
						}
					}
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive: {
				if ( BASS_WASAPI_IsStarted() ) {
					BASS_WASAPI_INFO wasapiInfo = {};
					if ( TRUE == BASS_WASAPI_GetInfo( &wasapiInfo ) ) {
						channels = static_cast<long>( wasapiInfo.chans );
						const long sampleSize = channels * sampleCount;
						if ( sampleSize > 0 ) {
							samples.resize( sampleSize );
							BASS_WASAPI_GetData( samples.data(), sampleSize * sizeof( float ) );
						}
					}
				}
				break;
			}
		}
	}
}

void Output::GetFFTData( std::vector<float>& fft )
{
	constexpr std::pair fftSize{ BASS_DATA_FFT4096, 2048 };

	if ( 0 != m_OutputStream ) {
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard: {
				fft.resize( fftSize.second );
				if ( -1 == BASS_ChannelGetData( m_OutputStream, fft.data(), BASS_DATA_FLOAT | fftSize.first ) ) {
					fft.clear();
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive: {
				if ( BASS_WASAPI_IsStarted() ) {
					fft.resize( fftSize.second );
					if ( -1 == BASS_WASAPI_GetData( fft.data(), BASS_DATA_FLOAT | fftSize.first ) ) {
						fft.clear();
					}
				}
				break;
			}
			case Settings::OutputMode::ASIO: {
				fft.resize( fftSize.second );
				if ( -1 == BASS_Mixer_ChannelGetData( m_OutputStream, fft.data(), BASS_DATA_FLOAT | fftSize.first ) ) {
					fft.clear();
				}
				break;
			}
		}
	}
}

void Output::OnSyncEnd()
{
	if ( m_RestartItemID > 0 ) {
		PostMessage( m_Parent, MSG_RESTARTPLAYBACK, m_RestartItemID, NULL /*lParam*/ );
	} else if ( GetStopAtTrackEnd() || GetFadeOut() ) {
		if ( GetStopAtTrackEnd() && !m_RetainStopAtTrackEnd ) {
			ToggleStopAtTrackEnd();
		}
		if ( GetFadeOut() ) {
			ToggleFadeOut();
		}
		m_RestartItemID = 0;
		m_MixerStreamHasEndSync = false;
	}
}

bool Output::GetRandomPlay() const
{
	return m_RandomPlay;
}

void Output::SetRandomPlay( const bool enabled )
{
	if ( enabled != m_RandomPlay ) {
		m_RandomPlay = enabled;
		if ( m_RandomPlay ) {
			m_RepeatTrack = m_RepeatPlaylist = false;
		}
		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		m_PreloadedDecoder = {};
	}
}

bool Output::GetRepeatTrack() const
{
	return m_RepeatTrack;
}

void Output::SetRepeatTrack( const bool enabled )
{
	m_RepeatTrack = enabled;
	if ( m_RepeatTrack ) {
		m_RandomPlay = m_RepeatPlaylist = false;
	}
}

bool Output::GetRepeatPlaylist() const
{
	return m_RepeatPlaylist;
}

void Output::SetRepeatPlaylist( const bool enabled )
{
	m_RepeatPlaylist = enabled;
	if ( m_RepeatPlaylist ) {
		m_RandomPlay = m_RepeatTrack = false;
	}
}

bool Output::GetCrossfade() const
{
	return m_Crossfade;
}

void Output::SetCrossfade( const bool enabled )
{
	m_Crossfade = enabled;
	if ( m_Crossfade ) {
		if ( GetState() != State::Stopped ) {
			const Queue queue = GetOutputQueue();
			auto iter = queue.rbegin();
			if ( queue.rend() != iter ) {
				const Item item = *iter;
				StartCrossfadeCalculationThread( item.PlaylistItem, item.InitialSeek );
			}
		}
	} else {
		StopCrossfadeCalculationThread();
	}
}

bool Output::GetLoudnessNormalisation() const
{
	return m_LoudnessNormalisation;
}

void Output::SetLoudnessNormalisation( const bool enabled )
{
	m_LoudnessNormalisation = enabled;
	if ( m_LoudnessNormalisation ) {
		EstimateGain( m_CurrentItemDecoding );
		if ( State::Stopped != GetState() ) {
			StartLoudnessPrecalcThread();
		}
	}
}

bool Output::OnUpdatedMedia( const MediaInfo& mediaInfo )
{
	std::lock_guard<std::mutex> lock( m_PlaylistMutex );

	bool changed = false;
	State state = GetState();
	if ( State::Stopped == state ) {
		if ( std::tie( m_CurrentSelectedPlaylistItem.Info.GetFilename(), m_CurrentSelectedPlaylistItem.Info.GetCueStart(), m_CurrentSelectedPlaylistItem.Info.GetCueEnd() ) == std::tie( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() ) ) {
			m_CurrentSelectedPlaylistItem.Info = mediaInfo;
			changed = true;
		}
	} else {
		const Item currentPlaying = GetCurrentPlaying();
		Queue queue = GetOutputQueue();
		for ( auto& iter : queue ) {
			if ( std::tie( iter.PlaylistItem.Info.GetFilename(), iter.PlaylistItem.Info.GetCueStart(), iter.PlaylistItem.Info.GetCueEnd() ) == std::tie( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() ) ) {
				iter.PlaylistItem.Info = mediaInfo;
				if ( currentPlaying.PlaylistItem.ID == iter.PlaylistItem.ID ) {
					changed = true;
				}
			}
		}
		if ( changed ) {
			SetOutputQueue( queue );
		}
	}
	return changed;
}

Playlist::Item Output::GetCurrentSelectedPlaylistItem()
{
	return m_CurrentSelectedPlaylistItem;
}

void Output::SetCurrentSelectedPlaylistItem( const Playlist::Item& item )
{
	m_CurrentSelectedPlaylistItem = item;
}

std::set<std::wstring> Output::GetAllSupportedFileExtensions() const
{
	const std::set<std::wstring> fileExtensions = m_Handlers.GetAllSupportedFileExtensions();
	return fileExtensions;
}

Output::Devices Output::GetDevices( const Settings::OutputMode mode ) const
{
	Devices devices;
	DWORD deviceID = 0;
	switch ( mode ) {
		case Settings::OutputMode::Standard: {
			BASS_DEVICEINFO deviceInfo = {};
			while ( TRUE == BASS_GetDeviceInfo( ++deviceID, &deviceInfo ) ) {
				if ( nullptr != deviceInfo.name ) {
					devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
				}
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive: {
			BASS_WASAPI_DEVICEINFO deviceInfo = {};
			while ( TRUE == BASS_WASAPI_GetDeviceInfo( deviceID, &deviceInfo ) ) {
				if ( ( deviceInfo.flags & BASS_DEVICE_ENABLED ) && !( deviceInfo.flags & BASS_DEVICE_INPUT ) && ( nullptr != deviceInfo.name ) ) {
					devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
				}
				++deviceID;
			}
			break;
		}
		case Settings::OutputMode::ASIO: {
			BASS_ASIO_DEVICEINFO deviceInfo = {};
			while ( TRUE == BASS_ASIO_GetDeviceInfo( deviceID, &deviceInfo ) ) {
				if ( nullptr != deviceInfo.name ) {
					devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
				}
				++deviceID;
			}
			break;
		}
	}
	if ( !devices.empty() ) {
		devices.insert( Devices::value_type( -1, std::wstring() ) );
	}
	return devices;
}

void Output::SettingsChanged()
{
	std::wstring settingsDevice;
	Settings::OutputMode settingsMode = Settings::OutputMode::Standard;
	m_Settings.GetOutputSettings( settingsDevice, settingsMode );

	uint32_t resamplerRate = 0;
	uint32_t resamplerChannels = 0;
	const bool resamplerEnabled = m_Settings.GetResamplerSettings( resamplerRate, resamplerChannels );

	const bool reinitialise =
		( settingsMode != m_OutputMode ) ||
		( settingsDevice != m_OutputDevice ) ||
		( resamplerEnabled != m_Resample.has_value() ) ||
		( resamplerRate != m_Resample.value_or( std::make_pair( resamplerRate, resamplerChannels ) ).first ) ||
		( resamplerChannels != m_Resample.value_or( std::make_pair( resamplerRate, resamplerChannels ) ).second );

	if ( reinitialise ) {
		Stop();
		if ( -1 != BASS_ASIO_GetDevice() ) {
			BASS_ASIO_Free();
		}
		BASS_Free();
		InitialiseOutput();
	}

	GetWASAPISettings();
	GetASIOSettings();

	Settings::GainMode gainMode = Settings::GainMode::Disabled;
	Settings::LimitMode limitMode = Settings::LimitMode::None;
	float gainPreamp = 0;
	m_Settings.GetGainSettings( gainMode, limitMode, gainPreamp );
	if ( ( gainMode != m_GainMode ) || ( limitMode != m_LimitMode ) || ( gainPreamp != m_GainPreamp ) ) {
		m_GainMode = gainMode;
		m_LimitMode = limitMode;
		m_GainPreamp = gainPreamp;
	}

	m_RetainStopAtTrackEnd = m_Settings.GetRetainStopAtTrackEnd();

	m_Handlers.SettingsChanged( m_Settings );
}

void Output::InitialiseOutput()
{
	int deviceNum = -1;
	m_Settings.GetOutputSettings( m_OutputDevice, m_OutputMode );

	if ( ( Settings::OutputMode::ASIO == m_OutputMode ) && !InitASIO() ) {
		m_OutputDevice.clear();
		m_OutputMode = Settings::OutputMode::Standard;
	}

	if ( Settings::OutputMode::Standard != m_OutputMode ) {
		// Use no sound device.
		deviceNum = 0;
	} else {
		if ( !m_OutputDevice.empty() ) {
			const Devices devices = GetDevices( m_OutputMode );
			for ( const auto& device : devices ) {
				if ( device.second == m_OutputDevice ) {
					deviceNum = device.first;
					break;
				}
			}
			if ( -1 == deviceNum ) {
				m_OutputDevice.clear();
			}
		}
	}

	BOOL success = BASS_Init( deviceNum, 48000 /*freq*/, 0 /*flags*/, m_Parent /*hwnd*/, NULL /*dsGUID*/ );
	if ( !success ) {
		if ( -1 != deviceNum ) {
			// Use default device.
			deviceNum = -1;
			success = BASS_Init( deviceNum, 48000 /*freq*/, 0 /*flags*/, m_Parent /*hwnd*/, NULL /*dsGUID*/ );
		}
		if ( !success ) {
			// Use no sound device.
			deviceNum = 0;
			success = BASS_Init( deviceNum, 48000 /*freq*/, 0 /*flags*/, m_Parent /*hwnd*/, NULL /*dsGUID*/ );
		}
	}

	if ( success ) {
		const int bufSize = 32;
		WCHAR buffer[ bufSize ] = {};
		LoadString( m_hInst, IDS_USERAGENT, buffer, bufSize );
		const std::wstring userAgent( buffer );
		BASS_SetConfigPtr( BASS_CONFIG_NET_AGENT, userAgent.c_str() );
	}

	uint32_t resamplerRate = 0;
	uint32_t resamplerChannels = 0;
	const bool resamplerEnabled = m_Settings.GetResamplerSettings( resamplerRate, resamplerChannels );
	m_Resample = resamplerEnabled ? std::make_optional( std::make_pair( resamplerRate, resamplerChannels ) ) : std::nullopt;

	GetWASAPISettings();
	GetASIOSettings();
}

void Output::GetWASAPISettings()
{
	bool useMixFormat = false;
	int bufferMilliseconds = 0;
	int leadInMilliseconds = 0;
	m_Settings.GetAdvancedWasapiExclusiveSettings( useMixFormat, bufferMilliseconds, leadInMilliseconds );
	m_WASAPIUseMixFormat.store( useMixFormat );
	m_WASAPIBufferMilliseconds.store( bufferMilliseconds );
	m_WASAPILeadInMilliseconds.store( leadInMilliseconds );
}

void Output::GetASIOSettings()
{
	bool useDefaultSamplerate = false;
	int defaultSamplerate = 0;
	int leadInMilliseconds = 0;
	m_Settings.GetAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadInMilliseconds );
	m_ASIOUseDefaultSamplerate.store( useDefaultSamplerate );
	m_ASIODefaultSamplerate.store( defaultSamplerate );
	m_ASIOLeadInMilliseconds.store( leadInMilliseconds );
}

void Output::EstimateGain( Playlist::Item& item )
{
	if ( m_LoudnessNormalisation && !IsURL( item.Info.GetFilename() ) ) {
		auto gain = item.Info.GetGainAlbum();
		if ( !gain.has_value() || ( Settings::GainMode::Album != m_GainMode ) ) {
			if ( const auto trackGain = item.Info.GetGainTrack(); trackGain.has_value() ) {
				gain = trackGain;
			}
		}
		if ( !gain.has_value() ) {
			const auto estimateIter = m_GainEstimateMap.find( item.ID );
			if ( m_GainEstimateMap.end() != estimateIter ) {
				item.Info.SetGainTrack( estimateIter->second );
			} else {
				const auto tempDecoder = OpenDecoder( item, Decoder::Context::Temporary );
				if ( tempDecoder ) {
					const auto trackGain = tempDecoder->CalculateTrackGain( [] () { return true; }, s_GainPrecalcTime );
					item.Info.SetGainTrack( trackGain );
					m_GainEstimateMap.insert( GainEstimateMap::value_type( item.ID, trackGain ) );
				}
			}
		}
	}
}

void Output::StartCrossfadeCalculationThread( const Playlist::Item& item, const double seekOffset )
{
	StopCrossfadeCalculationThread();
	ResetEvent( m_CrossfadeStopEvent );
	m_CrossfadeItem = item;
	m_CrossfadeSeekOffset = seekOffset;
	m_CrossfadeThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, CrossfadeThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
}

void Output::CalculateCrossfadeHandler()
{
	if ( !CanBackgroundThreadProceed( m_CrossfadeStopEvent ) ) {
		return;
	}

	const auto decoder = IsURL( m_CrossfadeItem.Info.GetFilename() ) ? nullptr : OpenDecoder( m_CrossfadeItem, Decoder::Context::Input );
	if ( decoder ) {
		const float duration = decoder->GetDuration();
		const long channels = decoder->GetChannels();
		const long samplerate = decoder->GetSampleRate();
		if ( ( duration > 0 ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
			const double crossfadePosition = CalculateCrossfadePosition( decoder, m_CrossfadeSeekOffset, [ this ] () {
				return WAIT_OBJECT_0 != WaitForSingleObject( m_CrossfadeStopEvent, 0 );
				} );
			if ( WAIT_OBJECT_0 != WaitForSingleObject( m_CrossfadeStopEvent, 0 ) ) {
				SetCrossfadePosition( crossfadePosition - m_CrossfadeSeekOffset );

				Playlist::Item nextItem = {};
				{
					std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
					nextItem = m_PreloadedDecoder.item;
				}
				if ( MediaInfo::Source::CDDA == nextItem.Info.GetSource() ) {
					// Pre-cache some CD audio data for the next track, to prevent glitches when crossfading.
					if ( const auto nextDecoder = OpenDecoder( nextItem, Decoder::Context::Output ); nextDecoder ) {
						const long bufferSize = nextDecoder->GetSampleRate() / 10;
						std::vector<float> buffer( bufferSize * channels );
						const long kSamplesToRead = 10 * nextDecoder->GetSampleRate();
						long totalSamplesRead = 0;
						while ( WAIT_OBJECT_0 != WaitForSingleObject( m_CrossfadeStopEvent, 0 ) ) {
							const long samplesRead = nextDecoder->ReadSamples( buffer.data(), bufferSize );
							totalSamplesRead += samplesRead;
							if ( ( totalSamplesRead >= kSamplesToRead ) || ( samplesRead <= 0 ) ) {
								break;
							}
						}
					}
				}
			}
		}
	}
}

void Output::StopCrossfadeCalculationThread()
{
	if ( nullptr != m_CrossfadeThread ) {
		SetEvent( m_CrossfadeStopEvent );
		WaitForSingleObject( m_CrossfadeThread, INFINITE );
		CloseHandle( m_CrossfadeThread );
		m_CrossfadeThread = nullptr;
	}
	SetCrossfadePosition( 0 );
	m_CrossfadeSeekOffset = 0;
	m_CrossfadeItem = {};
}

double Output::CalculateCrossfadePosition( Decoder::Ptr decoder, const double seekOffset, Decoder::CanContinue canContinue ) const
{
	if ( !decoder )
		return 0;

	if ( 0.0f == seekOffset ) {
		decoder->SkipSilence();
	}

	double position = 0;
	if ( seekOffset > 0 ) {
		position = decoder->SetPosition( seekOffset );
	}
	double crossfadePosition = 0;

	int64_t cumulativeCount = 0;
	double cumulativeTotal = 0;
	double cumulativeRMS = 0;

	const double crossfadeRMSRatio = s_CrossfadeVolume;
	const long windowSize = decoder->GetSampleRate() / 10;
	std::vector<float> buffer( windowSize * decoder->GetChannels() );

	while ( ( nullptr == canContinue ) || canContinue() ) {
		long sampleCount = decoder->ReadSamples( buffer.data(), windowSize );
		if ( sampleCount > 0 ) {

			auto sampleIter = buffer.begin();
			double windowTotal = 0;
			for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
				for ( long channel = 0; channel < decoder->GetChannels(); channel++, sampleIter++, cumulativeCount++ ) {
					const double value = *sampleIter * *sampleIter;
					windowTotal += value;
					cumulativeTotal += value;
				}
			}

			const double windowRMS = sqrt( windowTotal / ( sampleCount * decoder->GetChannels() ) );
			cumulativeRMS = sqrt( cumulativeTotal / cumulativeCount );
			position += static_cast<double>( sampleCount ) / decoder->GetSampleRate();

			if ( windowRMS > cumulativeRMS ) {
				crossfadePosition = position;
			} else if ( ( cumulativeRMS > 0 ) && ( ( windowRMS / cumulativeRMS ) > crossfadeRMSRatio ) ) {
				crossfadePosition = position;
			}

		} else {
			break;
		}
	}
	return crossfadePosition;
}

double Output::GetCrossfadePosition() const
{
	return m_CrossfadePosition;
}

void Output::SetCrossfadePosition( const double position )
{
	m_CrossfadePosition = position;
}

void Output::ToggleStopAtTrackEnd()
{
	m_StopAtTrackEnd = !m_StopAtTrackEnd;
}

bool Output::GetStopAtTrackEnd() const
{
	return m_StopAtTrackEnd;
}

void Output::ToggleFollowTrackSelection()
{
	m_FollowTrackSelection = !m_FollowTrackSelection;
}

bool Output::GetFollowTrackSelection() const
{
	return m_FollowTrackSelection;
}

void Output::ToggleMuted()
{
	m_Muted = !m_Muted;
	UpdateOutputVolume();
}

bool Output::GetMuted() const
{
	return m_Muted;
}

void Output::ToggleFadeOut()
{
	m_FadeOut = !m_FadeOut;
	if ( m_FadeOut && ( 0 != m_OutputStream ) ) {
		m_FadeOutStartPosition = GetDecodePosition();
	} else {
		m_FadeOutStartPosition = 0;
	}
}

bool Output::GetFadeOut() const
{
	return m_FadeOut;
}

void Output::ToggleFadeToNext()
{
	m_FadeToNext = !m_FadeToNext;
	if ( m_FadeToNext && ( 0 != m_OutputStream ) ) {
		m_FadeOutStartPosition = GetDecodePosition();
	} else {
		m_SwitchToNext = false;
		std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
		if ( m_CrossfadingStream ) {
			m_CrossfadingStream.reset();
			m_CurrentItemCrossfading = {};
			m_CrossfadingItemID = m_CurrentItemCrossfading.ID;
			m_SoftClipStateCrossfading.clear();
		}
	}
}

bool Output::GetFadeToNext() const
{
	return m_FadeToNext;
}

void Output::ApplyGain( float* buffer, const long sampleCount, const Playlist::Item& item, std::vector<float>& softClipState )
{
	const bool eqEnabled = m_EQEnabled;
	const long channels = OutputDecoder::GetOutputChannels( item.Info );
	if ( ( 0 != sampleCount ) && ( channels > 0 ) && ( m_LoudnessNormalisation || eqEnabled ) ) {
		float preamp = eqEnabled ? m_EQPreamp : 0;

		if ( m_LoudnessNormalisation ) {
			auto gain = item.Info.GetGainAlbum();
			if ( !gain.has_value() || ( Settings::GainMode::Album != m_GainMode ) ) {
				if ( const auto trackGain = item.Info.GetGainTrack(); trackGain.has_value() ) {
					gain = trackGain;
				}
			}
			if ( gain.has_value() ) {
				if ( gain.value() < s_GainMin ) {
					gain = s_GainMin;
				} else if ( gain.value() > s_GainMax ) {
					gain = s_GainMax;
				}
				preamp += m_GainPreamp;
				preamp += gain.value();
			}
		}

		if ( 0 != preamp ) {
			const float scale = powf( 10.0f, preamp / 20.0f );
			const long totalSamples = sampleCount * channels;
			for ( long sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++ ) {
				buffer[ sampleIndex ] *= scale;
			}
			switch ( m_LimitMode ) {
				case Settings::LimitMode::Hard: {
					for ( long sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++ ) {
						if ( buffer[ sampleIndex ] < -1.0f ) {
							buffer[ sampleIndex ] = -1.0f;
						} else if ( buffer[ sampleIndex ] > 1.0f ) {
							buffer[ sampleIndex ] = 1.0f;
						}
					}
					break;
				}
				case Settings::LimitMode::Soft: {
					if ( softClipState.size() != static_cast<size_t>( channels ) ) {
						softClipState.resize( channels, 0 );
					}
					opus_pcm_soft_clip( buffer, sampleCount, channels, softClipState.data() );
					break;
				}
				default: {
					break;
				}
			}
		}
	}
}

Output::Queue Output::GetOutputQueue()
{
	std::lock_guard<std::mutex> lock( m_QueueMutex );
	return m_OutputQueue;
}

void Output::SetOutputQueue( const Queue& queue )
{
	std::lock_guard<std::mutex> lock( m_QueueMutex );
	m_OutputQueue = queue;
}

float Output::GetPitchRange() const
{
	const float range = m_Settings.GetPitchRangeOptions()[ m_Settings.GetPitchRange() ];
	return range;
}

void Output::UpdateEQ( const Settings::EQ& eq )
{
	m_EQEnabled = eq.Enabled;
	m_EQPreamp = eq.Preamp;

	m_CurrentEQ = eq;
	if ( 0 != m_OutputStream ) {
		if ( eq.Enabled ) {
			if ( m_FX.empty() ) {
				// Add FX to current output stream.
				BASS_DX8_PARAMEQ params = {};
				params.fBandwidth = eq.Bandwidth;
				for ( const auto& setting : eq.Gains ) {
					const int freq = setting.first;
					const HFX fx = BASS_ChannelSetFX( m_OutputStream, BASS_FX_DX8_PARAMEQ, 100 /*priority*/ );
					if ( 0 != fx ) {
						params.fCenter = static_cast<float>( freq );
						params.fGain = setting.second;
						BASS_FXSetParameters( fx, &params );

						BASS_FXGetParameters( fx, &params );
						if ( static_cast<int>( params.fCenter ) != freq ) {
							// The centre frequency was too high for the stream, so remove the FX.
							BASS_ChannelRemoveFX( m_OutputStream, fx );
						} else {
							m_FX.push_back( fx );
						}
					}
				}
			} else {
				// Modify FX on the current output stream.
				for ( const auto& fx : m_FX ) {
					BASS_DX8_PARAMEQ params = {};
					if ( BASS_FXGetParameters( fx, &params ) ) {
						const int freq = static_cast<int>( params.fCenter );
						const auto setting = eq.Gains.find( freq );
						if ( eq.Gains.end() != setting ) {
							const float gain = setting->second;
							if ( gain != params.fGain ) {
								params.fGain = gain;
								BASS_FXSetParameters( fx, &params );
							}
						}
					}
				}
			}
		} else {
			// Disable EQ.
			for ( const auto& fx : m_FX ) {
				BASS_ChannelRemoveFX( m_OutputStream, fx );
			}
			m_FX.clear();
		}
	}
}

Decoder::Ptr Output::OpenDecoder( Playlist::Item& item, const Decoder::Context context )
{
	constexpr bool applyCues = true;
	Decoder::Ptr decoder = m_Handlers.OpenDecoder( item.Info, context, applyCues );
	if ( !decoder ) {
		auto duplicate = item.Duplicates.begin();
		while ( !decoder && ( item.Duplicates.end() != duplicate ) ) {
			MediaInfo duplicateInfo( item.Info );
			duplicateInfo.SetFilename( *duplicate );
			decoder = m_Handlers.OpenDecoder( duplicateInfo, context );
			++duplicate;
		}
	}
	if ( decoder ) {
		m_Playlist->GetLibrary().UpdateMediaInfoFromDecoder( item.Info, *decoder );
	}
	return decoder;
}

Output::OutputDecoderPtr Output::OpenOutputDecoder( Playlist::Item& item, const bool usePreloadedDecoder )
{
	OutputDecoderPtr outputDecoder;
	if ( usePreloadedDecoder ) {
		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		const auto& info = m_PreloadedDecoder.item.Info;
		if ( m_PreloadedDecoder.decoder && ( std::tie( info.GetFilename(), info.GetCueStart(), info.GetCueEnd() ) == std::tie( item.Info.GetFilename(), item.Info.GetCueStart(), item.Info.GetCueEnd() ) ) && ( info.GetFiletime() == item.Info.GetFiletime() ) ) {
			outputDecoder = m_PreloadedDecoder.decoder;
			m_PreloadedDecoder.decoder.reset();
			m_PreloadedDecoder.item = {};
			if ( !IsURL( item.Info.GetFilename() ) ) {
				// Ensure pre-buffering has started (in case the pre-buffer finished callback was not received for the previous decoder).
				outputDecoder->PreBuffer( m_OnPreBufferFinishedCallback );
			}
		}
	}
	if ( !outputDecoder ) {
		try {
			std::optional<uint32_t> resamplerRate = m_Resample ? std::make_optional( m_Resample->first ) : std::nullopt;
			std::optional<uint32_t> resamplerChannels = m_Resample ? std::make_optional( m_Resample->second ) : std::nullopt;
			if ( resamplerRate && resamplerChannels )
				outputDecoder = std::make_shared<Resampler>( OpenDecoder( item, Decoder::Context::Output ), item.ID, *resamplerRate, *resamplerChannels );
			else
				outputDecoder = std::make_shared<OutputDecoder>( OpenDecoder( item, Decoder::Context::Output ), item.ID );
		} catch ( const std::runtime_error& ) {
		}
	}
	return outputDecoder;
}

Output::State Output::StartOutput()
{
	State state = State::Stopped;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard: {
			if ( BASS_ChannelPlay( m_OutputStream, TRUE /*restart*/ ) ) {
				state = State::Playing;
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive: {
			if ( BASS_WASAPI_Start() ) {
				BASS_WASAPI_INFO info = {};
				if ( ( TRUE == BASS_WASAPI_GetInfo( &info ) ) && ( info.freq > 0 ) && ( info.chans > 0 ) ) {
					const DWORD samplecount = info.buflen / info.chans / 4;
					float latency = static_cast<float>( samplecount ) / info.freq;
					if ( info.initflags & BASS_WASAPI_EVENT ) {
						latency *= 2;
					}
					BASS_ChannelSetAttribute( m_MixerStream, BASS_ATTRIB_MIXER_LATENCY, latency );
				}
				state = State::Playing;
			}
			break;
		}
		case Settings::OutputMode::ASIO: {
			if ( BASS_ASIO_Start( 0, 0 ) ) {
				BASS_CHANNELINFO channelInfo = {};
				if ( ( TRUE == BASS_ChannelGetInfo( m_MixerStream, &channelInfo ) ) && ( channelInfo.freq > 0 ) ) {
					const float latency = static_cast<float>( BASS_ASIO_GetLatency( FALSE /*input*/ ) ) / channelInfo.freq;
					BASS_ChannelSetAttribute( m_MixerStream, BASS_ATTRIB_MIXER_LATENCY, latency );
				}
				state = State::Playing;
			}
			break;
		}
	}
	return state;
}

float Output::GetDecodePosition() const
{
	const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
	const float seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) );
	return seconds;
}

float Output::GetOutputPosition() const
{
	if ( m_PausedOnStartup )
		return 0;

	float seconds = 0;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard: {
			const QWORD bytePos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_BYTE );
			seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) );
			break;
		}
		case Settings::OutputMode::WASAPIExclusive:
		case Settings::OutputMode::ASIO: {
			const QWORD bytePos = BASS_Mixer_ChannelGetPosition( m_OutputStream, BASS_POS_BYTE );
			seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) ) - m_LeadInSeconds;
			if ( seconds < 0 ) {
				seconds = 0;
			}
			break;
		}
	}
	return seconds;
}

LONGLONG Output::GetTick()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter( &count );
	return count.QuadPart;
}

float Output::GetInterval( const LONGLONG startTick, const LONGLONG endTick )
{
	LARGE_INTEGER tickFreq = {};
	QueryPerformanceFrequency( &tickFreq );
	return static_cast<float>( endTick - startTick ) / tickFreq.QuadPart;
}

bool Output::CreateOutputStream( const MediaInfo& mediaInfo )
{
	bool success = false;
	const DWORD samplerate = static_cast<DWORD>( m_Resample ? m_Resample->first : mediaInfo.GetSampleRate() );
	const DWORD channels = static_cast<DWORD>( m_Resample ? m_Resample->second : OutputDecoder::GetOutputChannels( mediaInfo ) );
	if ( ( samplerate > 0 ) && ( channels > 0 ) ) {
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard: {
				m_LeadInSeconds = 0;
				BASS_INFO bassInfo = {};
				const DWORD outputChannels = ( BASS_GetInfo( &bassInfo ) && ( bassInfo.speakers >= 2ul ) ) ? std::clamp( channels, 2ul, bassInfo.speakers ) : channels;
				if ( channels > outputChannels ) {
					DWORD flags = BASS_SAMPLE_FLOAT | BASS_MIXER_POSEX;
					m_OutputStream = BASS_Mixer_StreamCreate( samplerate, outputChannels, flags );
					success = ( 0 != m_OutputStream );
					if ( success ) {
						flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
						m_MixerStream = BASS_StreamCreate( samplerate, channels, flags, StreamProc, this );
						success = ( 0 != m_MixerStream );
						if ( success ) {
							flags = BASS_MIXER_CHAN_BUFFER;
							if ( channels > outputChannels ) {
								flags |= BASS_MIXER_CHAN_DOWNMIX;
							}
							success = ( 0 != BASS_Mixer_StreamAddChannel( m_OutputStream, m_MixerStream, flags ) );
						}
					}
					if ( !success ) {
						if ( 0 != m_OutputStream ) {
							BASS_StreamFree( m_OutputStream );
							m_OutputStream = 0;
						}
						if ( 0 != m_MixerStream ) {
							BASS_StreamFree( m_MixerStream );
							m_MixerStream = 0;
						}
					}
				} else {
					const DWORD flags = BASS_SAMPLE_FLOAT;
					m_OutputStream = BASS_StreamCreate( samplerate, channels, flags, StreamProc, this );
					success = ( 0 != m_OutputStream );
				}
				break;
			}

			case Settings::OutputMode::WASAPIExclusive: {
				int deviceNum = -1;
				if ( !m_OutputDevice.empty() ) {
					const Devices devices = GetDevices( m_OutputMode );
					for ( const auto& device : devices ) {
						if ( device.second == m_OutputDevice ) {
							deviceNum = device.first;
							break;
						}
					}
				}

				m_LeadInSeconds = static_cast<float>( m_WASAPILeadInMilliseconds.load() ) / 1000;
				bool useMixFormat = m_WASAPIUseMixFormat.load();
				if ( !useMixFormat ) {
					const DWORD format = BASS_WASAPI_CheckFormat( deviceNum, samplerate, channels, BASS_WASAPI_EXCLUSIVE );
					useMixFormat = ( -1 == format ) && ( BASS_ERROR_FORMAT == BASS_ErrorGetCode() );
				}
				DWORD outputSamplerate = useMixFormat ? 0 : samplerate;
				DWORD outputChannels = useMixFormat ? 0 : channels;

				DWORD flags = BASS_WASAPI_EXCLUSIVE | BASS_WASAPI_BUFFER | BASS_WASAPI_EVENT | BASS_WASAPI_ASYNC;
				const float period = 0;
				float buffer = static_cast<float>( m_WASAPIBufferMilliseconds.load() ) / 1000;
				if ( flags & BASS_WASAPI_EVENT ) {
					buffer /= 2;
				}

				if ( TRUE == BASS_WASAPI_Init( deviceNum, outputSamplerate, outputChannels, flags, buffer, period, WasapiProc, this ) ) {
					BASS_WASAPI_INFO wasapiInfo = {};
					success = ( TRUE == BASS_WASAPI_GetInfo( &wasapiInfo ) );
					if ( success ) {
						outputSamplerate = wasapiInfo.freq;
						outputChannels = wasapiInfo.chans;
						flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_POSEX;
						m_MixerStream = BASS_Mixer_StreamCreate( outputSamplerate, outputChannels, flags );
						success = ( 0 != m_MixerStream );
						if ( success ) {
							flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
							m_OutputStream = BASS_StreamCreate( samplerate, channels, flags, StreamProc, this );
							success = ( 0 != m_OutputStream );
							if ( success ) {
								flags = BASS_MIXER_CHAN_BUFFER;
								if ( channels > outputChannels ) {
									flags |= BASS_MIXER_CHAN_DOWNMIX;
								}
								success = ( 0 != BASS_Mixer_StreamAddChannel( m_MixerStream, m_OutputStream, flags ) );
							}
						}

						if ( !success ) {
							if ( 0 != m_OutputStream ) {
								BASS_StreamFree( m_OutputStream );
								m_OutputStream = 0;
							}
							if ( 0 != m_MixerStream ) {
								BASS_StreamFree( m_MixerStream );
								m_MixerStream = 0;
							}
						}
					}

					if ( success ) {
						BASS_WASAPI_SetNotify( WasapiNotifyProc, this );
					} else {
						BASS_WASAPI_Free();
					}
				}
				break;
			}

			case Settings::OutputMode::ASIO: {
				if ( InitASIO() ) {
					m_LeadInSeconds = static_cast<float>( m_ASIOLeadInMilliseconds.load() ) / 1000;
					const bool useDefaultSamplerate = m_ASIOUseDefaultSamplerate.load();
					const double defaultSamplerate = static_cast<double>( m_ASIODefaultSamplerate.load() );
					double outputSamplerate = useDefaultSamplerate ? defaultSamplerate : static_cast<double>( samplerate );
					if ( FALSE == BASS_ASIO_CheckRate( outputSamplerate ) ) {
						if ( useDefaultSamplerate ) {
							outputSamplerate = static_cast<double>( samplerate );
						} else {
							outputSamplerate = defaultSamplerate;
						}
						if ( FALSE == BASS_ASIO_CheckRate( outputSamplerate ) ) {
							outputSamplerate = 48000;
						}
					}

					BASS_ASIO_INFO asioInfo = {};
					success = ( TRUE == BASS_ASIO_GetInfo( &asioInfo ) );
					if ( success ) {
						const DWORD outputChannels = std::clamp( channels, 2ul, asioInfo.outputs );
						DWORD flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_POSEX;
						m_MixerStream = BASS_Mixer_StreamCreate( static_cast<DWORD>( outputSamplerate ), outputChannels, flags );
						success = ( 0 != m_MixerStream );
						if ( success ) {
							flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE;
							m_OutputStream = BASS_StreamCreate( samplerate, channels, flags, StreamProc, this );
							success = ( 0 != m_OutputStream );
							if ( success ) {
								flags = BASS_MIXER_CHAN_BUFFER;
								if ( channels > outputChannels ) {
									flags |= BASS_MIXER_CHAN_DOWNMIX;
								}
								success = ( 0 != BASS_Mixer_StreamAddChannel( m_MixerStream, m_OutputStream, flags ) );
								if ( success ) {
									success = ( TRUE == BASS_ASIO_SetRate( outputSamplerate ) );
									if ( success ) {
										success = ( TRUE == BASS_ASIO_ChannelEnableBASS( FALSE /*input*/, 0 /*channel*/, m_MixerStream, TRUE /*join*/ ) );
									}
								}
							}
						}

						if ( !success ) {
							if ( 0 != m_OutputStream ) {
								BASS_StreamFree( m_OutputStream );
								m_OutputStream = 0;
							}
							if ( 0 != m_MixerStream ) {
								BASS_StreamFree( m_MixerStream );
								m_MixerStream = 0;
							}
						}
					}

					if ( !success ) {
						BASS_ASIO_Free();
					}
				}
				break;
			}
		}
	}
	return success;
}

void Output::UpdateOutputVolume()
{
	if ( 0 != m_OutputStream ) {
		BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_VOL, m_Muted ? 0 : m_Volume );
	}
}

bool Output::InitASIO()
{
	if ( m_ResetASIO ) {
		if ( -1 != BASS_ASIO_GetDevice() ) {
			BASS_ASIO_Free();
		}
		m_ResetASIO = false;
	}

	if ( ( -1 == BASS_ASIO_GetDevice() ) && ( Settings::OutputMode::ASIO == m_OutputMode ) ) {
		int deviceNum = -1;
		if ( !m_OutputDevice.empty() ) {
			const Devices devices = GetDevices( m_OutputMode );
			for ( const auto& device : devices ) {
				if ( device.second == m_OutputDevice ) {
					deviceNum = device.first;
					break;
				}
			}
		}
		if ( TRUE == BASS_ASIO_Init( deviceNum, BASS_ASIO_THREAD ) ) {
			BASS_ASIO_SetNotify( AsioNotifyProc, this );
		}
	}

	const bool success = ( -1 != BASS_ASIO_GetDevice() );
	return success;
}

DWORD Output::ApplyLeadIn( float* buffer, const DWORD byteCount, HSTREAM handle ) const
{
	DWORD bytesPadded = 0;
	if ( m_LeadInSeconds > 0 ) {
		const QWORD bytePos = BASS_ChannelGetPosition( handle, BASS_POS_BYTE );
		const double position = BASS_ChannelBytes2Seconds( handle, bytePos );
		if ( position < m_LeadInSeconds ) {
			BASS_CHANNELINFO info = {};
			if ( ( TRUE == BASS_ChannelGetInfo( handle, &info ) ) && ( info.freq > 0 ) && ( info.chans > 0 ) ) {
				bytesPadded = static_cast<DWORD>( 0.5f + ( m_LeadInSeconds - position ) * info.freq ) * info.chans * 4;
				bytesPadded = std::min<DWORD>( bytesPadded, byteCount );
				if ( bytesPadded > 0 ) {
					std::fill( buffer, buffer + bytesPadded / 4, 0.0f );
				}
			}
		}
	}
	return bytesPadded;
}

float Output::GetFadeOutDuration() const
{
	return s_FadeOutDuration;
}

float Output::GetFadeToNextDuration() const
{
	return s_FadeToNextDuration;
}

void Output::LoudnessPrecalcHandler()
{
	if ( !CanBackgroundThreadProceed( m_LoudnessPrecalcStopEvent ) ) {
		return;
	}

	// Playlist might have new items, so check back every so often.
	constexpr DWORD interval = 30 /*sec*/ * 1000;

	Decoder::CanContinue canContinue( [ stopEvent = m_LoudnessPrecalcStopEvent ] ()
		{
			return ( WAIT_OBJECT_0 != WaitForSingleObject( stopEvent, 0 ) );
		} );

	do {
		Playlist::Items items;
		{
			std::lock_guard<std::mutex> lock( m_PlaylistMutex );
			items = m_Playlist->GetItems();
		}
		auto item = items.begin();
		while ( ( items.end() != item ) && canContinue() ) {
			// Only scan files on local (fixed) drives.
			bool isLocalFile = false;
			if ( const auto& filename = item->Info.GetFilename(); !filename.empty() ) {
				std::vector<wchar_t> volumePathName( 1 + filename.size() );
				if ( GetVolumePathName( filename.c_str(), volumePathName.data(), static_cast<DWORD>( volumePathName.size() ) ) ) {
					isLocalFile = ( DRIVE_FIXED == GetDriveType( volumePathName.data() ) );
				}
			}
			if ( isLocalFile ) {
				auto gain = item->Info.GetGainTrack();
				if ( !gain.has_value() ) {
					m_Playlist->GetLibrary().GetMediaInfo( item->Info, false /*scanMedia*/, false /*sendNotification*/ );
					gain = item->Info.GetGainTrack();
					if ( !gain.has_value() ) {
						gain = GainCalculator::CalculateTrackGain( *item, m_Handlers, canContinue );
						if ( gain.has_value() ) {
							const MediaInfo previousMediaInfo( item->Info );
							item->Info.SetGainTrack( gain );
							std::lock_guard<std::mutex> lock( m_PlaylistMutex );
							m_Playlist->GetLibrary().UpdateTrackGain( previousMediaInfo, item->Info );
						}
					}
				}
			}
			++item;
		}
	} while ( WAIT_OBJECT_0 != WaitForSingleObject( m_LoudnessPrecalcStopEvent, interval ) );
}

void Output::StartLoudnessPrecalcThread()
{
	StopLoudnessPrecalcThread();
	if ( nullptr != m_LoudnessPrecalcStopEvent ) {
		ResetEvent( m_LoudnessPrecalcStopEvent );
		if ( m_LoudnessNormalisation && m_Playlist && ( Playlist::Type::CDDA != m_Playlist->GetType() ) ) {
			m_LoudnessPrecalcThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, LoudnessPrecalcThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
			if ( nullptr != m_LoudnessPrecalcThread ) {
				SetThreadPriority( m_LoudnessPrecalcThread, THREAD_MODE_BACKGROUND_BEGIN );
			}
		}
	}
}

void Output::StopLoudnessPrecalcThread()
{
	if ( nullptr != m_LoudnessPrecalcThread ) {
		SetThreadPriority( m_LoudnessPrecalcThread, THREAD_MODE_BACKGROUND_END );
		SetEvent( m_LoudnessPrecalcStopEvent );
		WaitForSingleObject( m_LoudnessPrecalcThread, INFINITE );
		CloseHandle( m_LoudnessPrecalcThread );
		m_LoudnessPrecalcThread = nullptr;
	}
}

const Handlers& Output::GetHandlers() const
{
	return m_Handlers;
}

void Output::SetOutputStreamFinished( const bool finished )
{
	m_OutputStreamFinished = finished;
}

Output::OutputDecoderPtr Output::GetNextDecoder( Playlist::Item& item )
{
	OutputDecoderPtr nextDecoder;
	Playlist::Item nextItem = item;
	item = {};

	if ( GetFollowTrackSelection() ) {
		bool selectNextItem = false;
		if ( GetRepeatTrack() ) {
			nextItem = m_CurrentItemDecoding;
		} else if ( const auto nextTrackToFollow = GetTrackToFollow( nextItem ) ) {
			auto [playlist, nextTrack, selectTrack] = *nextTrackToFollow;
			ChangePlaylist( playlist );
			nextItem = nextTrack;
			selectNextItem = selectTrack;
		} else {
			nextItem = {};
		}
		if ( nextItem.ID > 0 ) {
			nextDecoder = OpenOutputDecoder( nextItem, true /*usePreloadedDecoder*/ );
			if ( nextDecoder ) {
				item = nextItem;
				PreloadNextDecoder( item );
				if ( m_OnSelectFollowedTrackCallback && selectNextItem ) {
					m_OnSelectFollowedTrackCallback( nextItem.ID );
				}
			}
		}
		return nextDecoder;
	}

	std::lock_guard<std::mutex> playlistLock( m_PlaylistMutex );
	if ( m_Playlist ) {
		size_t skip = 0;
		while ( !nextDecoder && ( nextItem.ID > 0 ) && ( skip++ < s_MaxSkipItems ) ) {
			if ( GetRandomPlay() ) {
				std::lock_guard<std::mutex> preloadLock( m_PreloadedDecoderMutex );
				if ( ( m_PreloadedDecoder.item.ID > 0 ) && ( m_Playlist->ContainsItem( m_PreloadedDecoder.item ) ) ) {
					nextItem = m_PreloadedDecoder.item;
				} else {
					nextItem = m_Playlist->GetRandomItem( nextItem );
				}
			} else if ( GetRepeatTrack() ) {
				nextItem = m_CurrentItemDecoding;
			} else {
				Playlist::Item currentItem = nextItem;
				nextItem = {};
				m_Playlist->GetNextItem( currentItem, nextItem, GetRepeatPlaylist() /*wrap*/ );
			}
			if ( nextItem.ID > 0 ) {
				nextDecoder = OpenOutputDecoder( nextItem, true /*usePreloadedDecoder*/ );
				if ( nextDecoder ) {
					item = nextItem;
					PreloadNextDecoder( item );
				}
			}
		}
	}

	return nextDecoder;
}

void Output::StartPreloadDecoderThread()
{
	if ( ( nullptr == m_PreloadDecoderThread ) && ( nullptr != m_PreloadDecoderStopEvent ) && ( nullptr != m_PreloadDecoderWakeEvent ) ) {
		ResetEvent( m_PreloadDecoderStopEvent );
		m_PreloadDecoderThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, PreloadDecoderProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}
}

void Output::StopPreloadDecoderThread()
{
	if ( nullptr != m_PreloadDecoderThread ) {
		SetEvent( m_PreloadDecoderStopEvent );
		WaitForSingleObject( m_PreloadDecoderThread, INFINITE );
		CloseHandle( m_PreloadDecoderThread );
		m_PreloadDecoderThread = nullptr;
	}
}

void Output::PreloadDecoderHandler()
{
	const HANDLE handles[ 2 ] = { m_PreloadDecoderStopEvent, m_PreloadDecoderWakeEvent };
	while ( WaitForMultipleObjects( 2, handles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		const std::wstring& filename = m_PreloadedDecoder.itemToPreload.Info.GetFilename();
		m_PreloadedDecoder.decoder = IsURL( filename ) ? nullptr : OpenOutputDecoder( m_PreloadedDecoder.itemToPreload );
		if ( m_PreloadedDecoder.decoder ) {
			m_PreloadedDecoder.item = m_PreloadedDecoder.itemToPreload;
		} else {
			m_PreloadedDecoder.item = {};
		}
		m_PreloadedDecoder.itemToPreload = {};
		ResetEvent( m_PreloadDecoderWakeEvent );
	}
}

void Output::PreloadNextDecoder( const Playlist::Item& item )
{
	if ( m_Playlist ) {
		Playlist::Item preloadItem;
		if ( GetRandomPlay() ) {
			preloadItem = m_Playlist->GetRandomItem( item );
		} else if ( GetRepeatTrack() ) {
			preloadItem = item;
		} else {
			m_Playlist->GetNextItem( item, preloadItem, GetRepeatPlaylist() /*wrap*/ );
		}

		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		m_PreloadedDecoder.itemToPreload = preloadItem;
		SetEvent( m_PreloadDecoderWakeEvent );
	}
}

std::vector<std::pair<float /*seconds*/, std::wstring /*title*/>> Output::GetStreamTitleQueue()
{
	std::lock_guard<std::mutex> lock( m_StreamTitleMutex );
	return m_StreamTitleQueue;
}

void Output::SetStreamTitleQueue( const std::vector<std::pair<float /*seconds*/, std::wstring /*title*/>>& queue )
{
	std::lock_guard<std::mutex> lock( m_StreamTitleMutex );
	m_StreamTitleQueue = queue;
}

void Output::SetPlaylistChangeCallback( PlaylistChangeCallback callback )
{
	m_OnPlaylistChangeCallback = callback;
}

void Output::SetSelectFollowedTrackCallback( SelectFollowedTrackCallback callback )
{
	m_OnSelectFollowedTrackCallback = callback;
}

void Output::SetEndSync( const HSTREAM stream )
{
	if ( stream == m_OutputStream ) {
		BASS_ChannelSetSync( stream, BASS_SYNC_END | BASS_SYNC_ONETIME, 0 /*param*/, SyncEnd, this );
	} else if ( const DWORD mixerStream = BASS_Mixer_ChannelGetMixer( m_MixerStream ); mixerStream == m_OutputStream ) {
		BASS_ChannelSetSync( mixerStream, BASS_SYNC_STALL | BASS_SYNC_ONETIME, 0 /*param*/, SyncEnd, this );
		m_MixerStreamHasEndSync = true;
	}
}

bool Output::CanBackgroundThreadProceed( const HANDLE stopEvent )
{
	constexpr DWORD kStartupDelayMsec = static_cast<DWORD>( 1000 * 2 * s_BufferLength );
	return ( WAIT_TIMEOUT == WaitForSingleObject( stopEvent, kStartupDelayMsec ) );
}

void Output::SetPlaylistInformationToFollow( Playlist::Ptr playlist, const Playlist::Items& selectedItems )
{
	std::lock_guard<std::mutex> lock( m_FollowPlaylistInformationMutex );
	m_FollowPlaylistInformation = std::make_pair( playlist, selectedItems );
}

std::pair<Playlist::Ptr, Playlist::Items> Output::GetPlaylistInformationToFollow()
{
	std::lock_guard<std::mutex> lock( m_FollowPlaylistInformationMutex );
	return m_FollowPlaylistInformation;
}

std::optional<std::tuple<Playlist::Ptr, Playlist::Item, bool>> Output::GetTrackToFollow( const Playlist::Item& currentItem, const bool previous )
{
	auto [playlist, selectedItems] = GetPlaylistInformationToFollow();
	if ( !playlist || selectedItems.empty() ) {
		std::lock_guard<std::mutex> lock( m_PlaylistMutex );
		playlist = m_Playlist;
		if ( !playlist )
			return std::nullopt;
	}

	auto foundItem = std::find_if( selectedItems.begin(), selectedItems.end(), [ id = currentItem.ID ] ( const Playlist::Item& item )
		{
			return ( id == item.ID );
		} );

	Playlist::Item nextTrack;
	bool selectTrack = false;

	if ( ( selectedItems.end() == foundItem ) && !selectedItems.empty() ) {
		nextTrack = selectedItems.front();
	} else {
		if ( !selectedItems.empty() ) {
			if ( previous ) {
				if ( selectedItems.begin() != foundItem ) {
					--foundItem;
				} else {
					foundItem = selectedItems.end();
				}
			} else {
				++foundItem;
			}
		}
		if ( foundItem == selectedItems.end() ) {
			if ( GetRandomPlay() ) {
				nextTrack = playlist->GetRandomItem( currentItem );
				selectTrack = true;
			} else if ( Playlist::Item nextItem; previous ? playlist->GetPreviousItem( currentItem, nextItem, GetRepeatPlaylist() ) : playlist->GetNextItem( currentItem, nextItem, GetRepeatPlaylist() ) ) {
				nextTrack = nextItem;
				selectTrack = true;
			}
		} else {
			nextTrack = *foundItem;
		}
	}

	if ( 0 == nextTrack.ID )
		return std::nullopt;

	return std::make_tuple( playlist, nextTrack, selectTrack );
}

bool Output::ChangePlaylist( const Playlist::Ptr& playlist )
{
	std::lock_guard<std::mutex> playlistLock( m_PlaylistMutex );
	if ( m_Playlist != playlist ) {
		m_Playlist = playlist;
		if ( nullptr != m_OnPlaylistChangeCallback ) {
			m_OnPlaylistChangeCallback( m_Playlist );
		}
		return true;
	}
	return false;
}
