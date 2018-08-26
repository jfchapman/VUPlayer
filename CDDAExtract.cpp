#include "CDDAExtract.h"

#include "resource.h"
#include "Utility.h"

#include "EncoderFlac.h"
#include "HandlerFlac.h"
#include "replaygain_analysis.h"

#include <Shlwapi.h>

#include <iomanip>
#include <sstream>

// The maximum number of times to read a CDDA sector.
static const long s_MaxReadPasses = 10;

// Timer ID.
static const long s_TimerID = 1212;

// Timer interval in milliseconds.
static const long s_TimerInterval = 250;

// ID to indicate the the read thread has finished.
static const long s_ReadFinished = 111;

// Message ID sent when the extraction has finished successfully.
static const UINT MSG_EXTRACTFINISHED = WM_APP + 90;

// Message ID sent when there has been an error during extraction.
static const UINT MSG_EXTRACTERROR = WM_APP + 91;

INT_PTR CALLBACK CDDAExtract::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
			}
			break;
		}
		case WM_COMMAND : {
			switch ( LOWORD( wParam ) ) {
				case IDCANCEL : {
					CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
					if ( nullptr != dialog ) {
						dialog->Close();
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
			CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( ( nullptr != dialog ) && ( s_TimerID == wParam ) ) {
				dialog->UpdateStatus();
			}
			break;
		}
		case MSG_EXTRACTFINISHED : {
			CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				dialog->Close();
			}
			break;
		}
		case MSG_EXTRACTERROR : {
			CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( GetWindowLongPtr( hwnd, DWLP_USER ) );
			if ( nullptr != dialog ) {
				dialog->Error();
			}
			break;
		}
		default : {
			break;
		}
	}
	return FALSE;
}

DWORD WINAPI CDDAExtract::ReadThreadProc( LPVOID lpParam )
{
	CDDAExtract* extract = reinterpret_cast<CDDAExtract*>( lpParam );
	if ( nullptr != extract ) {
		extract->ReadHandler();
	}
	return 0;
}

DWORD WINAPI CDDAExtract::EncodeThreadProc( LPVOID lpParam )
{
	CDDAExtract* extract = reinterpret_cast<CDDAExtract*>( lpParam );
	if ( nullptr != extract ) {
		extract->EncodeHandler();
	}
	return 0;
}

CDDAExtract::CDDAExtract( const HINSTANCE instance, const HWND parent, Library& library, Settings& settings, Handlers& handlers, CDDAManager& cddaManager, const MediaInfo::List& tracks ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Library( library ),
	m_Settings( settings ),
	m_Handlers( handlers ),
	m_CDDAManager( cddaManager ),
	m_Tracks( tracks ),
	m_CancelEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_PendingEncodeEvent( CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ ) ),
	m_ReadThread( nullptr ),
	m_EncodeThread( nullptr ),
	m_PendingEncode(),
	m_PendingEncodeMutex(),
	m_StatusTrack( 0 ),
	m_StatusPass( 0 ),
	m_ProgressRead( 0 ),
	m_ProgressEncode( 0 ),
	m_ProgressRange( 100 ),
	m_DisplayedTrack( 0 ),
	m_DisplayedPass( 0 )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_EXTRACT_PROGRESS ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

CDDAExtract::~CDDAExtract()
{
}

void CDDAExtract::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_READ );
	if ( nullptr != progressRead ) {
		RECT rect = {};
		GetClientRect( progressRead, &rect );
		if ( ( rect.right - rect.left ) > 0 ) {
			m_ProgressRange = rect.right - rect.left;
			SendMessage( progressRead, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
		}
	}
	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_ENCODE );
	if ( nullptr != progressEncode ) {
		SendMessage( progressEncode, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
	}

	UpdateStatus();
	
	m_EncodeThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, EncodeThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );
	m_ReadThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, ReadThreadProc, this /*param*/, 0 /*flags*/, NULL /*threadId*/ );

	SetTimer( m_hWnd, s_TimerID, s_TimerInterval, NULL /*timerProc*/ );
}

void CDDAExtract::Close()
{
	KillTimer( m_hWnd, s_TimerID );

	SetEvent( m_CancelEvent );
	if ( nullptr != m_EncodeThread ) {
		WaitForSingleObject( m_EncodeThread, INFINITE );
		CloseHandle( m_EncodeThread );
		m_EncodeThread = nullptr;
	}
	if ( nullptr != m_ReadThread ) {
		WaitForSingleObject( m_ReadThread, INFINITE );
		CloseHandle( m_ReadThread );
		m_ReadThread = nullptr;
	}
	CloseHandle( m_CancelEvent );
	m_CancelEvent = nullptr;
	CloseHandle( m_PendingEncodeEvent );
	m_PendingEncodeEvent = nullptr;

	EndDialog( m_hWnd, 0 );
}

