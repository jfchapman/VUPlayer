#include "Converter.h"

#include "resource.h"
#include "Utility.h"
#include "WndTaskbar.h"
#include "Resampler.h"

#include "ebur128.h"

#include <iomanip>
#include <sstream>
#include <array>
#include <tuple>

// Timer ID.
constexpr long s_TimerID = 1212;

// Timer interval in milliseconds.
constexpr long s_TimerInterval = 250;

// Message ID sent when the conversion has finished.
// 'wParam' - boolean to indicate whether conversion was successful.
// 'lParam' - unused.
constexpr UINT MSG_CONVERTERFINISHED = WM_APP + 90;

INT_PTR CALLBACK Converter::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG: {
			Converter* converter = reinterpret_cast<Converter*>( lParam );
			if ( nullptr != converter ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				converter->OnInitDialog( hwnd );
				return TRUE;
			}
			break;
		}
		case WM_DESTROY: {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
			break;
		}
		case WM_COMMAND: {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL: {
					Converter* converter = reinterpret_cast<Converter*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != converter ) {
						converter->Close();
					}
					return TRUE;
				}
				default: {
					break;
				}
			}
			break;
		}
		case WM_TIMER: {
			Converter* converter = reinterpret_cast<Converter*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( ( nullptr != converter ) && ( s_TimerID == wParam ) ) {
				converter->UpdateStatus();
			}
			break;
		}
		case MSG_CONVERTERFINISHED: {
			Converter* converter = reinterpret_cast<Converter*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != converter ) {
				const bool success = wParam;
				if ( success ) {
					converter->Close();
				} else {
					converter->Error();
				}
			}
			break;
		}
		default: {
			break;
		}
	}
	return FALSE;
}

DWORD WINAPI Converter::EncodeThreadProc( LPVOID lpParam )
{
	Converter* converter = reinterpret_cast<Converter*>( lpParam );
	if ( nullptr != converter ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		converter->EncodeHandler();
		CoUninitialize();
	}
	return 0;
}

