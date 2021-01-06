#include "Output.h"

#include "Bling.h"
#include "GainCalculator.h"
#include "Utility.h"
#include "VUPlayer.h"

#include "opus.h"

#include "bassasio.h"
#include "bassmix.h"
#include "basswasapi.h"

#include <cmath>

// Output buffer length, in seconds.
static const float s_BufferLength = 1.5f;

// Cutoff point, in seconds, within which previous track replays the current track from the beginning.
static const float s_PreviousTrackCutoff = 2.0f;

// Amount of time to devote to estimating a gain value, in seconds.
static const float s_GainPrecalcTime = 0.25f;

// Minimum allowed gain adjustment, in dB.
static const float s_GainMin = -20.0f;

// Maximum allowed gain adjustment, in dB.
static const float s_GainMax = 20.0f;

// Fade out duration, in seconds.
static const float s_FadeOutDuration = 5.0f;

// The relative volume at which to set the crossfade position on a track.
static const float s_CrossfadeVolume = 0.3f;

// The fade to next duration, in seconds.
static const float s_FadeToNextDuration = 3.0f;

// Indicates when fading to the next track.
static const long s_ItemIsFadingToNext = -1;

// The fade out duration when stopping a track (in standard mode), in milliseconds.
static const DWORD s_StopFadeDuration = 20;

// Maximum number of playlist items to skip when trying to switch decoder streams.
static const size_t s_MaxSkipItems = 20;

// Define to output debug timing for slow StreamProc calls.
#undef STREAMPROC_TIMING