bool CDDAExtract::Cancelled() const
{
	const bool cancelled = ( WAIT_OBJECT_0 == WaitForSingleObject( m_CancelEvent, 0 ) );
	return cancelled;
}

void CDDAExtract::Error()
{
	SetEvent( m_CancelEvent );
	KillTimer( m_hWnd, s_TimerID );
	
	WCHAR buffer[ 32 ] = {};
	LoadString( m_hInst, IDS_EXTRACT_ERROR, buffer, 32 );
	SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, buffer );

	LoadString( m_hInst, IDS_CLOSE, buffer, 32 );
	SetDlgItemText( m_hWnd, IDCANCEL, buffer );

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_READ );
	if ( nullptr != progressRead ) {
		SendMessage( progressRead, PBM_SETPOS, 0, 0 );	
	}

	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_ENCODE );
	if ( nullptr != progressEncode ) {
		SendMessage( progressEncode, PBM_SETPOS, 0, 0 );	
	}
}

void CDDAExtract::ReadHandler()
{
	// A set of CDDA sector data.
	typedef std::set<CDDAMedia::Data> SectorSet;

	// A map of CDDA sector sets.
	typedef std::map<long,SectorSet> SectorMap;
	SectorMap sectorMap;

	bool readError = false;
	const CDDAManager::CDDAMediaMap drives = m_CDDAManager.GetCDDADrives();
	auto trackIter = m_Tracks.begin();
	while ( !Cancelled() && ( m_Tracks.end() != trackIter ) ) {
		// Get the CDDA media object for the current track.
		const CDDAMedia* media = nullptr;
		wchar_t drive = 0;
		long track = 0;
		if ( CDDAMedia::FromMediaFilepath( trackIter->GetFilename(), drive, track ) ) {
			const auto driveIter = drives.find( drive );
			if ( drives.end() != driveIter ) {
				media = &driveIter->second;
			}
		}

		// Keep reading the current track until we get two consistent reads for each sector.
		if ( nullptr != media ) {
			const long sectorCount = media->GetSectorCount( track );
			const long sectorStart = media->GetStartSector( track );
			const long sectorEnd = sectorStart + sectorCount;

			if ( sectorCount > 0 ) {
				const HANDLE mediaHandle = media->Open();
				if ( nullptr != mediaHandle ) {

					std::set<long> sectorsRemaining;
					for ( long sectorIndex = sectorStart; sectorIndex < sectorEnd; sectorIndex++ ) {
						sectorsRemaining.insert( sectorIndex );
					}

					sectorMap.clear();
					CDDAMedia::Data data;

					long pass = 1;

					m_StatusTrack.store( track );
					m_StatusPass.store( pass );
					m_ProgressRead.store( 0 );

					while ( !Cancelled() && ( pass <= s_MaxReadPasses ) && !sectorsRemaining.empty() ) {
						// Re-read all sectors on each pass, not just the remaining sectors, in an attempt to flush any cache on the device.
						long sectorIndex = sectorStart;
						while ( !Cancelled() && ( sectorIndex < sectorEnd ) && !sectorsRemaining.empty() ) {
							if ( media->Read( mediaHandle, sectorIndex, false /*useCache*/, data ) ) {
								const auto sectorIter = sectorsRemaining.find( sectorIndex );
								if ( sectorsRemaining.end() != sectorIter ) {
									SectorSet& sectorSet = sectorMap[ sectorIndex ];
									if ( sectorSet.end() == sectorSet.find( data ) ) {
										sectorSet.insert( data );
									} else {
										sectorsRemaining.erase( sectorIter );
										if ( sectorSet.size() > 1 ) {
											// Keep a single copy of the valid data for this sector.
											sectorSet.clear();
											sectorSet.insert( data );
										}
									}
								}
							}
							++sectorIndex;
							m_ProgressRead.store( static_cast<float>( sectorIndex - sectorStart ) / sectorCount );
						}
						++pass;
						if ( pass <= s_MaxReadPasses ) {
							m_StatusPass.store( pass );
						}
					}

					media->Close( mediaHandle );

					if ( !Cancelled() ) {
						if ( sectorsRemaining.empty() && ( sectorMap.size() == static_cast<size_t>( sectorCount ) ) ) {
							// Concatenate sector data and pass off to the encoder.
							const size_t totalSize = sectorMap.begin()->second.begin()->size() * sectorCount;
							DataPtr trackData( new CDDAMedia::Data() );
							trackData->reserve( totalSize );
							for ( const auto& sector : sectorMap ) {
								const CDDAMedia::Data& sectorData = *sector.second.begin();
								trackData->insert( trackData->end(), sectorData.begin(), sectorData.end() );
							}
							std::lock_guard<std::mutex> lock( m_PendingEncodeMutex );
							m_PendingEncode.insert( MediaData::value_type( *trackIter, trackData ) );
							SetEvent( m_PendingEncodeEvent );
						} else {
							readError = true;
							PostMessage( m_hWnd, MSG_EXTRACTERROR, 0, 0 );
							break;
						}
					}
				}
			}
		}
		++trackIter;
	}
	if ( !readError ) {
		m_StatusTrack.store( s_ReadFinished );
	}
}