Converter::Converter( const HINSTANCE instance, const HWND parent, Library& library, Settings& settings, Handlers& handlers, Playlist::Items& tracks, const Handler::Ptr encoderHandler, const std::wstring& joinFilename, WndTaskbar& taskbar ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Library( library ),
	m_Settings( settings ),
	m_Handlers( handlers ),
	m_Taskbar( taskbar ),
	m_Tracks( tracks ),
	m_CancelEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_EncodeThread( nullptr ),
	m_StatusTrack( joinFilename.empty() ? 0 : 1 ),
	m_ProgressTrack( 0 ),
	m_ProgressTotal( 0 ),
	m_ProgressRange( 100 ),
	m_DisplayedTrack( -1 ),
	m_EncoderHandler( encoderHandler ),
	m_EncoderSettings( m_Settings.GetEncoderSettings( encoderHandler->GetDescription() ) ),
	m_JoinFilename( joinFilename ),
	m_EncoderThreadCount( joinFilename.empty() ? std::min<uint32_t>( kEncoderThreadLimit, std::max<uint32_t>( 1, std::thread::hardware_concurrency() ) ) : 0 )
{
	if ( joinFilename.empty() ) {
		// Remove duplicate tracks.
		std::set<MediaInfo> uniqueMedia;
		for ( auto track = m_Tracks.begin(); m_Tracks.end() != track; ) {
			if ( uniqueMedia.insert( track->Info ).second )
				++track;
			else
				track = m_Tracks.erase( track );
		}
	}
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT_PROGRESS ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

Converter::~Converter()
{
}

void Converter::OnInitDialog( const HWND hwndDialog )
{
	m_hWnd = hwndDialog;
	CentreDialog( m_hWnd );

	const int bufferSize = 64;
	WCHAR buffer[ bufferSize ] = {};
	LoadString( m_hInst, m_JoinFilename.empty() ? IDS_CONVERT_TITLE : IDS_JOIN_TITLE, buffer, bufferSize );
	SetWindowText( m_hWnd, buffer );

	const HWND progressTrack = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressTrack ) {
		RECT progressRect = {};
		GetClientRect( progressTrack, &progressRect );
		if ( ( progressRect.right - progressRect.left ) > 0 ) {
			m_ProgressRange = progressRect.right - progressRect.left;
			SendMessage( progressTrack, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
		}
		if ( m_EncoderThreadCount > 0 ) {
			// Instead of a single track progress bar, set up multiple progress bars for each encoder thread.
			ShowWindow( progressTrack, SW_HIDE );

			constexpr int kMaxBarsPerRow = 8;
			const int rows = static_cast<int>( std::ceil( static_cast<float>( m_EncoderThreadCount ) / kMaxBarsPerRow ) );
			const int columns = static_cast<int>( std::ceil( static_cast<float>( m_EncoderThreadCount ) / rows ) );

			const int progressHeight = progressRect.bottom - progressRect.top;
			const int padding = progressHeight / 3;

			if ( const int expandDialog = ( progressHeight + padding ) * ( rows - 1 ); expandDialog > 0 ) {
				// Expand dialog vertically to accomodate extra progress bars.
				RECT dialogRect = {};
				GetWindowRect( m_hWnd, &dialogRect );
				const int cx = dialogRect.right - dialogRect.left;
				const int cy = dialogRect.bottom - dialogRect.top + expandDialog;
				SetWindowPos( m_hWnd, nullptr, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE );

				// Shift the other dialog controls down.
				std::array controls = {
					GetDlgItem( m_hWnd, IDCANCEL ),
					GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL ),
					GetDlgItem( m_hWnd, IDC_EXTRACT_STATE_ENCODER )
				};
				for ( const auto control : controls ) {
					RECT controlRect = {};
					GetWindowRect( control, &controlRect );
					POINT origin = { controlRect.left, controlRect.top };
					MapWindowPoints( nullptr, m_hWnd, &origin, 1 );
					const int x = origin.x;
					const int y = origin.y + expandDialog;
					SetWindowPos( control, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOACTIVATE );
				}
			}

			const int widthPerRow = progressRect.right - progressRect.left - padding * ( columns - 1 );
			const int progressWidth = widthPerRow / columns;
			GetWindowRect( progressTrack, &progressRect );
			POINT origin = { progressRect.left, progressRect.top };
			MapWindowPoints( nullptr, m_hWnd, &origin, 1 );

			const int progressBarCount = static_cast<int>( m_EncoderThreadCount );
			int progressBarIndex = 0;
			for ( int row = 0; ( row < rows ) && ( progressBarIndex < progressBarCount ); row++ ) {
				const int y = origin.y + row * ( progressHeight + padding );
				for ( int col = 0; ( col < columns ) && ( progressBarIndex < progressBarCount ); col++, progressBarIndex++ ) {
					const int x = origin.x + col * ( progressWidth + padding );
					if ( const HWND hwndProgress = CreateWindowEx( 0, PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE, x, y, progressWidth, progressHeight, m_hWnd, nullptr, m_hInst, nullptr ); nullptr != hwndProgress ) {
						if ( 0 == m_EncoderProgressRange ) {
							GetClientRect( hwndProgress, &progressRect );
							m_EncoderProgressRange = progressRect.right - progressRect.left;
						}
						SendMessage( hwndProgress, PBM_SETRANGE, 0, MAKELONG( 0, m_EncoderProgressRange ) );
						m_EncoderProgressBars[ progressBarIndex ].first = hwndProgress;
					}
				}
			}

			if ( const HWND progressTotal = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL ); nullptr != progressTotal ) {
				RECT controlRect = {};
				GetWindowRect( progressTotal, &controlRect );
				const int cx = progressWidth * columns + padding * ( columns - 1 );
				const int cy = controlRect.bottom - controlRect.top;
				SetWindowPos( progressTotal, nullptr, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
			}
		}
	}

	if ( const HWND progressTotal = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL ); nullptr != progressTotal ) {
		SendMessage( progressTotal, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
	}

	UpdateStatus();

	m_EncodeThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, EncodeThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );

	SetTimer( m_hWnd, s_TimerID, s_TimerInterval, NULL /*timerProc*/ );
}

