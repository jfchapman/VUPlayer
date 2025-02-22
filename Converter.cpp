#include "Converter.h"

#include "resource.h"
#include "Utility.h"
#include "WndTaskbar.h"

#include "ebur128.h"

#include <iomanip>
#include <sstream>

// Timer ID.
static const long s_TimerID = 1212;

// Timer interval in milliseconds.
static const long s_TimerInterval = 250;

// Message ID sent when the conversion has finished.
// 'wParam' - boolean to indicate whether conversion was successful.
// 'lParam' - unused.
static const UINT MSG_CONVERTERFINISHED = WM_APP + 90;

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
	m_StatusTrack( 1 ),
	m_ProgressTrack( 0 ),
	m_ProgressTotal( 0 ),
	m_ProgressRange( 100 ),
	m_DisplayedTrack( 0 ),
	m_TrackCount( static_cast<long>( tracks.size() ) ),
	m_EncoderHandler( encoderHandler ),
	m_Encoder( encoderHandler ? encoderHandler->OpenEncoder() : nullptr ),
	m_EncoderSettings( m_Encoder ? m_Settings.GetEncoderSettings( encoderHandler->GetDescription() ) : std::string() ),
	m_JoinFilename( joinFilename )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT_PROGRESS ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

Converter::~Converter()
{
}