void CDDAExtract::EncodeHandler()
{
	long long totalSamples = 0;
	for ( const auto& media : m_Tracks ) {
		totalSamples += ( media.GetFilesize() / 4 );
	}
	long long totalSamplesEncoded = 0;

	const size_t trackCount = m_Tracks.size();
	size_t tracksEncoded = 0;
	MediaInfo::List encodedMediaList;

	replaygain_analysis analysis;
	analysis.InitGainAnalysis( 44100 );
	float albumPeak = 0;

	const HANDLE eventHandles[ 2 ] = { m_CancelEvent, m_PendingEncodeEvent };
	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, INFINITE ) != WAIT_OBJECT_0 ) {
		MediaInfo mediaInfo;
		DataPtr data;
		m_PendingEncodeMutex.lock();
		auto dataIter = m_PendingEncode.begin();
		if ( m_PendingEncode.end() != dataIter ) {
			mediaInfo = dataIter->first;
			data = dataIter->second;
			m_PendingEncode.erase( dataIter );
			if ( m_PendingEncode.empty() ) {
				ResetEvent( m_PendingEncodeEvent );
			}
		}
		m_PendingEncodeMutex.unlock();

		if ( data ) {
			float trackPeak = 0;
			long long samplesEncoded = 0;
			const std::wstring filename = GetOutputFilename( mediaInfo );
			if ( !filename.empty() ) {
				Encoder::Ptr encoder( new EncoderFlac() );
				const long sampleRate = mediaInfo.GetSampleRate();
				const long channels = mediaInfo.GetChannels();
				const long bps = mediaInfo.GetBitsPerSample();
				if ( encoder->Open( filename, sampleRate, channels, bps ) ) {
					const long sampleBufferSize = 65536;
					std::vector<float> sampleBuffer( sampleBufferSize * channels );

					std::vector<float> analysisLeft( sampleBufferSize );
					std::vector<float> analysisRight( sampleBufferSize );

					auto sourceIter = data->begin();
					while ( !Cancelled() && ( data->end() != sourceIter ) ) {
						auto destIter = sampleBuffer.begin();
						while ( ( data->end() != sourceIter ) && ( sampleBuffer.end() != destIter ) ) {
							*destIter++ = *sourceIter++ / 32768.0f;
						}
						const long sampleCount = static_cast<long>( destIter - sampleBuffer.begin() ) / channels;
						if ( encoder->Write( &sampleBuffer[ 0 ], sampleCount ) ) {

							for ( long sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
								analysisLeft[ sampleIndex ] = sampleBuffer[ sampleIndex * channels ] * 32768.0f;
								if ( analysisLeft[ sampleIndex ] > trackPeak ) {
									trackPeak = analysisLeft[ sampleIndex ];
								}
								analysisRight[ sampleIndex ] = sampleBuffer[ sampleIndex * channels + 1 ] * 32768.0f;
								if ( analysisRight[ sampleIndex ] > trackPeak ) {
									trackPeak = analysisRight[ sampleIndex ];
								}
							}
							analysis.AnalyzeSamples( &analysisLeft[ 0 ], &analysisRight[ 0 ], sampleCount, channels );

							samplesEncoded += sampleCount;
							totalSamplesEncoded += sampleCount;
							m_ProgressEncode.store( static_cast<float>( totalSamplesEncoded ) / totalSamples );
						} else {
							break;
						}
					}
					encoder->Close();
				}
			}
			if ( !Cancelled() ) {
				const bool encodeSuccess = ( ( mediaInfo.GetFilesize() / 4 ) == samplesEncoded );
				if ( encodeSuccess ) {

					if ( trackPeak > albumPeak ) {
						albumPeak = trackPeak;
					}
					float trackGain = analysis.GetTitleGain();
					if ( GAIN_NOT_ENOUGH_SAMPLES == trackGain ) {
						trackGain = REPLAYGAIN_NOVALUE;
					}
					mediaInfo.SetPeakTrack( trackPeak );
					mediaInfo.SetGainTrack( trackGain );
					WriteTrackTags( filename, mediaInfo );

					encodedMediaList.push_back( MediaInfo( filename ) );

					if ( ++tracksEncoded == trackCount ) {
						
						std::wstring extractFolder;
						std::wstring extractFilename;
						bool extractToLibrary = false;
						m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );

						float albumGain = analysis.GetAlbumGain();
						if ( GAIN_NOT_ENOUGH_SAMPLES == albumGain ) {
							albumGain = REPLAYGAIN_NOVALUE;
						}
						mediaInfo.SetPeakAlbum( albumPeak );
						mediaInfo.SetGainAlbum( albumGain );
						for ( auto& encodedMedia : encodedMediaList ) {
							WriteAlbumTags( encodedMedia.GetFilename(), mediaInfo );					
							if ( extractToLibrary ) {
								m_Library.GetMediaInfo( encodedMedia );
							}
						}

						PostMessage( m_hWnd, MSG_EXTRACTFINISHED, 0, 0 );
						break;
					}
				} else {
					PostMessage( m_hWnd, MSG_EXTRACTERROR, 0, 0 );
					break;
				}
			}
		}
	}
}