void Converter::Close()
{
	KillTimer( m_hWnd, s_TimerID );

	SetEvent( m_CancelEvent );
	if ( nullptr != m_EncodeThread ) {
		WaitForSingleObject( m_EncodeThread, INFINITE );
		CloseHandle( m_EncodeThread );
		m_EncodeThread = nullptr;
	}
	CloseHandle( m_CancelEvent );
	m_CancelEvent = nullptr;

	EndDialog( m_hWnd, 0 );
}

void Converter::Error()
{
	KillTimer( m_hWnd, s_TimerID );
	const int bufferSize = 64;
	WCHAR buffer[ bufferSize ] = {};
	LoadString( m_hInst, IDS_CONVERT_ERROR, buffer, bufferSize );
	SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, buffer );
	SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_ENCODER, buffer );

	const HWND progressTrack = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressTrack ) {
		SendMessage( progressTrack, PBM_SETPOS, m_ProgressRange, 0 );
	}

	const HWND progressTotal = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
	if ( nullptr != progressTotal ) {
		SendMessage( progressTotal, PBM_SETPOS, m_ProgressRange, 0 );
	}

	LoadString( m_hInst, IDS_CLOSE, buffer, bufferSize );
	SetDlgItemText( m_hWnd, IDCANCEL, buffer );
}

bool Converter::Cancelled() const
{
	const bool cancelled = ( WAIT_OBJECT_0 == WaitForSingleObject( m_CancelEvent, 0 ) );
	return cancelled;
}

