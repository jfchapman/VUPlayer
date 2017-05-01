#include "Output.h"

#include "Utility.h"

// Output buffer length, in seconds.
static const float s_BufferLength = 1.0f;

// Cutoff point, in seconds, within which previous track replays the current track from the beginning.
static const float s_PreviousTrackCutoff = 2.0f;

// Amount of time to devote to estimating a ReplayGain value, in seconds.
static const float s_ReplayGainPrecalcTime = 0.2f;

// Minimum allowed ReplayGain adjustment, in dB.
static const float s_ReplayGainMin = -15.0f;

// Maximum allowed ReplayGain adjustment, in dB.
static const float s_ReplayGainMax = 12.0f;

// Fade out duration, in seconds.
static const float s_FadeOutDuration = 5.0f;

// The relative volume at which to set the crossfade position on a track.
static const float s_CrossfadeVolume = 0.3f;

// The fade to next duration, in seconds.
static const float s_FadeToNextDuration = 3.0f;

// Indicates when fading to the next track.
static const long s_ItemIsFadingToNext = -1;

// BASS stream callback
static DWORD CALLBACK StreamProc( HSTREAM handle, void *buf, DWORD len, void *user )
{
	DWORD bytesRead = 0;
	Output* output = reinterpret_cast<Output*>( user );
	if ( nullptr != output ) {
		float* sampleBuffer = reinterpret_cast<float*>( buf );
		bytesRead = output->ReadSampleData( sampleBuffer, len, handle );
		if ( 0 == bytesRead ) {
			bytesRead = BASS_STREAMPROC_END;
		}
	}
	return bytesRead;
}

// BASS sync callback
static void CALLBACK SyncProc( HSYNC /*handle*/, DWORD /*channel*/, DWORD /*data*/, void *user )
{
	Output* output = reinterpret_cast<Output*>( user );
	if ( nullptr != output ) {
		output->RestartPlayback();
	}
}

// Crossfade calculation thread procedure
static DWORD WINAPI CrossfadeThreadProc( LPVOID lpParam )
{
	Output* output = reinterpret_cast<Output*>( lpParam );
	if ( nullptr != output ) {
		output->OnCalculateCrossfadeHandler();
	}
	return 0;
}

Output::Output( const HWND hwnd, const Handlers& handlers, Settings& settings, const float initialVolume ) :
	m_Parent( hwnd ),
	m_Handlers( handlers ),
	m_Settings( settings ),
	m_Playlist(),
	m_CurrentItemDecoding( {} ),
	m_DecoderStream(),
	m_OutputStream( 0 ),
	m_PlaylistMutex(),
	m_QueueMutex(),
	m_Volume( 1.0 ),
	m_OutputQueue(),
	m_RestartItem( {} ),
	m_RandomPlay( false ),
	m_RepeatTrack( false ),
	m_RepeatPlaylist( false ),
	m_Crossfade( false ),
	m_CurrentSelectedPlaylistItem( {} ),
	m_UsingDefaultDevice( false ),
	m_ReplaygainMode( Settings::ReplaygainMode::Disabled ),
	m_ReplaygainPreamp( 0 ),
	m_ReplaygainHardlimit( false ),
	m_StopAtTrackEnd( false ),
	m_Muted( false ),
	m_FadeOut( false ),
	m_FadeToNext( false ),
	m_SwitchToNext( false ),
	m_FadeOutStartPosition( 0 ),
	m_LastTransitionPosition( 0 ),
	m_CrossfadePosition( 0 ),
	m_CrossfadeFilename(),
	m_CrossfadeThread( nullptr ),
	m_CrossfadeStopEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_CrossfadingStream(),
	m_CurrentItemCrossfading( {} ),
	m_CrossfadeSeekOffset( 0 ),
	m_ReplayGainEstimateMap()
{
	InitialiseBass();
	SetVolume( initialVolume );
	m_Settings.GetReplaygainSettings( m_ReplaygainMode, m_ReplaygainPreamp, m_ReplaygainHardlimit );
	m_Settings.GetPlaybackSettings( m_RandomPlay, m_RepeatTrack, m_RepeatPlaylist, m_Crossfade );
}

Output::~Output()
{
	StopCrossfadeThread();
	CloseHandle( m_CrossfadeStopEvent );

	Stop();
	BASS_Free();
}