std::wstring CDDAExtract::GetOutputFilename( const MediaInfo& mediaInfo ) const
{
	std::wstring outputFilename;

	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary );

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
	outputFilename += L".flac";
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

void CDDAExtract::UpdateStatus()
{
	const long currentTrack = m_StatusTrack.load();
	const long currentPass = m_StatusPass.load();
	if ( ( currentTrack != m_DisplayedTrack ) || ( currentPass != m_DisplayedPass ) ) {
		m_DisplayedTrack = currentTrack;
		m_DisplayedPass = currentPass;
		WCHAR buffer[ 32 ];
		std::wstring readStatus;
		if ( s_ReadFinished == m_DisplayedTrack ) {
			LoadString( m_hInst, IDS_EXTRACT_STATUS_READCOMPLETE, buffer, 32 );
			readStatus = buffer;
		} else {
			LoadString( m_hInst, IDS_EXTRACT_STATUS_TRACK, buffer, 32 );
			std::wstring trackStr( buffer );
			WideStringReplace( trackStr, L"%", std::to_wstring( m_DisplayedTrack ) );
			LoadString( m_hInst, IDS_EXTRACT_STATUS_PASS, buffer, 32 );
			std::wstring passStr( buffer );
			WideStringReplace( passStr, L"%", std::to_wstring( m_DisplayedPass ) );
			readStatus = trackStr + L" (" + passStr + L"):";
		}
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, readStatus.c_str() );
	}

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_READ );
	if ( nullptr != progressRead ) {
		const long displayPosition = static_cast<long>( SendMessage( progressRead, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressRead * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressRead, PBM_SETPOS, currentPosition, 0 );
		}
	}

	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_ENCODE );
	if ( nullptr != progressEncode ) {
		const long displayPosition = static_cast<long>( SendMessage( progressEncode, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressEncode * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressEncode, PBM_SETPOS, currentPosition, 0 );
		}
	}
}

void CDDAExtract::WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo )
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
		HandlerFlac handler;
		handler.SetTags( filename, tags );
	}
}

void CDDAExtract::WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo )
{
	Handler::Tags tags;
	const std::wstring peakAlbum = PeakToWideString( mediaInfo.GetPeakAlbum() );
	if ( !peakAlbum.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::PeakAlbum, peakAlbum ) );
	}
	const std::wstring gainAlbum = ( REPLAYGAIN_NOVALUE != mediaInfo.GetGainAlbum() ) ? GainToWideString( mediaInfo.GetGainAlbum() ) : std::wstring();
	if ( !gainAlbum.empty() ) {
		tags.insert( Handler::Tags::value_type( Handler::Tag::GainAlbum, gainAlbum ) );
	}

	if ( !tags.empty() ) {
		HandlerFlac handler;
		handler.SetTags( filename, tags );
	}
}