bool Converter::JoinTracksHandler( const bool addToLibrary )
{
	if ( m_JoinFilename.empty() || m_Tracks.empty() || !m_EncoderHandler )
		return false;

	const auto encoder = m_EncoderHandler->OpenEncoder();
	if ( !encoder )
		return false;

	float totalDuration = 0;
	for ( const auto& track : m_Tracks ) {
		totalDuration += track.Info.GetDuration();
	}
	float totalEncodedDuration = 0;

	long joinChannels = 0;
	long joinSampleRate = 0;
	std::optional<long> joinBPS;

	// Resample tracks to the most common sample format.
	std::map<std::tuple<long /*sampleRate*/, long /*channels*/, std::optional<long> /*bps*/>, uint32_t /*count*/> sampleFormats;
	for ( const auto& track : m_Tracks ) {
		const Decoder::Ptr decoder = OpenDecoder( track );
		if ( decoder ) {
			auto sampleFormat = sampleFormats.insert( { std::make_tuple( decoder->GetSampleRate(), decoder->GetChannels(), decoder->GetBPS() ), 0 } ).first;
			sampleFormat->second++;
		}
	}
	uint32_t currentHighestCount = 0;
	for ( auto sampleFormat = sampleFormats.rbegin(); sampleFormats.rend() != sampleFormat; ++sampleFormat ) {
		const auto [sampleRate, channels, bps] = sampleFormat->first;
		const auto count = sampleFormat->second;
		if ( count > currentHighestCount ) {
			joinSampleRate = sampleRate;
			joinChannels = channels;
			joinBPS = bps;
			currentHighestCount = count;
		}
	}
	const long long totalSamples = static_cast<long long>( totalDuration * joinSampleRate );
	if ( ( joinSampleRate <= 0 ) || ( joinChannels <= 0 ) || !encoder->Open( m_JoinFilename, joinSampleRate, joinChannels, joinBPS, totalSamples, m_EncoderSettings, {} /*tags*/ ) )
		return false;

	// Decode each track and feed it to the encoder.
	ebur128_state* r128State = ebur128_init( static_cast<unsigned int>( joinChannels ), static_cast<unsigned int>( joinSampleRate ), EBUR128_MODE_I );
	long currentTrack = 1;
	bool conversionOK = true;
	for ( auto track = m_Tracks.begin(); conversionOK && !Cancelled() && ( m_Tracks.end() != track ); track++, currentTrack++ ) {
		m_ProgressTrack.store( 0 );
		m_StatusTrack.store( currentTrack );
		try {
			const Decoder::Ptr decoder = OpenDecoder( *track );
			const auto resampler = std::make_unique<Resampler>( decoder, 0, joinSampleRate, joinChannels );

			const long sampleRate = resampler->GetOutputSampleRate();
			const long channels = resampler->GetOutputChannels();
			const auto bps = decoder->GetBPS();
			const long long trackSamplesTotal = static_cast<long long>( track->Info.GetDuration() * sampleRate );

			long long trackSamplesRead = 0;

			const long sampleCount = 65536;
			std::vector<float> sampleBuffer( sampleCount * channels );

			int r128Error = EBUR128_SUCCESS;
			bool continueEncoding = true;
			while ( !Cancelled() && continueEncoding ) {
				const long samplesRead = resampler->Read( sampleBuffer.data(), sampleCount );
				if ( samplesRead > 0 ) {
					if ( ( nullptr != r128State ) && ( EBUR128_SUCCESS == r128Error ) ) {
						r128Error = ebur128_add_frames_float( r128State, sampleBuffer.data(), static_cast<size_t>( samplesRead ) );
					}
					continueEncoding = encoder->Write( sampleBuffer.data(), samplesRead );

					trackSamplesRead += samplesRead;
					if ( 0 != trackSamplesTotal ) {
						m_ProgressTrack.store( static_cast<float>( trackSamplesRead ) / trackSamplesTotal );
					}

					totalEncodedDuration += static_cast<float>( samplesRead ) / sampleRate;
					if ( 0 != totalDuration ) {
						m_ProgressTotal.store( static_cast<float>( totalEncodedDuration ) / totalDuration );
					}
				} else {
					continueEncoding = false;
				}
			}
		} catch ( const std::runtime_error& ) {
			conversionOK = false;
		}
	}

	// Write any common tags and update library.
	encoder->Close();
	if ( conversionOK && !Cancelled() ) {
		MediaInfo joinedMediaInfo;

		MediaInfo::List mediaList;
		for ( const auto& item : m_Tracks ) {
			mediaList.push_back( item.Info );
		}

		MediaInfo::GetCommonInfo( mediaList, joinedMediaInfo );
		joinedMediaInfo.SetFilename( m_JoinFilename );

		if ( nullptr != r128State ) {
			double loudness = 0;
			if ( EBUR128_SUCCESS == ebur128_loudness_global( r128State, &loudness ) ) {
				const float trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
				joinedMediaInfo.SetGainTrack( trackGain );
			}
		}

		WriteTrackTags( joinedMediaInfo.GetFilename(), joinedMediaInfo );
		if ( addToLibrary || m_Library.GetMediaInfo( joinedMediaInfo, false /*scanMedia*/ ) ) {
			m_Library.GetMediaInfo( joinedMediaInfo );
		}
	}
	if ( nullptr != r128State ) {
		ebur128_destroy( &r128State );
	}

	return conversionOK;
}