bool Output::Play( const long playlistID, const float seek )
{
	Stop();
	Playlist::Item item( { playlistID, MediaInfo() } );
	if ( m_Playlist && m_Playlist->GetItem( item ) ) {
		const std::wstring& filename = item.Info.GetFilename();
		m_DecoderStream = m_Handlers.OpenDecoder( filename );
		if ( m_DecoderStream ) {
			EstimateReplayGain( item );
			const DWORD freq = static_cast<DWORD>( m_DecoderStream->GetSampleRate() );
			const DWORD channels = static_cast<DWORD>( m_DecoderStream->GetChannels() );
			const DWORD flags = BASS_SAMPLE_FLOAT;
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
			m_OutputStream = BASS_StreamCreate( freq, channels, flags, StreamProc, this );
			if ( 0 != m_OutputStream ) {
				m_CurrentItemDecoding = item;
				BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_VOL, m_Muted ? 0 : m_Volume );
				if ( TRUE == BASS_ChannelPlay( m_OutputStream, TRUE /*restart*/ ) ) {
					Queue queue = GetOutputQueue();
					queue.push_back( { item, 0, seekPosition } );
					SetOutputQueue( queue );
					if ( GetCrossfade() ) {
						CalculateCrossfadePoint( filename, seekPosition );
					}
				}
			}
		}
	}
	const bool started = ( GetState() == State::Playing );
	return started;
}

void Output::Stop()
{
	BASS_ChannelStop( m_OutputStream );
	BASS_StreamFree( m_OutputStream );
	m_OutputStream = 0;
	m_DecoderStream.reset();
	m_CrossfadingStream.reset();
	m_CurrentItemDecoding = {};
	m_CurrentItemCrossfading = {};
	m_RestartItem = {};
	SetOutputQueue( Queue() );
	m_FadeOut = false;
	m_FadeToNext = false;
	m_SwitchToNext = false;
	m_FadeOutStartPosition = 0;
	m_LastTransitionPosition = 0;
	StopCrossfadeThread();
}

void Output::Pause()
{
	const State state = GetState();
	if ( State::Paused == state ) {
		BASS_ChannelPlay( m_OutputStream, FALSE /*restart*/ );
	} else if ( State::Playing == state ) {
		BASS_ChannelPause( m_OutputStream );
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
					previousItem = m_Playlist->GetRandomItem();
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
			nextItem = m_Playlist->GetRandomItem();
		} else {
			m_Playlist->GetNextItem( currentItem, nextItem );
		}
		if ( nextItem.ID > 0 ) {
			Play( nextItem.ID );
		}
	}
}

void Output::SetPlaylist( const Playlist::Ptr& playlist )
{
	std::lock_guard<std::mutex> lock( m_PlaylistMutex );
	if ( m_Playlist != playlist ) {
		m_Playlist = playlist;
	}
}

Playlist::Ptr Output::GetPlaylist()
{
	return m_Playlist;
}

Output::State Output::GetState() const
{
	State state = State::Stopped;
	const DWORD channelState = BASS_ChannelIsActive( m_OutputStream );
	switch ( channelState ) {
		case BASS_ACTIVE_PLAYING : {
			state = State::Playing;
			break;
		}
		case BASS_ACTIVE_PAUSED : {
			state = State::Paused;
			break;
		}
		default : {
			state = State::Stopped;
			break;
		}
	};
	return state;
}

