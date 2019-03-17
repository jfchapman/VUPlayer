#include "Converter.h"

#include "resource.h"
#include "Utility.h"

#include "replaygain_analysis.h"

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
		case WM_INITDIALOG : {
			Converter* converter = reinterpret_cast<Converter*>( lParam );
			if ( nullptr != converter ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				converter->OnInitDialog( hwnd );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL : {
					Converter* converter = reinterpret_cast<Converter*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != converter ) {
						converter->Close();
					}
					return TRUE;
				}
				default : {
					break;
				}
			}
			break;
		}
		case WM_TIMER : {
			Converter* converter = reinterpret_cast<Converter*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( ( nullptr != converter ) && ( s_TimerID == wParam ) ) {
				converter->UpdateStatus();
			}
			break;
		}
		case MSG_CONVERTERFINISHED : {
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
		default : {
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

Converter::Converter( const HINSTANCE instance, const HWND parent, Library& library, Settings& settings, Handlers& handlers, const Playlist::ItemList& tracks, const Handler::Ptr encoderHandler, const std::wstring& joinFilename ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Library( library ),
	m_Settings( settings ),
	m_Handlers( handlers ),
	m_Tracks( tracks ),
	m_CancelEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_EncodeThread( nullptr ),
	m_StatusTrack( 1 ),
	m_ProgressTrack( 0 ),
	m_ProgressTotal( 0 ),
	m_ProgressRange( 100 ),
	m_DisplayedTrack( 0 ),
	m_TrackCount( static_cast<long>( tracks.size() ) ),
	m_Encoder( encoderHandler->OpenEncoder() ),
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

		std::wstring extractFolder;
		std::wstring extractFilename;
		bool extractToLibrary = false;
		bool extractJoin = false;
		m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary, extractJoin );

		long joinChannels = 0;
		long joinSampleRate = 0;
		if ( extractJoin ) {
			const Decoder::Ptr decoder = OpenDecoder( m_Tracks.front() );
			if ( decoder ) {
				joinChannels = decoder->GetChannels();
				joinSampleRate = decoder->GetSampleRate();
				conversionOK = m_Encoder->Open( m_JoinFilename, joinSampleRate, joinChannels, decoder->GetBPS(), m_EncoderSettings );
			} else {
				conversionOK = false;
			}
		}

		if ( conversionOK ) {
			replaygain_analysis analysis;
			const long replayGainScale = 1 << 15;
			bool calculateReplayGain = extractJoin ? 
				( ( ( 2 == joinChannels ) || ( 1 == joinChannels ) ) && analysis.InitGainAnalysis( joinSampleRate ) ) : false;

			float trackPeak = 0;

			long currentTrack = 0;
			auto track = m_Tracks.begin();
			while ( conversionOK && !Cancelled() && ( m_Tracks.end() != track ) ) {
				MediaInfo mediaInfo( track->Info );
				m_ProgressTrack.store( 0 );
				m_StatusTrack.store( ++currentTrack );
				std::wstring filename = extractJoin ? m_JoinFilename : GetOutputFilename( track->Info );
				conversionOK = !filename.empty();
				if ( conversionOK ) {
					const Decoder::Ptr decoder = OpenDecoder( *track );
					if ( extractJoin ) {
						conversionOK = static_cast<bool>( decoder );
					}
					if ( decoder ) {
						const long sampleRate = decoder->GetSampleRate();
						const long channels = decoder->GetChannels();
						const long bps = decoder->GetBPS();

						conversionOK = extractJoin ? ( ( sampleRate == joinSampleRate ) && ( channels == joinChannels ) ) :
							m_Encoder->Open( filename, sampleRate, channels, bps, m_EncoderSettings );

						if ( conversionOK ) {
							const long long trackSamplesTotal = static_cast<long long>( track->Info.GetDuration() * sampleRate );
							long long trackSamplesRead = 0;

							const long sampleCount = 65536;
							std::vector<float> sampleBuffer( sampleCount * channels );

							std::vector<float> analysisLeft;
							std::vector<float> analysisRight;
							if ( !extractJoin ) {
								calculateReplayGain = ( ( 2 == channels ) || ( 1 == channels ) ) && analysis.InitGainAnalysis( sampleRate );
							}
							if ( calculateReplayGain ) {
								analysisLeft.resize( sampleCount );
								if ( 2 == channels ) {
									analysisRight.resize( sampleCount );
								}
							}

							if ( !extractJoin ) {
								trackPeak = 0;
							}

							bool continueEncoding = true;
							while ( !Cancelled() && continueEncoding ) {
								const long samplesRead = decoder->Read( &sampleBuffer[ 0 ], sampleCount );
								if ( samplesRead > 0 ) {
									continueEncoding = m_Encoder->Write( &sampleBuffer[ 0 ], samplesRead );

									if ( calculateReplayGain ) {
										for ( long sampleIndex = 0; sampleIndex < samplesRead; sampleIndex++ ) {
											analysisLeft[ sampleIndex ] = sampleBuffer[ sampleIndex * channels ] * replayGainScale;
											if ( sqrt( sampleBuffer[ sampleIndex * channels ] * sampleBuffer[ sampleIndex * channels ] ) > trackPeak ) {
												trackPeak = sqrt( sampleBuffer[ sampleIndex * channels ] * sampleBuffer[ sampleIndex * channels ] );
											}
											if ( 2 == channels ) {
												analysisRight[ sampleIndex ] = sampleBuffer[ sampleIndex * channels + 1 ] * replayGainScale;
												if ( sqrt( sampleBuffer[ sampleIndex * channels + 1 ] * sampleBuffer[ sampleIndex * channels + 1 ] ) > trackPeak ) {
													trackPeak = sqrt( sampleBuffer[ sampleIndex * channels + 1 ] * sampleBuffer[ sampleIndex * channels + 1 ] );
												}
											}
										}
										analysis.AnalyzeSamples( &analysisLeft[ 0 ], analysisRight.empty() ? nullptr : &analysisRight[ 0 ], sampleCount, channels );
									}

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

								if ( calculateReplayGain ) {
									float trackGain = analysis.GetTitleGain();
									if ( GAIN_NOT_ENOUGH_SAMPLES != trackGain ) {
										mediaInfo.SetPeakTrack( trackPeak );
										mediaInfo.SetGainTrack( trackGain );
									}
								}

								WriteTags( filename, mediaInfo );
								if ( extractToLibrary ) {
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

					if ( calculateReplayGain ) {
						float trackGain = analysis.GetTitleGain();
						if ( GAIN_NOT_ENOUGH_SAMPLES != trackGain ) {
							joinedMediaInfo.SetPeakTrack( trackPeak );
							joinedMediaInfo.SetGainTrack( trackGain );
						}
					}

					WriteTags( joinedMediaInfo.GetFilename(), joinedMediaInfo );
					if ( extractToLibrary ) {
						m_Library.GetMediaInfo( joinedMediaInfo );
					}
				}
			}
		}
	}

	if ( !Cancelled() ) {
		PostMessage( m_hWnd, MSG_CONVERTERFINISHED, conversionOK, 0 );
	}
}

std::wstring Converter::GetOutputFilename( const MediaInfo& mediaInfo ) const
{
	std::wstring outputFilename;

	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	bool extractJoin = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary, extractJoin );

	WCHAR buffer[ 2 + MAX_PATH ] = {};
	const std::wstring emptyArtist = LoadString( m_hInst, IDS_EMPTYARTIST, buffer, 32 ) ? buffer : std::wstring();
	const std::wstring emptyAlbum = LoadString( m_hInst, IDS_EMPTYALBUM, buffer, 32 ) ? buffer : std::wstring();

	std::wstring title = mediaInfo.GetTitle( true /*filenameAsTitle*/ );
	WideStringReplaceInvalidFilenameCharacters( title, L"_", true /*replaceFolderDelimiters*/ );

	std::wstring artist = mediaInfo.GetArtist().empty() ? emptyArtist : mediaInfo.GetArtist();
	WideStringReplaceInvalidFilenameCharacters( artist, L"_", true /*replaceFolderDelimiters*/ );

	std::wstring album = mediaInfo.GetAlbum().empty() ? emptyAlbum : mediaInfo.GetAlbum();
	WideStringReplaceInvalidFilenameCharacters( album, L"_", true /*replaceFolderDelimiters*/ );

	std::wstringstream ss;
	ss << std::setfill( static_cast<wchar_t>( '0' ) ) << std::setw( 2 ) << mediaInfo.GetTrack();
	const std::wstring track = ss.str();

	auto currentChar = extractFilename.begin();
	while ( extractFilename.end() != currentChar ) {
		switch ( *currentChar ) {
			case '%' : {
				const auto nextChar = 1 + currentChar;
				if ( extractFilename.end() == nextChar ) {
					outputFilename += *currentChar;
				} else {
					switch ( *nextChar ) {
						case 'A' :
						case 'a' : {
							outputFilename += artist;
							++currentChar;
							break;
						}
						case 'D' :
						case 'd' : {
							outputFilename += album;
							++currentChar;
							break;
						}
						case 'N' :
						case 'n' : {
							outputFilename += track;
							++currentChar;
							break;
						}
						case 'T' :
						case 't' : {
							outputFilename += title;
							++currentChar;
							break;
						}
						default : {
							outputFilename += *currentChar;
							break;
						}
					}
				}
				break;
			}
			default : {
				outputFilename += *currentChar;
				break;
			}
		}
		++currentChar;
	}

	// Sanitise folder and file names.
	if ( !extractFolder.empty() && ( extractFolder.back() != '\\' ) ) {
		extractFolder += '\\';
	}
	WideStringReplaceInvalidFilenameCharacters( outputFilename, L"_", false /*replaceFolderDelimiters*/ );
	WideStringReplace( outputFilename, L"/", L"\\" );
	const size_t firstPos = outputFilename.find_first_not_of( L"\\ " );
	const size_t lastPos = outputFilename.find_last_not_of( L"\\ " );
	if ( ( std::wstring::npos != firstPos ) && ( std::wstring::npos != lastPos ) ) {
		outputFilename = outputFilename.substr( firstPos, 1 + lastPos );
	} else {
		outputFilename.clear();
	}
	if ( outputFilename.empty() ) {
		outputFilename = std::to_wstring( mediaInfo.GetTrack() );
	}
	outputFilename = extractFolder + outputFilename;

	// Create the output folder, if necessary.
	size_t pos = outputFilename.find( '\\', extractFolder.size() );
	while ( std::wstring::npos != pos ) {
		const std::wstring folder = outputFilename.substr( 0, pos );
		CreateDirectory( folder.c_str(), NULL /*attributes*/ );
		pos = outputFilename.find( '\\', 1 + folder.size() );
	};

	return outputFilename;
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
}

void Converter::WriteTags( const std::wstring& filename, const MediaInfo& mediaInfo )
{
	Handler::Tags tags;
	const std::wstring& title = mediaInfo.GetTitle();
	if ( !title.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Title, title ) );
	}
	const std::wstring& artist = mediaInfo.GetArtist();
	if ( !artist.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Artist, artist ) );
	}
	const std::wstring& album = mediaInfo.GetAlbum();
	if ( !album.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Album, album ) );
	}
	const std::wstring& genre = mediaInfo.GetGenre();
	if ( !genre.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Genre, genre ) );
	}
	const std::wstring& comment = mediaInfo.GetComment();
	if ( !comment.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Comment, comment ) );
	}
	const std::wstring track = ( mediaInfo.GetTrack() > 0 ) ? std::to_wstring( mediaInfo.GetTrack() ) : std::wstring();
	if ( !track.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Track, track ) );
	}
	const std::wstring year = ( mediaInfo.GetYear() > 0 ) ? std::to_wstring( mediaInfo.GetYear() ) : std::wstring();
	if ( !year.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::Year, year ) );
	}
	const std::wstring peakTrack = PeakToWideString( mediaInfo.GetPeakTrack() );
	if ( !peakTrack.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::PeakTrack, peakTrack ) );
	}
	const std::wstring gainTrack = ( REPLAYGAIN_NOVALUE != mediaInfo.GetGainTrack() ) ? GainToWideString( mediaInfo.GetGainTrack() ) : std::wstring();
	if ( !gainTrack.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::GainTrack, gainTrack ) );
	}
	const std::vector<BYTE> imageBytes = m_Library.GetMediaArtwork( mediaInfo );
	if ( !imageBytes.empty() ) {
		const std::wstring encodedArtwork = Base64Encode( &imageBytes[ 0 ], static_cast<int>( imageBytes.size() ) );
		tags.insert( Handler::Tags::value_type( Handler::Tag::Artwork, encodedArtwork ) );
	}

	if ( !tags.empty() ) {
		m_Handlers.SetTags( filename, tags );
	}
}

Decoder::Ptr Converter::OpenDecoder( const Playlist::Item& item ) const
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