bool Converter::EncodeTracksHandler( const bool addToLibrary )
{
	if ( m_Tracks.empty() || !m_EncoderHandler )
		return false;

	// Group tracks based on their folder location, and make a note of all input filenames (lowercase, without extensions) for later.
	std::map<std::filesystem::path, Playlist::Items> groups;
	std::set<std::wstring> usedFilenamesWithExtensions;
	float totalDuration = 0;
	for ( const auto& track : m_Tracks ) {
		const std::filesystem::path inputFilename = std::filesystem::path( track.Info.GetFilename() );
		const auto folder = inputFilename.parent_path();
		auto album = groups.insert( { folder, {} } ).first;
		usedFilenamesWithExtensions.insert( WideStringToLower( inputFilename ) ).second;
		album->second.emplace_back( track );
		totalDuration += track.Info.GetDuration();
	}
	std::set<std::wstring> usedFilenames;
	for ( const auto& filename : usedFilenamesWithExtensions ) {
		usedFilenames.insert( std::filesystem::path( filename ).replace_extension() );
	}

	// If all tracks in a folder have the same (non-empty) album name, encode these tracks in a single batch, so that album gain can be calculated.
	Playlist::Items nonAlbumTracks;
	nonAlbumTracks.reserve( m_Tracks.size() );
	for ( auto& album : groups ) {
		auto& [folder, tracks] = album;
		std::set<std::wstring> albumNames;
		for ( const auto& track : tracks ) {
			albumNames.insert( track.Info.GetAlbum() );
		}
		if ( ( 1 != albumNames.size() ) || albumNames.begin()->empty() ) {
			nonAlbumTracks.insert( nonAlbumTracks.end(), tracks.begin(), tracks.end() );
			tracks.clear();
		}
	}
	groups.emplace( std::filesystem::path(), nonAlbumTracks );

	// Generate unique output filenames, which are also distinct from any input filenames.
	using Album = std::vector<std::pair<Playlist::Item, std::wstring /*outputFilename*/>>;
	std::map<int, Album> albums;
	int albumID = 0;
	for ( auto group = groups.begin(); groups.end() != group; group++, albumID++ ) {
		Album album;
		for ( const auto& track : group->second ) {
			const auto outputFilename = GetOutputFilename( track, usedFilenames );
			if ( outputFilename ) {
				album.push_back( std::make_pair( track, *outputFilename ) );
			}
		}
		albums.emplace( albumID, album );
	}

	// Convert each batch of tracks.
	bool conversionOK = true;
	std::atomic<long> currentTrack = 0;
	std::atomic<float> totalEncodedDuration = 0;
	for ( auto album = albums.begin(); conversionOK && !Cancelled() && ( albums.end() != album ); album++ ) {
		if ( album->second.empty() )
			continue;

		for ( uint32_t i = 0; i < m_EncoderThreadCount; i++ )
			m_EncoderProgressBars[ i ].second.store( 0 );

		// Sort tracks by duration, so that the longest tracks start encoding first (note that the threads pick tracks off the end of the list).
		auto tracks = album->second;
		std::sort( tracks.begin(), tracks.end(), [] ( const std::pair<Playlist::Item, std::wstring>& a, const std::pair<Playlist::Item, std::wstring>& b ) {
			return a.first.Info.GetDuration() < b.first.Info.GetDuration();
			} );

		// Don't calculate album gain for non-album tracks (the album with a zero ID).
		std::vector<ebur128_state*> r128States;
		const bool calculateAlbumGain = ( 0 != album->first );
		if ( calculateAlbumGain ) {
			r128States.reserve( tracks.size() );
		}

		std::mutex trackMutex;
		std::mutex r128Mutex;
		std::mutex encodedMediaMutex;
		MediaInfo::List encodedMediaList;
		const size_t threadCount = std::min<size_t>( tracks.size(), m_EncoderThreadCount );
		std::list<std::thread> threads;
		for ( size_t threadIndex = 0; threadIndex < threadCount; threadIndex++ ) {
			threads.push_back( std::thread( [ &tracks, &trackMutex, &r128States, &r128Mutex, &encodedMediaList, &encodedMediaMutex, &currentTrack, &trackProgress = m_EncoderProgressBars[ threadIndex ].second, &totalEncodedDuration, totalDuration, calculateAlbumGain, addToLibrary, this ] ()
				{
					Playlist::Item item = {};
					std::wstring outputFilename;
					{
						std::lock_guard<std::mutex> lock( trackMutex );
						if ( !tracks.empty() ) {
							const auto track = tracks.back();
							tracks.pop_back();
							item = track.first;
							outputFilename = track.second;
						}
					}
					std::error_code ec;
					while ( !Cancelled() && ( 0 != item.ID ) ) {
						MediaInfo mediaInfo( item.Info );

						// Ensure track information is up to date.
						if ( m_Library.GetMediaInfo( mediaInfo ) ) {
							item.Info = mediaInfo;
						}

						if ( !outputFilename.empty() ) {
							try {
								const Decoder::Ptr decoder = OpenDecoder( item );
								const auto resampler = std::make_unique<Resampler>( decoder, 0, decoder->GetSampleRate(), decoder->GetChannels() );

								const long sampleRate = resampler->GetOutputSampleRate();
								const long channels = resampler->GetOutputChannels();
								const auto bps = decoder->GetBPS();
								const long long trackSamplesTotal = static_cast<long long>( item.Info.GetDuration() * sampleRate );

								auto encoder = m_EncoderHandler->OpenEncoder();
								if ( encoder && encoder->Open( outputFilename, sampleRate, channels, bps, trackSamplesTotal, m_EncoderSettings, m_Library.GetTags( mediaInfo ) ) ) {
									long long trackSamplesRead = 0;

									const long sampleCount = 65536;
									std::vector<float> sampleBuffer( sampleCount * channels );

									ebur128_state* r128State = ebur128_init( static_cast<unsigned int>( channels ), static_cast<unsigned int>( sampleRate ), EBUR128_MODE_I );
									if ( calculateAlbumGain ) {
										std::lock_guard<std::mutex> r128lock( r128Mutex );
										r128States.push_back( r128State );
									}

									int r128Error = EBUR128_SUCCESS;
									bool continueEncoding = true;
									while ( !Cancelled() && continueEncoding ) {
										const long samplesRead = resampler->Read( sampleBuffer.data(), sampleCount );
										if ( samplesRead > 0 ) {
											if ( ( nullptr != r128State ) && ( EBUR128_SUCCESS == r128Error ) ) {
												r128Error = ebur128_add_frames_float( r128State, sampleBuffer.data(), static_cast<size_t>( samplesRead ) );
											}
											continueEncoding = encoder->Write( sampleBuffer.data(), samplesRead );

											trackSamplesRead += samplesRead;
											if ( 0 != trackSamplesTotal ) {
												trackProgress.store( static_cast<float>( trackSamplesRead ) / trackSamplesTotal );
											}

											totalEncodedDuration += static_cast<float>( samplesRead ) / sampleRate;
											if ( 0 != totalDuration ) {
												m_ProgressTotal.store( static_cast<float>( totalEncodedDuration ) / totalDuration );
											}
										} else {
											continueEncoding = false;
										}
									}
									encoder->Close();

									if ( Cancelled() ) {
										std::filesystem::remove( outputFilename, ec );
									} else {
										if ( nullptr != r128State ) {
											double loudness = 0;
											if ( EBUR128_SUCCESS == ebur128_loudness_global( r128State, &loudness ) ) {
												const float trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
												mediaInfo.SetGainTrack( trackGain );
											}
										}

										mediaInfo.SetFilename( outputFilename );
										encodedMediaList.push_back( mediaInfo );

										WriteTrackTags( outputFilename, mediaInfo );
										if ( addToLibrary || m_Library.GetMediaInfo( mediaInfo, false /*scanMedia*/ ) ) {
											MediaInfo extractedMediaInfo( outputFilename );
											m_Library.GetMediaInfo( extractedMediaInfo );
										}
									}
									if ( !calculateAlbumGain && ( nullptr != r128State ) ) {
										ebur128_destroy( &r128State );
										r128State = nullptr;
									}
								}
							} catch ( const std::runtime_error& ) {
							}
						}
						m_StatusTrack.store( ++currentTrack );

						item = {};
						outputFilename.clear();
						std::lock_guard<std::mutex> lock( trackMutex );
						if ( !tracks.empty() ) {
							const auto track = tracks.back();
							tracks.pop_back();
							item = track.first;
							outputFilename = track.second;
						}
					}
				} ) );
		}
		for ( auto& thread : threads ) {
			thread.join();
		}

		// Write album gain for all tracks in this batch.
		if ( calculateAlbumGain && conversionOK && !Cancelled() && !encodedMediaList.empty() ) {
			std::optional<float> albumGain;
			if ( !r128States.empty() ) {
				double loudness = 0;
				if ( EBUR128_SUCCESS == ebur128_loudness_global_multiple( r128States.data(), r128States.size(), &loudness ) ) {
					albumGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
				}
			}
			if ( albumGain.has_value() ) {
				encodedMediaList.sort();
				for ( auto& encodedMedia : encodedMediaList ) {
					encodedMedia.SetGainAlbum( albumGain );
					WriteAlbumTags( encodedMedia.GetFilename(), encodedMedia );
					MediaInfo info( encodedMedia.GetFilename() );
					if ( addToLibrary || m_Library.GetMediaInfo( info, false /*scanMedia*/ ) ) {
						m_Library.GetMediaInfo( info );
					}
				}
			}
		}
		for ( const auto& iter : r128States ) {
			ebur128_state* state = iter;
			ebur128_destroy( &state );
		}
		r128States.clear();
	}

	return conversionOK;
}