Output::Item Output::GetCurrentPlaying()
{
	Item currentItem = {};
	const State state = GetState();
	if ( State::Stopped != state ) {
		const QWORD bytePos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_BYTE );
		const float seconds = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) );
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
					// Ensure we don't read past the crossfade point.
					const long sampleRate = m_DecoderStream->GetSampleRate();
					if ( sampleRate > 0 ) {
						const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
						const float trackPos = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) ) - m_LastTransitionPosition;
						const float secondsTillCrossfade = crossfadePosition - trackPos;
						const long samplesTillCrossfade = static_cast<long>( secondsTillCrossfade * sampleRate );
						if ( samplesTillCrossfade < samplesToRead ) {
							samplesToRead = samplesTillCrossfade;
							if ( samplesToRead <= 0 ) {
								samplesToRead = 0;
								// Hold on the the decoder, and indicate its fade out position.
								m_CrossfadingStream = m_DecoderStream;
								m_CurrentItemCrossfading = m_CurrentItemDecoding;
							}
						}
					}
				}
			} else if ( GetFadeToNext() && m_SwitchToNext ) {
				ToggleFadeToNext();
				samplesToRead = 0;
				m_CrossfadingStream = m_DecoderStream;
				m_CurrentItemCrossfading = m_CurrentItemDecoding;
				m_CurrentItemCrossfading.ID = s_ItemIsFadingToNext;
			}

			bytesRead = static_cast<DWORD>( m_DecoderStream->Read( buffer, samplesToRead ) * channels * 4 );
		}
	}

	// Check if we need to switch to the next decoder stream.
	if ( 0 == bytesRead ) {
		SetCrossfadePosition( 0 );
		m_LastTransitionPosition = 0;

		if ( GetStopAtTrackEnd() ) {
			ToggleStopAtTrackEnd();
		} else {
			// Create a stream from the next playlist item.
			std::lock_guard<std::mutex> lock( m_PlaylistMutex );
			Playlist::Item nextItem = {};
			if ( m_Playlist ) {
				if ( GetRandomPlay() ) {
					nextItem = m_Playlist->GetRandomItem();
				} else if ( GetRepeatTrack() ) {
					nextItem = m_CurrentItemDecoding;
				} else {
					m_Playlist->GetNextItem( m_CurrentItemDecoding, nextItem, GetRepeatPlaylist() /*wrap*/ );
				}
				if ( nextItem.ID > 0 ) {
					EstimateReplayGain( nextItem );
					const long channels = m_DecoderStream->GetChannels();
					const long sampleRate = m_DecoderStream->GetSampleRate();
					const std::wstring& filename = nextItem.Info.GetFilename();
					m_DecoderStream = m_Handlers.OpenDecoder( filename );
					if ( m_DecoderStream && ( m_DecoderStream->GetChannels() == channels ) && ( m_DecoderStream->GetSampleRate() == sampleRate ) ) {
						if ( GetCrossfade() || GetFadeToNext() ) {
							m_DecoderStream->SkipSilence();
						}

						const long sampleCount = static_cast<long>( byteCount ) / ( channels * 4 );
						bytesRead = static_cast<DWORD>( m_DecoderStream->Read( buffer, sampleCount ) * channels * 4 );
						m_CurrentItemDecoding = nextItem;

						const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
						m_LastTransitionPosition = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) );
						Queue queue = GetOutputQueue();
						queue.push_back( { m_CurrentItemDecoding, m_LastTransitionPosition } );
						SetOutputQueue( queue );

						if ( GetCrossfade() && ( 0 != bytesRead ) ) {
							CalculateCrossfadePoint( filename );
						}
					}
				}
			}

			if ( ( 0 == bytesRead ) && ( nextItem.ID > 0 ) && !GetFadeOut() ) {
				// Signal that playback should be restarted from the next playlist item.
				m_RestartItem = nextItem;
				BASS_ChannelSetSync( handle, BASS_SYNC_END | BASS_SYNC_ONETIME, 0 /*param*/, SyncProc, this );

				if ( m_CrossfadingStream ) {
					m_CrossfadingStream.reset();
					m_CurrentItemCrossfading = {};
				}
			}
		}
	}

	if ( 0 != bytesRead ) {
		ApplyReplayGain( buffer, static_cast<long>( bytesRead ), m_CurrentItemDecoding );

		if ( m_CrossfadingStream ) {
			// Decode and fade out the crossfading stream and mix with the final output buffer.
			const long channels = m_CurrentItemCrossfading.Info.GetChannels();
			const long samplerate = m_CurrentItemCrossfading.Info.GetSampleRate();
			if ( ( channels > 0 ) && ( samplerate > 0 ) ) {
				const long samplesToRead = static_cast<long>( bytesRead ) / ( channels * 4 );
				std::vector<float> crossfadingBuffer( bytesRead / 4 );
				const long crossfadingBytesRead = m_CrossfadingStream->Read( &crossfadingBuffer[ 0 ], samplesToRead ) * channels * 4;
				ApplyReplayGain( &crossfadingBuffer[ 0 ], crossfadingBytesRead, m_CurrentItemCrossfading );
				if ( crossfadingBytesRead <= static_cast<long>( bytesRead ) ) {
					long crossfadingSamplesRead = crossfadingBytesRead / ( channels * 4 );

					if ( s_ItemIsFadingToNext == m_CurrentItemCrossfading.ID ) {
						// Fade to next track.
						const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
						const float currentPos = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) );
						if ( ( currentPos > m_FadeOutStartPosition ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
							if ( ( currentPos - m_FadeOutStartPosition ) > s_FadeOutDuration ) {
								crossfadingSamplesRead = 0;
							} else {
								const float fadeOutEndPosition = m_FadeOutStartPosition + s_FadeOutDuration;
								for ( long sampleIndex = 0; sampleIndex < crossfadingSamplesRead; sampleIndex++ ) {
									const float pos = static_cast<float>( sampleIndex ) / samplerate;			
									float scale = static_cast<float>( fadeOutEndPosition - currentPos - pos ) / s_FadeOutDuration;
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
						const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
						const float trackPos = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) ) - m_LastTransitionPosition;
						if ( ( crossfadingBytesRead > 0 ) && ( trackPos < s_FadeOutDuration ) ) {				
							for ( long sampleIndex = 0; sampleIndex < crossfadingSamplesRead; sampleIndex++ ) {
								const float pos = static_cast<float>( sampleIndex ) / samplerate;			
								float scale = static_cast<float>( s_FadeOutDuration - trackPos - pos ) / s_FadeOutDuration;
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
	if ( ( GetFadeOut() || GetFadeToNext() ) && ( 0 != bytesRead ) && ( m_FadeOutStartPosition > 0 ) ) {
		const QWORD bytesPos = BASS_ChannelGetPosition( m_OutputStream, BASS_POS_DECODE );
		const float currentPos = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytesPos ) );
		const long channels = m_DecoderStream->GetChannels();
		const long samplerate = m_DecoderStream->GetSampleRate();
		if ( ( currentPos > m_FadeOutStartPosition ) && ( channels > 0 ) && ( samplerate > 0 ) ) {
			if ( ( currentPos - m_FadeOutStartPosition ) > s_FadeOutDuration ) {
				bytesRead = 0;
			} else {
				const long sampleCount = static_cast<long>( bytesRead ) / ( channels * 4 );
				const float fadeOutEndPosition = m_FadeOutStartPosition + s_FadeOutDuration;
				for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
					const float pos = static_cast<float>( sampleIndex ) / samplerate;			
					float scale = static_cast<float>( fadeOutEndPosition - currentPos - pos ) / s_FadeOutDuration;
					if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
						scale = 0;
					}
					for ( long channel = 0; channel < channels; channel++ ) {
						buffer[ sampleIndex * channels + channel ] *= scale;
					}
				}

				if ( GetFadeToNext() && ( currentPos > ( m_FadeOutStartPosition + s_FadeToNextDuration ) ) ) {
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
		if ( 0 != m_OutputStream ) {
			if ( m_Muted ) {
				m_Muted = false;
			}
			BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_VOL, m_Volume );
		}
	}
}

void Output::GetLevels( float& left, float& right )
{
	left = 0.0f;
	right = 0.0f;
	if ( 0 != m_OutputStream ) {
		float levels[ 2 ] = {};
		const float length = 0.05f;
		if ( TRUE == BASS_ChannelGetLevelEx( m_OutputStream, levels, length, BASS_LEVEL_STEREO ) ) {
			left = levels[ 0 ];
			right = levels[ 1 ];
		}
	}
}

void Output::GetSampleData( const long sampleCount, std::vector<float>& samples, long& channels )
{
	BASS_CHANNELINFO channelInfo = {};
	if ( GetState() != State::Stopped ) {
		BASS_ChannelGetInfo( m_OutputStream, &channelInfo );
	}
	channels = static_cast<long>( channelInfo.chans );
	if ( ( channels > 0 ) && ( sampleCount > 0 ) ) {
		const long sampleSize = channels * sampleCount;
		samples.resize( sampleSize );
		BASS_ChannelGetData( m_OutputStream, &samples[ 0 ], sampleSize * sizeof( float ) );
	}
}

void Output::GetFFTData( std::vector<float>& fft )
{
	fft.resize( 2048 );
	if ( BASS_ChannelGetData( m_OutputStream, &fft[ 0 ], BASS_DATA_FLOAT | BASS_DATA_FFT4096 ) < 0 ) {
		fft.clear();
	}
}

void Output::RestartPlayback()
{
	if ( m_RestartItem.ID > 0 ) {
		if ( GetStopAtTrackEnd() ) {
			ToggleStopAtTrackEnd();
			m_RestartItem = {};
		} else {
			PostMessage( m_Parent, MSG_RESTARTPLAYBACK, m_RestartItem.ID, NULL /*lParam*/ );
		}
	}
}

bool Output::GetRandomPlay() const
{
	return m_RandomPlay;
}

void Output::SetRandomPlay( const bool enabled )
{
	m_RandomPlay = enabled;
	if ( m_RandomPlay ) {
		m_RepeatTrack = m_RepeatPlaylist = false;
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
				CalculateCrossfadePoint( item.PlaylistItem.Info.GetFilename(), item.InitialSeek );
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

Output::Devices Output::GetDevices() const
{
	Devices devices;
	BASS_DEVICEINFO deviceInfo = {};
	DWORD deviceID = 0;
	while( TRUE == BASS_GetDeviceInfo( ++deviceID, &deviceInfo ) ) {
		devices.insert( Devices::value_type( deviceID, AnsiCodePageToWideString( deviceInfo.name ) ) );
	}
	return devices;
}

std::wstring Output::GetCurrentDevice() const
{
	std::wstring name;
	const DWORD deviceNum = BASS_GetDevice();
	BASS_DEVICEINFO deviceInfo = {};
	if ( TRUE == BASS_GetDeviceInfo( deviceNum, &deviceInfo ) ) {
		name = AnsiCodePageToWideString( deviceInfo.name );
	}
	return name;
}

void Output::SettingsChanged()
{
	const std::wstring currentDevice = GetCurrentDevice();
	const std::wstring settingsDevice = m_Settings.GetOutputDevice();

	bool reinitialise = false;
	if ( settingsDevice.empty() ) {
		reinitialise = !m_UsingDefaultDevice;
	} else {
		reinitialise = ( currentDevice != settingsDevice ) || m_UsingDefaultDevice;
	}

	if ( reinitialise ) {
		Stop();
		BASS_Free();
		InitialiseBass();
	}

	Settings::ReplaygainMode replaygainMode = Settings::ReplaygainMode::Disabled;
	float replaygainPreamp = 0;
	bool replaygainHardlimit = false;
	m_Settings.GetReplaygainSettings( replaygainMode, replaygainPreamp, replaygainHardlimit );
	if ( ( replaygainMode != m_ReplaygainMode ) || ( replaygainPreamp != m_ReplaygainPreamp ) || ( replaygainHardlimit != m_ReplaygainHardlimit ) ) {
		m_ReplaygainMode = replaygainMode;
		m_ReplaygainPreamp = replaygainPreamp;
		m_ReplaygainHardlimit = replaygainHardlimit;
		EstimateReplayGain( m_CurrentItemDecoding );
	}
}

void Output::InitialiseBass()
{
	BASS_SetConfig( BASS_CONFIG_BUFFER, static_cast<DWORD>( s_BufferLength * 1000 ) );
	int deviceNum = -1;
	const std::wstring deviceName = m_Settings.GetOutputDevice();
	if ( !deviceName.empty() ) {
		const Devices devices = GetDevices();
		for ( const auto& device : devices ) {
			if ( device.second == deviceName ) {
				deviceNum = device.first;
				break;
			}
		}
		if ( -1 == deviceNum ) {
			m_Settings.SetOutputDevice( std::wstring() );
		}
	}
	m_UsingDefaultDevice = ( -1 == deviceNum );
	BASS_Init( deviceNum, 48000 /*freq*/, 0 /*flags*/, m_Parent /*hwnd*/, NULL /*dsGUID*/ );
}

void Output::EstimateReplayGain( Playlist::Item& item )
{
	if ( Settings::ReplaygainMode::Disabled != m_ReplaygainMode ) {
		float gain = item.Info.GetGainAlbum();
		if ( ( REPLAYGAIN_NOVALUE == gain ) || ( Settings::ReplaygainMode::Track == m_ReplaygainMode ) ) {
			gain = item.Info.GetGainTrack();
		}
		if ( REPLAYGAIN_NOVALUE == gain ) {
			const auto estimateIter = m_ReplayGainEstimateMap.find( item.ID );
			if ( m_ReplayGainEstimateMap.end() != estimateIter ) {
				item.Info.SetGainTrack( estimateIter->second );
			} else {
				Decoder::Ptr tempDecoder = m_Handlers.OpenDecoder( item.Info.GetFilename() );
				if ( tempDecoder ) {
					const float trackGain = tempDecoder->CalculateTrackGain( s_ReplayGainPrecalcTime );
					item.Info.SetGainTrack( trackGain );
					m_ReplayGainEstimateMap.insert( ReplayGainEstimateMap::value_type( item.ID, trackGain ) );
				}
			}
		}
	}
}

void Output::CalculateCrossfadePoint( const std::wstring& filename, const float seekOffset )
{
	StopCrossfadeThread();
	ResetEvent( m_CrossfadeStopEvent );
	m_CrossfadeFilename = filename;
	m_CrossfadeSeekOffset = seekOffset;
	m_CrossfadeThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, CrossfadeThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
}

void Output::OnCalculateCrossfadeHandler()
{
	Decoder::Ptr decoder = m_Handlers.OpenDecoder( m_CrossfadeFilename );
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
	m_CrossfadeFilename = std::wstring();
}

float Output::GetCrossfadePosition() const
{
	return m_CrossfadePosition.load();
}

void Output::SetCrossfadePosition( const float position )
{
	m_CrossfadePosition.store( position );
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
	if ( 0 != m_OutputStream ) {
		BASS_ChannelSetAttribute( m_OutputStream, BASS_ATTRIB_VOL, m_Muted ? 0 : m_Volume );
	}
}

bool Output::GetMuted() const
{
	return m_Muted;
}

void Output::ToggleFadeOut()
{
	m_FadeOut = !m_FadeOut;
	if ( m_FadeOut && ( 0 != m_OutputStream ) ) {
		const QWORD bytePos = BASS_ChannelGetPosition( m_OutputStream , BASS_POS_DECODE );
		m_FadeOutStartPosition = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) );
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
		const QWORD bytePos = BASS_ChannelGetPosition( m_OutputStream , BASS_POS_DECODE );
		m_FadeOutStartPosition = static_cast<float>( BASS_ChannelBytes2Seconds( m_OutputStream, bytePos ) );
	} else {
		m_SwitchToNext = false;
		if ( m_CrossfadingStream ) {
			m_CrossfadingStream.reset();
			m_CurrentItemCrossfading = {};
		}
	}
}

bool Output::GetFadeToNext() const
{
	return m_FadeToNext;
}

void Output::ApplyReplayGain( float* buffer, const long bufferSize, const Playlist::Item& item )
{
	if ( ( Settings::ReplaygainMode::Disabled != m_ReplaygainMode ) && ( 0 != bufferSize ) ) {
		float gain = item.Info.GetGainAlbum();
		if ( ( REPLAYGAIN_NOVALUE == gain ) || ( Settings::ReplaygainMode::Track == m_ReplaygainMode ) ) {
			gain = item.Info.GetGainTrack();
		}
		if ( REPLAYGAIN_NOVALUE != gain ) {
			if ( gain < s_ReplayGainMin ) {
				gain = s_ReplayGainMin;
			} else if ( gain > s_ReplayGainMax ) {
				gain = s_ReplayGainMax;
			}
			const float scale = powf( 10.0f, m_ReplaygainPreamp / 20.0f ) * powf( 10.0f, gain / 20.0f );
			const long sampleCount = bufferSize / 4;
			for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
				buffer[ sampleIndex ] *= scale;
				if ( m_ReplaygainHardlimit ) {
					if ( buffer[ sampleIndex ] < -1.0f ) {
						buffer[ sampleIndex ] = -1.0f;
					} else if ( buffer[ sampleIndex ] > 1.0f ) {
						buffer[ sampleIndex ] = 1.0f;
					}
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
