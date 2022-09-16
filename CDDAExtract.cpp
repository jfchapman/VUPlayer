#include "CDDAExtract.h"

#include "resource.h"
#include "Utility.h"
#include "WndTaskbar.h"

#include "ebur128.h"

#include <iomanip>
#include <sstream>

// The maximum number of times to read a CDDA sector.
static const long s_MaxReadPasses = 9;

// Timer ID.
static const long s_TimerID = 1212;

// Timer interval in milliseconds.
static const long s_TimerInterval = 250;

// ID to indicate that the read thread is fixing sectors.
static const long s_ReadFixingSectors = 110;

// ID to indicate that the read thread has finished.
static const long s_ReadFinished = 111;

// Message ID sent when the extraction has finished successfully.
// 'wParam' - unused.
// 'lParam' - unused.
static const UINT MSG_EXTRACTFINISHED = WM_APP + 90;

// Message ID sent when there has been an error during extraction.
// 'wParam' - error message resource ID.
// 'lParam' - unused.
static const UINT MSG_EXTRACTERROR = WM_APP + 91;

INT_PTR CALLBACK CDDAExtract::DialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message ) {
		case WM_INITDIALOG : {
			CDDAExtract* dialog = reinterpret_cast<CDDAExtract*>( lParam );
			if ( nullptr != dialog ) {
				SetWindowLongPtr( hwnd, DWLP_USER, lParam );
				dialog->OnInitDialog( hwnd );
				return TRUE;
			}
			break;
		}
		case WM_DESTROY : {
			SetWindowLongPtr( hwnd, DWLP_USER, 0 );
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
				dialog->Error( static_cast<WORD>( wParam ) );
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
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		extract->EncodeHandler();
		CoUninitialize();
	}
	return 0;
}

CDDAExtract::CDDAExtract( const HINSTANCE instance, const HWND parent, Library& library, Settings& settings, Handlers& handlers, DiscManager& discManager, const Playlist::ItemList& tracks, const Handler::Ptr encoderHandler, const std::wstring& joinFilename, WndTaskbar& taskbar ) :
	m_hInst( instance ),
	m_hWnd( nullptr ),
	m_Library( library ),
	m_Settings( settings ),
	m_Handlers( handlers ),
	m_DiscManager( discManager ),
	m_Taskbar( taskbar ),
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
	m_DisplayedPass( 0 ),
	m_EncoderHandler( encoderHandler ),
	m_Encoder( encoderHandler ? encoderHandler->OpenEncoder() : nullptr ),
	m_EncoderSettings( m_Encoder ? m_Settings.GetEncoderSettings( encoderHandler->GetDescription() ) : std::string() ),
	m_JoinFilename( joinFilename )
{
	DialogBoxParam( instance, MAKEINTRESOURCE( IDD_CONVERT_PROGRESS ), parent, DialogProc, reinterpret_cast<LPARAM>( this ) );
}

CDDAExtract::~CDDAExtract()
{
}

void CDDAExtract::OnInitDialog( const HWND hwnd )
{
	m_hWnd = hwnd;
	CentreDialog( m_hWnd );

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressRead ) {
		RECT rect = {};
		GetClientRect( progressRead, &rect );
		if ( ( rect.right - rect.left ) > 0 ) {
			m_ProgressRange = rect.right - rect.left;
			SendMessage( progressRead, PBM_SETRANGE, 0, MAKELONG( 0, m_ProgressRange ) );
		}
	}
	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
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

void CDDAExtract::Error( const WORD errorID )
{
	SetEvent( m_CancelEvent );
	KillTimer( m_hWnd, s_TimerID );
	
	const int bufferSize = 256;
	WCHAR buffer[ bufferSize ] = {};
	if ( FALSE != LoadString( m_hInst, errorID, buffer, bufferSize ) ) {
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, buffer );
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_ENCODER, buffer );
	}

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressRead ) {
		SendMessage( progressRead, PBM_SETPOS, m_ProgressRange, 0 );			
	}

	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
	if ( nullptr != progressEncode ) {
		SendMessage( progressEncode, PBM_SETPOS, m_ProgressRange, 0 );			
	}

	LoadString( m_hInst, IDS_CLOSE, buffer, bufferSize );
	SetDlgItemText( m_hWnd, IDCANCEL, buffer );
}