std::optional<std::wstring> Converter::GetOutputFilename( const Playlist::Item& track, std::set<std::wstring>& usedFilenames )
{
	const std::wstring originalFilename = m_Settings.GetOutputFilename( track.Info, m_hInst );
	std::wstring filename = originalFilename;
	std::wstring filenameLowerCase = WideStringToLower( filename );
	constexpr int kMaxRenameAttempts = 100;
	int index = 1;
	do {
		if ( const auto found = usedFilenames.find( filenameLowerCase ); usedFilenames.end() == found ) {
			usedFilenames.insert( filenameLowerCase );
			return filename;
		}
		filename = originalFilename + L" (" + std::to_wstring( index ) + L")";
		filenameLowerCase = WideStringToLower( filename );
	} while ( ++index < kMaxRenameAttempts );
	return std::nullopt;
}

void Converter::EncodeHandler()
{
	std::wstring extractFolder;
	std::wstring extractFilename;
	bool addToLibrary = false;
	bool joinTracks = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, addToLibrary, joinTracks );
	const bool conversionOK = joinTracks ? JoinTracksHandler( addToLibrary ) : EncodeTracksHandler( addToLibrary );
	if ( !Cancelled() ) {
		PostMessage( m_hWnd, MSG_CONVERTERFINISHED, conversionOK, 0 );
	}
}