DWORD CALLBACK Output::StreamProc( HSTREAM handle, void *buf, DWORD length, void *user )
{
	DWORD bytesRead = 0;
	Output* output = static_cast<Output*>( user );
	if ( nullptr != output ) {
#ifdef STREAMPROC_TIMING
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
			if ( 0 == bytesRead ) {
				bytesRead = BASS_STREAMPROC_END;
				output->SetOutputStreamFinished( true );
			}
		}

#ifdef STREAMPROC_TIMING
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
			case BASS_WASAPI_NOTIFY_DISABLED :
			case BASS_WASAPI_NOTIFY_FAIL : {
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

Output::Output( const HWND hwnd, const Handlers& handlers, Settings& settings, const float initialVolume ) :
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
	m_StopAtTrackEnd( false ),
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
	m_SoftClipStateCrossfading(),
	m_CrossfadeSeekOffset( 0 ),
	m_GainEstimateMap(),
	m_BlingMap(),
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
	m_LeadInSeconds( 0 ),
	m_PreloadedDecoder( {} ),
	m_PreloadedDecoderMutex()
{
	InitialiseBass();
	SetVolume( initialVolume );
	SetPitch( m_Pitch );

	m_Settings.GetGainSettings( m_GainMode, m_LimitMode, m_GainPreamp );
	m_Settings.GetPlaybackSettings( m_RandomPlay, m_RepeatTrack, m_RepeatPlaylist, m_Crossfade );

	StartPreloadDecoderThread();
}

Output::~Output()
{
	StopCrossfadeThread();
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

bool Output::Play( const long playlistID, const float seek )
{
	Stop();

	Playlist::Item item( { playlistID, MediaInfo() } );
	if ( ( 0 == item.ID ) && m_Playlist ) {
		const Playlist::ItemList items = m_Playlist->GetItems();
		if ( !items.empty() ) {
			item.ID = items.front().ID;
		}
	}

	if ( m_Playlist && m_Playlist->GetItem( item ) ) {
		m_DecoderStream = OpenDecoder( item );
		if ( m_DecoderStream ) {

			const DWORD outputBufferSize = static_cast<DWORD>( 1000 * ( ( ( MediaInfo::Source::CDDA ) == item.Info.GetSource() ) ? ( 2 * s_BufferLength ) : s_BufferLength ) );
			const DWORD previousOutputBufferSize = BASS_GetConfig( BASS_CONFIG_BUFFER );
			if ( previousOutputBufferSize != outputBufferSize ) {
				BASS_SetConfig( BASS_CONFIG_BUFFER, outputBufferSize );
			}

			EstimateGain( item );

			m_DecoderSampleRate = m_DecoderStream->GetSampleRate();
			const DWORD freq = static_cast<DWORD>( m_DecoderSampleRate );
			float seekPosition = seek;
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

			if ( CreateOutputStream( item.Info ) ) {
				m_CurrentItemDecoding = item;
				UpdateOutputVolume();
				if ( 1.0f != m_Pitch ) {
					BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_FREQ, freq * m_Pitch );
				}
				UpdateEQ( m_CurrentEQ );

				State state = StartOutput();
				if ( State::Playing == state ) {
					Queue queue = GetOutputQueue();
					queue.push_back( { item, 0, seekPosition } );
					SetOutputQueue( queue );
					if ( GetCrossfade() ) {
						CalculateCrossfadePoint( item, seekPosition );
					}
					StartLoudnessPrecalcThread();
					PreloadNextDecoder( item );
				} else {
					Stop();
				}
			}
		}
	}
	const bool started = ( GetState() == State::Playing );
	return started;
}

void Output::Stop()
{
	if ( 0 != m_OutputStream ) {
		if ( Settings::OutputMode::Standard == m_OutputMode ) {
			if ( BASS_ACTIVE_PLAYING == BASS_ChannelIsActive( m_OutputStream ) ) {
				const HANDLE slideFinishedEvent = CreateEvent( nullptr /*attributes*/, FALSE /*manualReset*/, FALSE /*initial*/, L"" /*name*/ );
				if ( nullptr != slideFinishedEvent ) {
					const HSYNC syncSlide =	BASS_ChannelSetSync( m_OutputStream, BASS_SYNC_SLIDE | BASS_SYNC_ONETIME, 0 /*param*/, SyncSlideStop, slideFinishedEvent );
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
	m_SoftClipStateCrossfading.clear();
	m_RestartItemID = 0;
	SetOutputQueue( Queue() );
	m_FadeOut = false;
	m_FadeToNext = false;
	m_SwitchToNext = false;
	m_FadeOutStartPosition = 0;
	m_LastTransitionPosition = 0;
	m_WASAPIFailed = false;
	m_WASAPIPaused = false;
	m_OutputStreamFinished = false;
	StopCrossfadeThread();
	StopLoudnessPrecalcThread();
	std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
	m_PreloadedDecoder = {};
}

void Output::Pause()
{
	const State state = GetState();
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard : {
			if ( State::Paused == state ) {	
				BASS_ChannelPlay( m_OutputStream, FALSE /*restart*/ );
			} else if ( State::Playing == state ) {
				BASS_ChannelPause( m_OutputStream );
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive : {
			if ( ( -1 != BASS_WASAPI_GetDevice() ) && BASS_WASAPI_IsStarted() ) {
				m_WASAPIPaused = !m_WASAPIPaused;
			}
			break;
		}
		case Settings::OutputMode::ASIO : {
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
				if ( GetRandomPlay() ) {
					previousItem = m_Playlist->GetRandomItem( currentItem );
				} else {
					m_Playlist->GetPreviousItem( currentItem, previousItem );
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
		if ( GetRandomPlay() ) {
			nextItem = m_Playlist->GetRandomItem( currentItem );
		} else {
			m_Playlist->GetNextItem( currentItem, nextItem );
		}
		if ( nextItem.ID > 0 ) {
			Play( nextItem.ID );
		}
	}
}

void Output::Play( const Playlist::Ptr playlist, const long startID, const float seek )
{
	Stop();
	if ( m_Playlist != playlist ) {
		m_Playlist = playlist;
	}
	Play( startID, seek );
}

Playlist::Ptr Output::GetPlaylist()
{
	return m_Playlist;
}

Output::State Output::GetState()
{
	State state = State::Stopped;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard : {
			const DWORD channelState = BASS_ChannelIsActive( m_OutputStream );
			switch ( channelState ) {
				case BASS_ACTIVE_PLAYING : {
					state = State::Playing;
					break;
				}
				case BASS_ACTIVE_STALLED : {
					if ( m_OutputStreamFinished ) {
						Stop();
					} else {
						state = State::Playing;
					}
					break;
				}
				case BASS_ACTIVE_PAUSED : {
					state = State::Paused;
					break;
				}
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive : {
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
		case Settings::OutputMode::ASIO : {
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
	}
	return currentItem;
}

DWORD Output::ReadSampleData( float* buffer, const DWORD byteCount, HSTREAM handle )
{
	// Read sample data into the output buffer.
	DWORD bytesRead = 0;
	if ( ( nullptr != buffer ) && ( byteCount > 0 ) && m_DecoderStream ) {
		const long channels = m_DecoderStream->GetChannels();
		if ( channels > 0 ) {
			long samplesToRead = static_cast<long>( byteCount ) / ( channels * 4 );

			if ( GetCrossfade() && !GetFadeOut() && !GetFadeToNext() ) {
				const float crossfadePosition = GetCrossfadePosition();			
				if ( crossfadePosition > 0 ) {
					bool checkCrossFade = ( GetRandomPlay() || GetRepeatTrack() );
					if ( !checkCrossFade ) {
						std::lock_guard<std::mutex> lock( m_PlaylistMutex );
						Playlist::Item nextItem = {};
						checkCrossFade = m_Playlist->GetNextItem( m_CurrentItemDecoding, nextItem, GetRepeatPlaylist() /*wrap*/ );
					}
					if ( checkCrossFade ) {
						// Ensure we don't read past the crossfade point.
						const long sampleRate = m_DecoderStream->GetSampleRate();
						if ( sampleRate > 0 ) {
							const float trackPos = GetDecodePosition() - m_LastTransitionPosition - m_LeadInSeconds;
							const float secondsTillCrossfade = crossfadePosition - trackPos;
							const long samplesTillCrossfade = static_cast<long>( secondsTillCrossfade * sampleRate );
							if ( samplesTillCrossfade < samplesToRead ) {
								samplesToRead = samplesTillCrossfade;
								if ( samplesToRead <= 0 ) {
									samplesToRead = 0;
									// Hold on the the decoder, and indicate its fade out position.
									std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
									m_CrossfadingStream = m_DecoderStream;
									m_CurrentItemCrossfading = m_CurrentItemDecoding;
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
				m_SoftClipStateCrossfading = m_SoftClipStateDecoding;
			}

			bytesRead = static_cast<DWORD>( m_DecoderStream->Read( buffer, samplesToRead ) * channels * 4 );
		}
	}

	// Check if we need to switch to the next decoder stream.
	if ( 0 == bytesRead ) {
		SetCrossfadePosition( 0 );
		m_LastTransitionPosition = 0;

		if ( GetStopAtTrackEnd() || GetFadeOut() ) {
			// Set a sync on the output stream, so that the states can be toggled when playback actually finishes.
			m_RestartItemID = {};
			BASS_ChannelSetSync( handle, BASS_SYNC_END | BASS_SYNC_ONETIME, 0 /*param*/, SyncEnd, this );
		} else if ( ( 0 != BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE ) ) && m_DecoderStream ) {
			// Create a stream from the next playlist item, but only if there has not been an error starting playback.
			Playlist::Item nextItem = m_CurrentItemDecoding;
			Decoder::Ptr nextDecoder = GetNextDecoder( nextItem );
			if ( nextDecoder ) {
				EstimateGain( nextItem );
				const long channels = m_DecoderStream->GetChannels();
				const long sampleRate = m_DecoderStream->GetSampleRate();
				if ( ( nextDecoder->GetChannels() == channels ) && ( nextDecoder->GetSampleRate() == sampleRate ) ) {
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
							CalculateCrossfadePoint( nextItem );
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
				BASS_ChannelSetSync( handle, BASS_SYNC_END | BASS_SYNC_ONETIME, 0 /*param*/, SyncEnd, this );

				std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
				if ( m_CrossfadingStream ) {
					m_CrossfadingStream.reset();
					m_CurrentItemCrossfading = {};
					m_SoftClipStateCrossfading.clear();
				}
			}
		}
	}

	if ( 0 != bytesRead ) {
		const long currentDecodingChannels = m_CurrentItemDecoding.Info.GetChannels();
		if ( currentDecodingChannels > 0 ) {
			ApplyGain( buffer, static_cast<long>( bytesRead / ( currentDecodingChannels * 4 ) ), m_CurrentItemDecoding, m_SoftClipStateDecoding );
		}

		std::lock_guard<std::mutex> crossfadingStreamLock( m_CrossfadingStreamMutex );
		if ( m_CrossfadingStream ) {
			// Decode and fade out the crossfading stream and mix with the final output buffer.
			const long channels = m_CurrentItemCrossfading.Info.GetChannels();
			const long samplerate = m_CurrentItemCrossfading.Info.GetSampleRate();
			if ( ( channels > 0 ) && ( samplerate > 0 ) ) {
				const long samplesToRead = static_cast<long>( bytesRead ) / ( channels * 4 );
				std::vector<float> crossfadingBuffer( bytesRead / 4 );
				const long crossfadingBytesRead = m_CrossfadingStream->Read( &crossfadingBuffer[ 0 ], samplesToRead ) * channels * 4;
				ApplyGain( &crossfadingBuffer[ 0 ], crossfadingBytesRead / ( channels * 4 ), m_CurrentItemCrossfading, m_SoftClipStateCrossfading );
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
		const long channels = m_DecoderStream->GetChannels();
		const long samplerate = m_DecoderStream->GetSampleRate();
		if ( ( currentPos > m_FadeOutStartPosition ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
			if ( ( currentPos - m_FadeOutStartPosition ) > GetFadeOutDuration() ) {
				bytesRead = 0;
				// Set a sync on the output stream, so that the 'fade out' state can be toggled when playback actually finishes.
				m_RestartItemID = {};
				BASS_ChannelSetSync( handle, BASS_SYNC_END | BASS_SYNC_ONETIME, 0 /*param*/, SyncEnd, this );
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
		m_Volume = ( volume < 0.0f ) ? 0.0f : ( ( volume > 1.0f ) ? 1.0f : volume );
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

void Output::GetLevels( float& left, float& right )
{
	left = right = 0;
	if ( 0 != m_OutputStream ) {
		float levels[ 2 ] = {};
		const float length = 0.05f;
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard : {
				if ( TRUE == BASS_ChannelGetLevelEx( m_OutputStream, levels, length, BASS_LEVEL_STEREO ) ) {
					left = levels[ 0 ];
					right = levels[ 1 ];
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive : {
				if ( TRUE == BASS_WASAPI_GetLevelEx( levels, length, BASS_LEVEL_STEREO ) ) {
					left = levels[ 0 ];
					right = levels[ 1 ];
				}
				break;
			}
			case Settings::OutputMode::ASIO : {
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
			case Settings::OutputMode::Standard : 
			case Settings::OutputMode::ASIO : {
				BASS_CHANNELINFO channelInfo = {};
				if ( BASS_ChannelGetInfo( m_OutputStream, &channelInfo ) ) {
					channels = static_cast<long>( channelInfo.chans );
					const long sampleSize = channels * sampleCount;
					if ( sampleSize > 0 ) {
						samples.resize( sampleSize );
						if ( Settings::OutputMode::Standard == m_OutputMode ) {
							BASS_ChannelGetData( m_OutputStream, &samples[ 0 ], sampleSize * sizeof( float ) );
						} else {
							BASS_Mixer_ChannelGetData( m_OutputStream, &samples[ 0 ], sampleSize * sizeof( float ) );
						}
					}
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive : {
				if ( BASS_WASAPI_IsStarted() ) {
					BASS_WASAPI_INFO wasapiInfo = {};
					if ( TRUE == BASS_WASAPI_GetInfo( &wasapiInfo ) ) {
						channels = static_cast<long>( wasapiInfo.chans );
						const long sampleSize = channels * sampleCount;
						if ( sampleSize > 0 ) {
							samples.resize( sampleSize );
							BASS_WASAPI_GetData( &samples[ 0 ], sampleSize * sizeof( float ) );
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
	constexpr std::pair fftSize { BASS_DATA_FFT4096, 2048 };

	if ( 0 != m_OutputStream ) {
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard : {
				fft.resize( fftSize.second );
				if ( -1 == BASS_ChannelGetData( m_OutputStream, fft.data(), BASS_DATA_FLOAT | fftSize.first ) ) {
					fft.clear();
				}
				break;
			}
			case Settings::OutputMode::WASAPIExclusive : {
				if ( BASS_WASAPI_IsStarted() ) {
					fft.resize( fftSize.second );
					if ( -1 == BASS_WASAPI_GetData( fft.data(), BASS_DATA_FLOAT | fftSize.first ) ) {
						fft.clear();
					}
				}
				break;
			}
			case Settings::OutputMode::ASIO : {
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
	if ( GetStopAtTrackEnd() || GetFadeOut() ) {
		if ( GetStopAtTrackEnd() ) {
			ToggleStopAtTrackEnd();
		}
		if ( GetFadeOut() ) {
			ToggleFadeOut();
		}
		m_RestartItemID = 0;
	} else if ( m_RestartItemID > 0 ) {
		PostMessage( m_Parent, MSG_RESTARTPLAYBACK, m_RestartItemID, NULL /*lParam*/ );
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
				CalculateCrossfadePoint( item.PlaylistItem, item.InitialSeek );
			}
		}
	} else {
		StopCrossfadeThread();
	}
}

bool Output::OnUpdatedMedia( const MediaInfo& mediaInfo )
{
	std::lock_guard<std::mutex> lock( m_PlaylistMutex );

	bool changed = false;
	State state = GetState();
	if ( State::Stopped == state ) {
		if ( m_CurrentSelectedPlaylistItem.Info.GetFilename() == mediaInfo.GetFilename() ) {
			m_CurrentSelectedPlaylistItem.Info = mediaInfo;
			changed = true;
		}
	} else {
		const Item currentPlaying = GetCurrentPlaying();
		Queue queue = GetOutputQueue();
		for ( auto& iter : queue ) {
			if ( iter.PlaylistItem.Info.GetFilename() == mediaInfo.GetFilename() ) {
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
		case Settings::OutputMode::Standard : {
			BASS_DEVICEINFO deviceInfo = {};
			while ( TRUE == BASS_GetDeviceInfo( ++deviceID, &deviceInfo ) ) {
				if ( nullptr != deviceInfo.name ) {
					devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
				}
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive : {
			BASS_WASAPI_DEVICEINFO deviceInfo = {};
			while ( TRUE == BASS_WASAPI_GetDeviceInfo( deviceID, &deviceInfo ) ) {
				if ( ( deviceInfo.flags & BASS_DEVICE_ENABLED ) && !( deviceInfo.flags & BASS_DEVICE_INPUT ) && ( nullptr != deviceInfo.name ) ) {
					devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
				}
				++deviceID;
			}
			break;
		}
		case Settings::OutputMode::ASIO : {
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

	const bool reinitialise = ( settingsMode != m_OutputMode ) || ( settingsDevice != m_OutputDevice );

	if ( reinitialise ) {
		Stop();
		if ( -1 != BASS_ASIO_GetDevice() ) {
			BASS_ASIO_Free();
		}
		BASS_Free();
		InitialiseBass();
	}

	Settings::GainMode gainMode = Settings::GainMode::Disabled;
	Settings::LimitMode limitMode = Settings::LimitMode::None;
	float gainPreamp = 0;
	m_Settings.GetGainSettings( gainMode, limitMode, gainPreamp );
	if ( ( gainMode != m_GainMode ) || ( limitMode != m_LimitMode ) || ( gainPreamp != m_GainPreamp ) ) {
		m_GainMode = gainMode;
		m_LimitMode = limitMode;
		m_GainPreamp = gainPreamp;
		EstimateGain( m_CurrentItemDecoding );
		if ( State::Stopped != GetState() ) {
			StartLoudnessPrecalcThread();
		}
	}

	m_Handlers.SettingsChanged( m_Settings );
}

void Output::InitialiseBass()
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
}

void Output::EstimateGain( Playlist::Item& item )
{
	if ( ( Settings::GainMode::Disabled != m_GainMode ) && !IsURL( item.Info.GetFilename() ) ) {
		float gain = item.Info.GetGainAlbum();
		if ( std::isnan( gain ) || ( Settings::GainMode::Track == m_GainMode ) ) {
			const float trackGain = item.Info.GetGainTrack();
			if ( !std::isnan( trackGain ) ) {
				gain = trackGain;
			}
		}
		if ( std::isnan( gain ) ) {
			const auto estimateIter = m_GainEstimateMap.find( item.ID );
			if ( m_GainEstimateMap.end() != estimateIter ) {
				item.Info.SetGainTrack( estimateIter->second );
			} else {
				Decoder::Ptr tempDecoder = OpenDecoder( item );
				if ( tempDecoder ) {
					const float trackGain = tempDecoder->CalculateTrackGain( [] () { return true; }, s_GainPrecalcTime );
					item.Info.SetGainTrack( trackGain );
					m_GainEstimateMap.insert( GainEstimateMap::value_type( item.ID, trackGain ) );
				}
			}
		}
	}
}

void Output::CalculateCrossfadePoint( const Playlist::Item& item, const float seekOffset )
{
	StopCrossfadeThread();
	ResetEvent( m_CrossfadeStopEvent );
	m_CrossfadeItem = item;
	m_CrossfadeSeekOffset = seekOffset;
	m_CrossfadeThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, CrossfadeThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
}

void Output::CalculateCrossfadeHandler()
{
	const Decoder::Ptr decoder = IsURL( m_CrossfadeItem.Info.GetFilename() ) ? nullptr : OpenDecoder( m_CrossfadeItem );
	if ( decoder ) {
		const float duration = decoder->GetDuration();
		const long channels = decoder->GetChannels();
		const long samplerate = decoder->GetSampleRate();
		if ( ( duration > 0 ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
			if ( 0.0f == m_CrossfadeSeekOffset ) {
				decoder->SkipSilence();
			}

			float position = 0;
			float crossfadePosition = 0;

			int64_t cumulativeCount = 0;
			double cumulativeTotal = 0;
			double cumulativeRMS = 0;

			const double crossfadeRMSRatio = s_CrossfadeVolume;
			const long windowSize = samplerate / 10;
			std::vector<float> buffer( windowSize * channels );

			while ( WAIT_OBJECT_0 != WaitForSingleObject( m_CrossfadeStopEvent, 0 ) ) {
				long sampleCount = decoder->Read( &buffer[ 0 ], windowSize );
				if ( sampleCount > 0 ) {
					auto sampleIter = buffer.begin();
					double windowTotal = 0;
					for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
						for ( long channel = 0; channel < channels; channel++, sampleIter++, cumulativeCount++ ) {
							const double value = *sampleIter * *sampleIter;
							windowTotal += value;
							cumulativeTotal += value;
						}
					}

					const double windowRMS = sqrt( windowTotal / ( sampleCount * channels ) );
					cumulativeRMS = sqrt( cumulativeTotal / cumulativeCount );
					position += static_cast<float>( sampleCount ) / samplerate;

					if ( windowRMS > cumulativeRMS ) {
						crossfadePosition = position;
					} else if ( ( cumulativeRMS > 0 ) && ( ( windowRMS / cumulativeRMS ) > crossfadeRMSRatio ) ) {
						crossfadePosition = position;
					}
				} else {
					break;
				}
			}
			if ( WAIT_OBJECT_0 != WaitForSingleObject( m_CrossfadeStopEvent, 0 ) ) {
				SetCrossfadePosition( crossfadePosition - m_CrossfadeSeekOffset );
			}
		}
	}
}

void Output::StopCrossfadeThread()
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

float Output::GetCrossfadePosition() const
{
	return m_CrossfadePosition;
}

void Output::SetCrossfadePosition( const float position )
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
	const long channels = item.Info.GetChannels();
	if ( ( 0 != sampleCount ) && ( channels > 0 ) && ( ( Settings::GainMode::Disabled != m_GainMode ) || eqEnabled ) ) {
		float preamp = eqEnabled ? m_EQPreamp : 0;

		if ( Settings::GainMode::Disabled != m_GainMode ) {
			float gain = item.Info.GetGainAlbum();
			if ( std::isnan( gain ) || ( Settings::GainMode::Track == m_GainMode ) ) {
				gain = item.Info.GetGainTrack();
			}
			if ( !std::isnan( gain ) ) {
				if ( gain < s_GainMin ) {
					gain = s_GainMin;
				} else if ( gain > s_GainMax ) {
					gain = s_GainMax;
				}
				preamp += m_GainPreamp;
				preamp += gain;
			}
		}

		if ( 0 != preamp ) {
			const float scale = powf( 10.0f, preamp / 20.0f );
			const long totalSamples = sampleCount * channels;
			for ( long sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++ ) {
				buffer[ sampleIndex ] *= scale;
			}
			switch ( m_LimitMode ) {
				case Settings::LimitMode::Hard : {
					for ( long sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++ ) {
						if ( buffer[ sampleIndex ] < -1.0f ) {
							buffer[ sampleIndex ] = -1.0f;
						} else if ( buffer[ sampleIndex ] > 1.0f ) {
							buffer[ sampleIndex ] = 1.0f;
						}
					}
					break;
				}
				case Settings::LimitMode::Soft : {
					if ( softClipState.size() != static_cast<size_t>( channels ) ) {
						softClipState.resize( channels, 0 );
					}
					opus_pcm_soft_clip( buffer, sampleCount, channels, &softClipState[ 0 ] );
					break;
				}
				default : {
					break;
				}
			}
		}
	}
}

Output::Queue Output::GetOutputQueue()
{
	std::lock_guard<std::mutex> lock( m_QueueMutex );
	Queue queue = m_OutputQueue;
	return queue;
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

void Output::Bling( const int blingID )
{
	if ( Settings::OutputMode::Standard == m_OutputMode ) {
		auto blingIter = m_BlingMap.find( blingID );
		if ( m_BlingMap.end() == blingIter ) {
			const BYTE* bling = nullptr;
			QWORD blingSize = 0;
			switch ( blingID ) {
				case 1 : {
					bling = bling1;
					blingSize = sizeof( bling1 );
					break;
				}
				case 2 : {
					bling = bling2;
					blingSize = sizeof( bling2 );
					break;
				}
				case 3 : {
					bling = bling3;
					blingSize = sizeof( bling3 );
					break;
				}
				case 4 : {
					bling = bling4;
					blingSize = sizeof( bling4 );
					break;
				}
				default : {
					break;
				}
			}
			if ( nullptr != bling ) {
				const HSTREAM stream = BASS_StreamCreateFile( TRUE /*mem*/, bling, 0 /*offset*/, blingSize, 0 /*flags*/ );
				if ( 0 != stream ) {
					blingIter = m_BlingMap.insert( StreamMap::value_type( blingID, stream ) ).first;
				}
			}
		}
		if ( m_BlingMap.end() != blingIter ) {
			BASS_ChannelPlay( blingIter->second, TRUE /*restart*/ );
		}
	}
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

Decoder::Ptr Output::OpenDecoder( Playlist::Item& item, const bool usePreloadedDecoder )
{
	Decoder::Ptr decoder;
	if ( usePreloadedDecoder ) {
		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		if ( m_PreloadedDecoder.decoder && ( m_PreloadedDecoder.item.Info.GetFilename() == item.Info.GetFilename() ) && ( m_PreloadedDecoder.item.Info.GetFiletime() == item.Info.GetFiletime() ) ) {
			decoder = m_PreloadedDecoder.decoder;
			m_PreloadedDecoder.decoder.reset();
			m_PreloadedDecoder.item = {};
		}
	}

	if ( !decoder ) {
		decoder = m_Handlers.OpenDecoder( item.Info.GetFilename() );
	}

	if ( !decoder ) {
		auto duplicate = item.Duplicates.begin();
		while ( !decoder && ( item.Duplicates.end() != duplicate ) ) {
			decoder = m_Handlers.OpenDecoder( *duplicate );
			++duplicate;
		}
	}

	if ( decoder ) {
		m_Playlist->GetLibrary().UpdateMediaInfoFromDecoder( item.Info, *decoder );
	}

	return decoder;
}

Output::State Output::StartOutput()
{
	State state = State::Stopped;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard : {
			if ( BASS_ChannelPlay( m_OutputStream, TRUE /*restart*/ ) ) {
				state = State::Playing;
			}
			break;
		}
		case Settings::OutputMode::WASAPIExclusive : {
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
		case Settings::OutputMode::ASIO : {
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
	float seconds = 0;
	switch ( m_OutputMode ) {
		case Settings::OutputMode::Standard : {
			const QWORD bytePos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_BYTE );
			seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) );
			break;
		}
		case Settings::OutputMode::WASAPIExclusive :
		case Settings::OutputMode::ASIO : {
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
	if ( ( mediaInfo.GetSampleRate() > 0 ) && ( mediaInfo.GetChannels() > 0 ) ) {
		const DWORD samplerate = static_cast<DWORD>( mediaInfo.GetSampleRate() );
		const DWORD channels = static_cast<DWORD>( mediaInfo.GetChannels() );
		switch ( m_OutputMode ) {
			case Settings::OutputMode::Standard : {
				m_LeadInSeconds = 0;
				const DWORD flags = BASS_SAMPLE_FLOAT;
				m_OutputStream = BASS_StreamCreate( samplerate, channels, flags, StreamProc, this );
				success = ( 0 != m_OutputStream );
				break;
			}

			case Settings::OutputMode::WASAPIExclusive : {
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

				bool useMixFormat = false;
				int bufferMilliseconds = 0;
				int leadInMilliseconds = 0;
				m_Settings.GetAdvancedWasapiExclusiveSettings( useMixFormat, bufferMilliseconds, leadInMilliseconds );
				m_LeadInSeconds = static_cast<float>( leadInMilliseconds ) / 1000;

				if ( !useMixFormat ) {
					const DWORD format = BASS_WASAPI_CheckFormat( deviceNum, samplerate, channels, BASS_WASAPI_EXCLUSIVE );
					useMixFormat = ( -1 == format ) && ( BASS_ERROR_FORMAT == BASS_ErrorGetCode() );
				}
				DWORD outputSamplerate = useMixFormat ? 0 : samplerate;
				DWORD outputChannels = useMixFormat ? 0 : channels;

				DWORD flags = BASS_WASAPI_EXCLUSIVE | BASS_WASAPI_BUFFER | BASS_WASAPI_EVENT | BASS_WASAPI_ASYNC;
				const float period = 0;
				float buffer = static_cast<float>( bufferMilliseconds ) / 1000;
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

			case Settings::OutputMode::ASIO : {
				if ( InitASIO() ) {
					bool useDefaultSamplerate = false;
					int defaultSamplerate = 0;
					int leadInMilliseconds = 0;
					m_Settings.GetAdvancedASIOSettings( useDefaultSamplerate, defaultSamplerate, leadInMilliseconds );
					m_LeadInSeconds = static_cast<float>( leadInMilliseconds ) / 1000;

					double outputSamplerate = useDefaultSamplerate ? static_cast<double>( defaultSamplerate ) : static_cast<double>( samplerate );
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
		const float position = static_cast<float>( BASS_ChannelBytes2Seconds( handle, bytePos ) );
		if ( position < m_LeadInSeconds ) {
			BASS_CHANNELINFO info = {};
			if ( ( TRUE == BASS_ChannelGetInfo( handle, &info ) ) && ( info.freq > 0 ) && ( info.chans > 0 ) ) {
				bytesPadded = static_cast<DWORD>( 0.5f + ( m_LeadInSeconds - position ) * info.freq ) * info.chans * 4;
				bytesPadded = min( bytesPadded, byteCount );
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
	// Playlist might have new items, so check back every so often.
	const DWORD interval = 30 /*sec*/ * 1000;

	Decoder::CanContinue canContinue( [ stopEvent = m_LoudnessPrecalcStopEvent ] ()
	{
		return ( WAIT_OBJECT_0 != WaitForSingleObject( stopEvent, 0 ) );
	} );

	do {
		Playlist::ItemList items;
		{
			std::lock_guard<std::mutex> lock( m_PlaylistMutex );
			items = m_Playlist->GetItems();
		}
		auto item = items.begin();
		while ( ( items.end() != item ) && canContinue() ) {
			if ( std::isnan( item->Info.GetGainTrack() ) ) {
				m_Playlist->GetLibrary().GetMediaInfo( item->Info, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ );
				if ( std::isnan( item->Info.GetGainTrack() ) ) {
					const float gain = GainCalculator::CalculateTrackGain( item->Info.GetFilename(), m_Handlers, canContinue );
					if ( !std::isnan( gain ) ) {
						const MediaInfo previousMediaInfo( item->Info );
						item->Info.SetGainTrack( gain );
						std::lock_guard<std::mutex> lock( m_PlaylistMutex );
						m_Playlist->UpdateItem( *item );
						m_Playlist->GetLibrary().UpdateTrackGain( previousMediaInfo, item->Info );
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
		if ( m_Playlist && ( Playlist::Type::CDDA != m_Playlist->GetType() ) ) {
			Settings::GainMode gainMode = Settings::GainMode::Disabled;
			Settings::LimitMode limitMode = Settings::LimitMode::None;
			float preamp = 0;
			m_Settings.GetGainSettings( gainMode, limitMode, preamp );
			if ( Settings::GainMode::Disabled != gainMode ) {
				m_LoudnessPrecalcThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, LoudnessPrecalcThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
				if ( nullptr != m_LoudnessPrecalcThread ) {
					SetThreadPriority( m_LoudnessPrecalcThread, THREAD_PRIORITY_BELOW_NORMAL );
				}
			}
		}
	}
}

void Output::StopLoudnessPrecalcThread()
{
	if ( nullptr != m_LoudnessPrecalcThread ) {
		SetThreadPriority( m_LoudnessPrecalcThread, THREAD_PRIORITY_NORMAL );
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

Decoder::Ptr Output::GetNextDecoder( Playlist::Item& item )
{
	Decoder::Ptr nextDecoder;
	Playlist::Item nextItem = item;
	item = {};
	std::lock_guard<std::mutex> playlistLock( m_PlaylistMutex );
	if ( m_Playlist ) {
		size_t skip = 0;
		while ( !nextDecoder && ( nextItem.ID > 0 ) && ( skip++ < s_MaxSkipItems ) ) {
			if ( GetRandomPlay() ) {
				std::lock_guard<std::mutex> preloadLock( m_PreloadedDecoderMutex );
				Playlist::Item preloadItem = m_PreloadedDecoder.item;
				if ( m_PreloadedDecoder.decoder && ( preloadItem.ID > 0 ) && ( m_Playlist->GetItem( preloadItem ) ) ) {
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
				nextDecoder = OpenDecoder( nextItem, true /*usePreloadedDecoder*/ );
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
		m_PreloadedDecoder.decoder = IsURL( filename ) ? nullptr : m_Handlers.OpenDecoder( filename );
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
		} else if ( !GetRandomPlay() ) {
			m_Playlist->GetNextItem( item, preloadItem, GetRepeatPlaylist() /*wrap*/ );
		}

		std::lock_guard<std::mutex> lock( m_PreloadedDecoderMutex );
		m_PreloadedDecoder.itemToPreload = preloadItem;
		SetEvent( m_PreloadDecoderWakeEvent );
	}
}