void CDDAExtract::ReadHandler()
{
	// A set of CDDA sector data.
	typedef std::set<CDDAMedia::Data> SectorSet;

	// A map of CDDA sector sets.
	typedef std::map<long,SectorSet> SectorMap;
	SectorMap sectorMap;

	// A cache of CDDA sectors.
	CDDAMedia::DataMap sectorCache;
	const long maxCachedSectors = 32;

	bool readError = false;
	const DiscManager::CDDAMediaMap drives = m_DiscManager.GetCDDADrives();
	auto trackIter = m_Tracks.begin();
	while ( !Cancelled() && ( m_Tracks.end() != trackIter ) ) {
		// Get the CDDA media object for the current track.
		const CDDAMedia* media = nullptr;
		wchar_t drive = 0;
		long track = 0;
		if ( CDDAMedia::FromMediaFilepath( trackIter->Info.GetFilename(), drive, track ) ) {
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
					sectorCache.clear();

					long pass = 1;

					m_StatusTrack.store( track );
					m_StatusPass.store( pass );
					m_ProgressRead.store( 0 );

					while ( !Cancelled() && ( pass <= s_MaxReadPasses ) && !sectorsRemaining.empty() ) {
						// Re-read all sectors on each pass, not just the remaining sectors, in an attempt to flush any cache on the device.
						long sectorIndex = sectorStart;
						while ( !Cancelled() && ( sectorIndex < sectorEnd ) && !sectorsRemaining.empty() ) {
					
							auto cacheIter = sectorCache.find( sectorIndex );
							if ( sectorCache.end() == cacheIter ) {
								sectorCache.clear();
								if ( media->Read( mediaHandle, sectorIndex, std::min<long>( maxCachedSectors, sectorEnd - sectorIndex ), sectorCache ) ) {
									cacheIter = sectorCache.find( sectorIndex );
								}
							}

							if ( sectorCache.end() != cacheIter ) {
								const CDDAMedia::Data& data = cacheIter->second;
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
						if ( sectorMap.size() == static_cast<size_t>( sectorCount ) ) {

							if ( !sectorsRemaining.empty() ) {
								// For each inconsistent sector, take the modal value for each sample.
								m_StatusPass.store( s_ReadFixingSectors );
								typedef std::map<short,int> SampleMap;
								SampleMap sampleMap;

								auto sectorIter = sectorsRemaining.begin();
								const size_t sectorsRemainingCount = sectorsRemaining.size();
								size_t currentSector = 0;
								while ( !Cancelled() && ( sectorsRemaining.end() != sectorIter ) ) {
									m_ProgressRead.store( static_cast<float>( currentSector ) / sectorsRemainingCount );
									SectorSet& sectorSet = sectorMap[ *sectorIter ];
									if ( sectorSet.size() > 1 ) {
										const size_t sampleCount = sectorSet.begin()->size();
										CDDAMedia::Data fixedSector( sampleCount );
										for ( size_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ ) {
											sampleMap.clear();
											for ( const auto& sector : sectorSet ) {
												auto sampleMapIter = sampleMap.insert( SampleMap::value_type( sector[ sampleIndex ], 0 ) ).first;
												if ( sampleMap.end() != sampleMapIter ) {
													sampleMapIter->second++;
												}
											}
											if ( 1 == sampleMap.size() ) {
												fixedSector[ sampleIndex ] = sampleMap.begin()->first;
											} else {
												int sampleSize = INT_MIN;
												short modalValue = 0;
												for ( const auto& sampleIter : sampleMap ) {
													if ( sampleIter.second > sampleSize ) {
														sampleSize = sampleIter.second;
														modalValue = sampleIter.first;
													}
												}
												fixedSector[ sampleIndex ] = modalValue;
											}
										}
										sectorSet.clear();
										sectorSet.insert( fixedSector );
									}
									++sectorIter;
									++currentSector;
								}
							}

							if ( !Cancelled() ) {
								// Concatenate sector data and pass off to the encoder.
								const size_t totalSize = sectorMap.begin()->second.begin()->size() * sectorCount;
								DataPtr trackData( new CDDAMedia::Data() );
								trackData->reserve( totalSize );
								for ( const auto& sector : sectorMap ) {
									const CDDAMedia::Data& sectorData = *sector.second.begin();
									trackData->insert( trackData->end(), sectorData.begin(), sectorData.end() );
								}
								std::lock_guard<std::mutex> lock( m_PendingEncodeMutex );
								m_PendingEncode.insert( MediaData::value_type( trackIter->Info, trackData ) );
								SetEvent( m_PendingEncodeEvent );
							}
						} else {
							readError = true;
							PostMessage( m_hWnd, MSG_EXTRACTERROR, IDS_EXTRACT_ERROR_READ, 0 );
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
	if ( m_Encoder && !m_Tracks.empty() ) {
		long long totalSamples = 0;
		for ( const auto& media : m_Tracks ) {
			totalSamples += ( media.Info.GetFilesize() / 4 );
		}
		long long totalSamplesEncoded = 0;

		const size_t trackCount = m_Tracks.size();
		size_t tracksEncoded = 0;
		MediaInfo::List encodedMediaList;

		std::wstring extractFolder;
		std::wstring extractFilename;
		bool extractToLibrary = false;
		bool extractJoin = false;
		m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary, extractJoin );

		std::vector<ebur128_state*> r128States;
		if ( !extractJoin ) {
			r128States.reserve( trackCount );
		}
		ebur128_state* r128State = nullptr;

		bool encoderOK = true;
		if ( extractJoin ) {
			const long sampleRate = m_Tracks.front().Info.GetSampleRate();
			const long channels = m_Tracks.front().Info.GetChannels();
			const auto bps = m_Tracks.front().Info.GetBitsPerSample();
			encoderOK = m_Encoder->Open( m_JoinFilename, sampleRate, channels, bps, totalSamples, m_EncoderSettings, {} /*tags*/ );
			r128State = ebur128_init( static_cast<unsigned int>( channels ), static_cast<unsigned int>( sampleRate ), EBUR128_MODE_I );
			if ( nullptr != r128State ) {
				r128States.push_back( r128State );
			}
		}

		if ( encoderOK ) {
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
					const long sampleRate = mediaInfo.GetSampleRate();
					const long channels = mediaInfo.GetChannels();
					const auto bps = mediaInfo.GetBitsPerSample();
					const long long trackSamplesTotal = static_cast<long long>( mediaInfo.GetDuration() * sampleRate );
					long long samplesEncoded = 0;

					if ( !extractJoin ) {
						r128State = ebur128_init( static_cast<unsigned int>( channels ), static_cast<unsigned int>( sampleRate ), EBUR128_MODE_I );
						if ( nullptr != r128State ) {
							r128States.push_back( r128State );
						}
					}

					std::wstring filename = extractJoin ? m_JoinFilename : GetOutputFilename( mediaInfo );
					if ( !filename.empty() ) {
						if ( extractJoin || m_Encoder->Open( filename, sampleRate, channels, bps, trackSamplesTotal, m_EncoderSettings, m_Library.GetTags( mediaInfo ) ) ) {
							const long sampleBufferSize = 65536;
							std::vector<float> sampleBuffer( sampleBufferSize * channels );
							int r128Error = EBUR128_SUCCESS;
							auto sourceIter = data->begin();
							while ( !Cancelled() && ( data->end() != sourceIter ) ) {
								auto destIter = sampleBuffer.begin();
								while ( ( data->end() != sourceIter ) && ( sampleBuffer.end() != destIter ) ) {
									*destIter++ = *sourceIter++ / 32768.0f;
								}
								const long sampleCount = static_cast<long>( destIter - sampleBuffer.begin() ) / channels;
								if ( m_Encoder->Write( sampleBuffer.data(), sampleCount ) ) {
									if ( ( nullptr != r128State ) && ( EBUR128_SUCCESS == r128Error ) ) {
										r128Error = ebur128_add_frames_float( r128State, sampleBuffer.data(), static_cast<size_t>( sampleCount ) );
									}
									samplesEncoded += sampleCount;
									totalSamplesEncoded += sampleCount;
									m_ProgressEncode.store( static_cast<float>( totalSamplesEncoded ) / totalSamples );
								} else {
									break;
								}
							}
							if ( !extractJoin ) {
								m_Encoder->Close();
							}
						}
					}

					if ( !Cancelled() ) {
						const bool encodeSuccess = ( ( mediaInfo.GetFilesize() / 4 ) == samplesEncoded );
						if ( encodeSuccess ) {

							if ( extractJoin ) {
								if ( ++tracksEncoded == trackCount ) {
									break;
								}
							} else {
								if ( nullptr != r128State ) {
									double loudness = 0;
									if ( EBUR128_SUCCESS == ebur128_loudness_global( r128State, &loudness ) ) {
										const float trackGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
										mediaInfo.SetGainTrack( trackGain );
									}
								}
								WriteTrackTags( filename, mediaInfo );

								encodedMediaList.push_back( MediaInfo( filename ) );

								if ( ++tracksEncoded == trackCount ) {
									std::optional<float> albumGain;
									if ( !r128States.empty() ) {
										double loudness = 0;
										if ( EBUR128_SUCCESS == ebur128_loudness_global_multiple( r128States.data(), r128States.size(), &loudness ) ) {
											albumGain = LOUDNESS_REFERENCE - static_cast<float>( loudness );
										}
									}
									mediaInfo.SetGainAlbum( albumGain );

									for ( auto& encodedMedia : encodedMediaList ) {
										WriteAlbumTags( encodedMedia.GetFilename(), mediaInfo );					
										if ( extractToLibrary || m_Library.GetMediaInfo( encodedMedia, false /*checkFileAttributes*/, false /*scanMedia*/ ) ) {
											m_Library.GetMediaInfo( encodedMedia );
										}
									}

									PostMessage( m_hWnd, MSG_EXTRACTFINISHED, 0, 0 );
									break;
								}
							}
						} else {
							encoderOK = false;
							break;
						}
					}
				}
			}

			if ( extractJoin ) {
				m_Encoder->Close();

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
				if ( extractToLibrary || m_Library.GetMediaInfo( joinedMediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/ ) ) {
					m_Library.GetMediaInfo( joinedMediaInfo );
				}

				if ( encoderOK && !Cancelled() ) {
					PostMessage( m_hWnd, MSG_EXTRACTFINISHED, 0, 0 );
				}
			}
		}

		if ( !encoderOK && !Cancelled() ) {
			PostMessage( m_hWnd, MSG_EXTRACTERROR, IDS_EXTRACT_ERROR_ENCODER, 0 );
		}

		for ( const auto& iter : r128States ) {
			ebur128_state* state = iter;
			ebur128_destroy( &state );
		}
	}
}

std::wstring CDDAExtract::GetOutputFilename( const MediaInfo& mediaInfo ) const
{
	std::wstring outputFilename;

	std::wstring extractFolder;
	std::wstring extractFilename;
	bool extractToLibrary = false;
	bool extractJoin = false;
	m_Settings.GetExtractSettings( extractFolder, extractFilename, extractToLibrary, extractJoin );

	WCHAR buffer[ 64 ] = {};
	const std::wstring emptyArtist = LoadString( m_hInst, IDS_EMPTYARTIST, buffer, 64 ) ? buffer : std::wstring();
	const std::wstring emptyAlbum = LoadString( m_hInst, IDS_EMPTYALBUM, buffer, 64 ) ? buffer : std::wstring();

	std::wstring title = mediaInfo.GetTitle( true /*filenameAsTitle*/ );
	WideStringReplaceInvalidFilenameCharacters( title, L"_", true /*replaceFolderDelimiters*/ );

	std::wstring artist = mediaInfo.GetArtist().empty() ? emptyArtist : mediaInfo.GetArtist();
	WideStringReplaceInvalidFilenameCharacters( artist, L"_", true /*replaceFolderDelimiters*/ );

	std::wstring album = mediaInfo.GetAlbum().empty() ? emptyAlbum : mediaInfo.GetAlbum();
	WideStringReplaceInvalidFilenameCharacters( album, L"_", true /*replaceFolderDelimiters*/ );

	const std::wstring sourceFilename = std::filesystem::path( mediaInfo.GetFilename() ).stem();

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
						case 'F' :
						case 'f' : {
							outputFilename += sourceFilename;
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

void CDDAExtract::UpdateStatus()
{
	const long currentTrack = m_StatusTrack.load();
	const long currentPass = m_StatusPass.load();
	if ( ( currentTrack != m_DisplayedTrack ) || ( currentPass != m_DisplayedPass ) ) {
		m_DisplayedTrack = currentTrack;
		m_DisplayedPass = currentPass;
		const int bufSize = 128;
		WCHAR buffer[ bufSize ];
		std::wstring readStatus;
		if ( s_ReadFinished == m_DisplayedTrack ) {
			LoadString( m_hInst, IDS_EXTRACT_STATUS_READCOMPLETE, buffer, bufSize );
			readStatus = buffer;
			const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
			if ( nullptr != progressRead ) {
				SendMessage( progressRead, PBM_SETPOS, m_ProgressRange, 0 );			
			}
		} else {
			if ( s_ReadFixingSectors == m_DisplayedPass ) {
				LoadString( m_hInst, IDS_EXTRACT_STATUS_FIXING, buffer, bufSize );
				readStatus = buffer;
				WideStringReplace( readStatus, L"%", std::to_wstring( m_DisplayedTrack ) );
				readStatus += L":";
			} else {
				LoadString( m_hInst, IDS_EXTRACT_STATUS_TRACK, buffer, bufSize );
				std::wstring trackStr( buffer );
				WideStringReplace( trackStr, L"%", std::to_wstring( m_DisplayedTrack ) );
				LoadString( m_hInst, IDS_EXTRACT_STATUS_PASS, buffer, bufSize );
				std::wstring passStr( buffer );
				WideStringReplace( passStr, L"%", std::to_wstring( m_DisplayedPass ) );
				readStatus = trackStr + L" (" + passStr + L"):";
			}
		}
		SetDlgItemText( m_hWnd, IDC_EXTRACT_STATE_READ, readStatus.c_str() );
	}

	const HWND progressRead = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TRACK );
	if ( nullptr != progressRead ) {
		const long displayPosition = static_cast<long>( SendMessage( progressRead, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressRead * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressRead, PBM_SETPOS, currentPosition, 0 );
		}
	}

	const HWND progressEncode = GetDlgItem( m_hWnd, IDC_EXTRACT_PROGRESS_TOTAL );
	if ( nullptr != progressEncode ) {
		const long displayPosition = static_cast<long>( SendMessage( progressEncode, PBM_GETPOS, 0, 0 ) );
		const long currentPosition = static_cast<long>( m_ProgressEncode * m_ProgressRange );
		if ( currentPosition != displayPosition ) {
			SendMessage( progressEncode, PBM_SETPOS, currentPosition, 0 );
		}
	}

	m_Taskbar.UpdateProgress( m_ProgressEncode );
}

void CDDAExtract::WriteTrackTags( const std::wstring& filename, const MediaInfo& mediaInfo )
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

void CDDAExtract::WriteAlbumTags( const std::wstring& filename, const MediaInfo& mediaInfo )
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