void Converter::UpdateStatus()
{
	const long currentTrack = m_StatusTrack.load();
	if ( currentTrack != m_DisplayedTrack ) {
		m_DisplayedTrack = currentTrack;
		const int bufSize = 128;
		WCHAR buffer[ bufSize ];
		LoadString( m_hInst, ( m_EncoderThreadCount > 0 ) ? IDS_CONVERT_STATUS_TRACK : IDS_JOIN_STATUS_TRACK, buffer, bufSize );
		std::wstring trackStatus( buffer );
		trackStatus += L":";
		WideStringReplace( trackStatus, L"%1", std::to_wstring( m_DisplayedTrack ) );
		WideStringReplace( trackStatus, L"%2", std::to_wstring( m_Tracks.size() ) );
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, trackStatus.c_str() );
	}

	if ( m_EncoderThreadCount > 0 ) {
		// Update all the encoder progress bars.
		for ( uint32_t i = 0; i < m_EncoderThreadCount; i++ ) {
			auto& [hwndProgress, progress] = m_EncoderProgressBars[ i ];
			if ( nullptr != hwndProgress ) {
				const long displayPosition = static_cast<long>( SendMessage( hwndProgress, PBM_GETPOS, 0, 0 ) );
				const long currentPosition = static_cast<long>( progress * m_EncoderProgressRange );
				if ( currentPosition != displayPosition ) {
					SendMessage( hwndProgress, PBM_SETPOS, currentPosition, 0 );
				}
			}
		}
	} else {
		// Update the single track progress bar.
		if ( const HWND hwndProgress = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK ); nullptr != hwndProgress ) {
			const long displayPosition = static_cast<long>( SendMessage( hwndProgress, PBM_GETPOS, 0, 0 ) );
			const long currentPosition = static_cast<long>( m_ProgressTrack * m_ProgressRange );
			if ( currentPosition != displayPosition ) {
				SendMessage( hwndProgress, PBM_SETPOS, currentPosition, 0 );
			}
		}
	}

	const HWND progressTotal = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
	if ( nullptr != progressTotal ) {
		const long displayPosition = static_cast<long>( SendMessage( progressTotal, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressTotal * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressTotal, PBM_SETPOS, currentPosition, 0 );
		}
	}

	m_Taskbar.UpdateProgress( m_ProgressTotal );
}