void Converter::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );

	const int bufferSize = 64;
	WCHAR buffer[ bufferSize ] = {};
	LoadString( m_hInst, IDS_CONVERT_TITLE, buffer, bufferSize );
	SetWindowText( m_hWnd, buffer );

	const HWND progressTrack = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressTrack ) {
		RECT rect = {};
		GetClientRect( progressTrack, &rect );
		if ( ( rect.right - rect.left ) > 0 ) {
			m_ProgressRange = rect.right - rect.left;
			SendMessage( progressTrack, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
		}
	}

	const HWND progressTotal = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
	if ( nullptr != progressTotal ) {
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

void Converter::EncodeHandler()
{
	bool conversionOK = m_Encoder && !m_Tracks.empty();
	if ( conversionOK ) {
		float totalDuration = 0;
		for ( const auto& track : m_Tracks ) {
			totalDuration += track.Info.GetDuration();
		}
		float totalEncodedDuration = 0;
		MediaInfo::List encodedMediaList;

		std::wstring extractFolder;
		std::wstring extractFilename;
		bool extractToLibrary = false;
		bool extractJoin = false;
		m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary, extractJoin );

		std::vector<ebur128_state*> r128States;
		if ( !extractJoin ) {
			r128States.reserve( m_Tracks.size() );
		}
		ebur128_state* r128State = nullptr;

		long joinChannels = 0;
		long joinSampleRate = 0;
		if ( extractJoin ) {
			const Decoder::Ptr decoder = OpenDecoder( m_Tracks.front() );
			if ( decoder ) {
				joinChannels = decoder->GetChannels();
				joinSampleRate = decoder->GetSampleRate();
				const long long totalSamples = static_cast<long long>( totalDuration * joinSampleRate );
				conversionOK = m_Encoder->Open( m_JoinFilename, joinSampleRate, joinChannels, decoder->GetBPS(), totalSamples, m_EncoderSettings, {} /*tags*/ );
				r128State = ebur128_init( static_cast<unsigned int>( joinChannels ), static_cast<unsigned int>( joinSampleRate ), EBUR128_MODE_I );
				if ( nullptr != r128State ) {
					r128States.push_back( r128State );
				}
			} else {
				conversionOK = false;
			}
		}

		if ( conversionOK ) {
			long currentTrack = 0;
			auto track = m_Tracks.begin();
			while ( conversionOK && !Cancelled() && ( m_Tracks.end() != track ) ) {
				MediaInfo mediaInfo( track->Info );

				// Ensure track information is up to date.
				if ( m_Library.GetMediaInfo( mediaInfo ) ) {
					track->Info = mediaInfo;
				}

				m_ProgressTrack.store( 0 );
				m_StatusTrack.store( ++currentTrack );
				std::wstring filename = extractJoin ? m_JoinFilename : m_Settings.GetOutputFilename( track->Info, m_hInst );
				conversionOK = !filename.empty();
				if ( conversionOK ) {
					const Decoder::Ptr decoder = OpenDecoder( *track );
					if ( extractJoin ) {
						conversionOK = static_cast<bool>( decoder );
					}
					if ( decoder ) {
						const long sampleRate = decoder->GetSampleRate();
						const long channels = decoder->GetChannels();
						const auto bps = decoder->GetBPS();
						const long long trackSamplesTotal = static_cast<long long>( track->Info.GetDuration() * sampleRate );

						conversionOK = extractJoin ? ( ( sampleRate == joinSampleRate ) && ( channels == joinChannels ) ) :
							m_Encoder->Open( filename, sampleRate, channels, bps, trackSamplesTotal, m_EncoderSettings, m_Library.GetTags( mediaInfo ) );

						if ( conversionOK ) {
							long long trackSamplesRead = 0;

							const long sampleCount = 65536;
							std::vector<float> sampleBuffer( sampleCount * channels );

							if ( !extractJoin ) {
								r128State = ebur128_init( static_cast<unsigned int>( channels ), static_cast<unsigned int>( sampleRate ), EBUR128_MODE_I );
								if ( nullptr != r128State ) {
									r128States.push_back( r128State );
								}
							}

							int r128Error = EBUR128_SUCCESS;
							bool continueEncoding = true;
							while ( !Cancelled() && continueEncoding ) {
								const long samplesRead = decoder->ReadSamples( sampleBuffer.data(), sampleCount );
								if ( samplesRead > 0 ) {
									if ( ( nullptr != r128State ) && ( EBUR128_SUCCESS == r128Error ) ) {
										r128Error = ebur128_add_frames_float( r128State, sampleBuffer.data(), static_cast<size_t>( samplesRead ) );
									}
									continueEncoding = m_Encoder->Write( sampleBuffer.data(), samplesRead );

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

							if ( !extractJoin ) {
								m_Encoder->Close();

								if ( nullptr != r128State ) {
									double loudness = 0;
									if ( EBUR128_SUCCESS == ebur128_loudness_global( r128State, &loudness ) ) {
										const float trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
										mediaInfo.SetGainTrack( trackGain );
									}
								}

								mediaInfo.SetFilename( filename );
								encodedMediaList.push_back( mediaInfo );

								WriteTrackTags( filename, mediaInfo );
								if ( extractToLibrary || m_Library.GetMediaInfo( mediaInfo, false /*scanMedia*/ ) ) {
									MediaInfo extractedMediaInfo( filename );
									m_Library.GetMediaInfo( extractedMediaInfo );
								}
							}
						}
					}
				}
				++track;
			}

			if ( extractJoin ) {
				m_Encoder->Close();

				if ( conversionOK ) {
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
					if ( extractToLibrary || m_Library.GetMediaInfo( joinedMediaInfo, false /*scanMedia*/ ) ) {
						m_Library.GetMediaInfo( joinedMediaInfo );
					}
				}
			} else {
				if ( conversionOK && !Cancelled() && !encodedMediaList.empty() ) {
					const std::wstring album = encodedMediaList.front().GetAlbum();
					bool writeAlbumGain = !album.empty();
					auto encodedMediaIter = encodedMediaList.begin();
					while ( writeAlbumGain && ( encodedMediaList.end() != ++encodedMediaIter ) ) {
						writeAlbumGain = ( encodedMediaIter->GetAlbum() == album );
					}

					if ( writeAlbumGain ) {
						std::optional<float> albumGain;
						if ( !r128States.empty() ) {
							double loudness = 0;
							if ( EBUR128_SUCCESS == ebur128_loudness_global_multiple( r128States.data(), r128States.size(), &loudness ) ) {
								albumGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
							}
						}

						if ( albumGain.has_value() ) {
							for ( auto& encodedMedia : encodedMediaList ) {
								encodedMedia.SetGainAlbum( albumGain );
								WriteAlbumTags( encodedMedia.GetFilename(), encodedMedia );
								MediaInfo info( encodedMedia.GetFilename() );
								if ( extractToLibrary || m_Library.GetMediaInfo( info, false /*scanMedia*/ ) ) {
									m_Library.GetMediaInfo( info );
								}
							}
						}
					}
				}
			}
		}

		for ( const auto& iter : r128States ) {
			ebur128_state* state = iter;
			ebur128_destroy( &state );
		}
	}

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
		LoadString( m_hInst, IDS_CONVERT_STATUS_TRACK, buffer, bufSize );
		std::wstring trackStatus( buffer );
		trackStatus += L":";
		WideStringReplace( trackStatus, L"%1", std::to_wstring( m_DisplayedTrack ) );
		WideStringReplace( trackStatus, L"%2", std::to_wstring( m_TrackCount ) );
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, trackStatus.c_str() );
	}

	const HWND progressTrack = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressTrack ) {
		const long displayPosition = static_cast<long>( SendMessage( progressTrack, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressTrack * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressTrack, PBM_SETPOS, currentPosition, 0 );
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
	const std::string gainTrack = GainToString( mediaInfo.GetGainTrack() );
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
	const std::string gainAlbum = GainToString( mediaInfo.GetGainAlbum() );
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