void Converter::WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo )
{
	Tags tags;
	const std::wstring& title = mediaInfo.GetTitle();
	if ( !title.empty() ) {
		tags.insert( Tags::value_type( Tag::Title, WideStringToUTF8( title ) ) );
	}
	const std::wstring& artist = mediaInfo.GetArtist();
	if ( !artist.empty() ) {
		tags.insert( Tags::value_type( Tag::Artist, WideStringToUTF8( artist ) ) );
	}
	const std::wstring& album = mediaInfo.GetAlbum();
	if ( !album.empty() ) {
		tags.insert( Tags::value_type( Tag::Album, WideStringToUTF8( album ) ) );
	}
	const std::wstring& genre = mediaInfo.GetGenre();
	if ( !genre.empty() ) {
		tags.insert( Tags::value_type( Tag::Genre, WideStringToUTF8( genre ) ) );
	}
	const std::wstring& comment = mediaInfo.GetComment();
	if ( !comment.empty() ) {
		tags.insert( Tags::value_type( Tag::Comment, WideStringToUTF8( comment ) ) );
	}
	const std::wstring& composer = mediaInfo.GetComposer();
	if ( !composer.empty() ) {
		tags.insert( Tags::value_type( Tag::Composer, WideStringToUTF8( composer ) ) );
	}
	const std::wstring& conductor = mediaInfo.GetConductor();
	if ( !conductor.empty() ) {
		tags.insert( Tags::value_type( Tag::Conductor, WideStringToUTF8( conductor ) ) );
	}
	const std::wstring& publisher = mediaInfo.GetPublisher();
	if ( !publisher.empty() ) {
		tags.insert( Tags::value_type( Tag::Publisher, WideStringToUTF8( publisher ) ) );
	}
	const std::string track = ( mediaInfo.GetTrack() > 0 ) ? std::to_string( mediaInfo.GetTrack() ) : std::string();
	if ( !track.empty() ) {
		tags.insert( Tags::value_type( Tag::Track, track ) );
	}
	const std::string year = ( mediaInfo.GetYear() > 0 ) ? std::to_string( mediaInfo.GetYear() ) : std::string();
	if ( !year.empty() ) {
		tags.insert( Tags::value_type( Tag::Year, year ) );
	}
	const std::string gainTrack = GainToString( m_hInst, mediaInfo.GetGainTrack() );
	if ( !gainTrack.empty() ) {
		tags.insert( Tags::value_type( Tag::GainTrack, gainTrack ) );
	}
	const std::vector<BYTE> imageBytes = m_Library.GetMediaArtwork( mediaInfo );
	if ( !imageBytes.empty() ) {
		const std::string encodedArtwork = Base64Encode( imageBytes.data(), static_cast<int>( imageBytes.size() ) );
		tags.insert( Tags::value_type( Tag::Artwork, encodedArtwork ) );
	}

	if ( !tags.empty() ) {
		m_EncoderHandler->SetTags( filename, tags );
	}
}

void Converter::WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo )
{
	Tags tags;
	const std::string gainAlbum = GainToString( m_hInst, mediaInfo.GetGainAlbum() );
	if ( !gainAlbum.empty() ) {
		tags.insert( Tags::value_type( Tag::GainAlbum, gainAlbum ) );
	}

	if ( !tags.empty() ) {
		m_EncoderHandler->SetTags( filename, tags );
	}
}

Decoder::Ptr Converter::OpenDecoder( const Playlist::Item& item ) const
{
	Decoder::Ptr decoder = m_Handlers.OpenDecoder( item.Info, Decoder::Context::Input );
	if ( !decoder ) {
		auto duplicate = item.Duplicates.begin();
		while ( !decoder && ( item.Duplicates.end() != duplicate ) ) {
			MediaInfo duplicateInfo( item.Info );
			duplicateInfo.SetFilename( *duplicate );
			decoder = m_Handlers.OpenDecoder( duplicateInfo, Decoder::Context::Input );
			++duplicate;
		}
	}
	return decoder;
}
