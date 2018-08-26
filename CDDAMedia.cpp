#include "CDDAMedia.h"

#include "replaygain_analysis.h"

#include "resource.h"

#include "Utility.h"

#include <iomanip>
#include <numeric>
#include <regex>
#include <sstream>
#include <stdexcept>

// CDDA sector size in bytes.
static const unsigned long SECTORSIZE = 2352;

// CDDA pregap in sectors.
static const unsigned long PREGAP = 150;

CDDAMedia::CDDAMedia( const wchar_t drive, Library& library, Gracenote& gracenote ) :
	m_DrivePath( L"\\\\.\\" + std::wstring( 1, drive ) + L":" ),
	m_Library( library ),
	m_Gracenote( gracenote ),
	m_DiskGeometry( {} ),
	m_TOC( {} ),
	m_CDDB( 0 ),
	m_Playlist( new Playlist( m_Library, Playlist::Type::CDDA ) ),
	m_Cache( std::make_shared<Cache>() )
{
	if ( !ReadTOC() || !GeneratePlaylist( drive ) ) {
		throw std::runtime_error( "No audio CD in drive " + std::string( 1, static_cast<char>( drive ) ) );
	}
}

CDDAMedia::~CDDAMedia()
{
}

std::wstring CDDAMedia::ToMediaFilepath( const wchar_t drive, const long track )
{
	const int bufSize = 32;
	WCHAR trackStr[ bufSize ] = {};
	LoadString( GetModuleHandle( nullptr ), IDS_TRACK, trackStr, bufSize );
	std::wstringstream ss;
	ss << drive << L":\\" << trackStr << ' ' << track << L".cdda";
	const	std::wstring filepath = ss.str();
	return filepath;
}

bool CDDAMedia::FromMediaFilepath( const std::wstring& filepath, wchar_t& drive, long& track )
{
	bool success = false;
	const int bufSize = 32;
	WCHAR trackStr[ bufSize ] = {};
	LoadString( GetModuleHandle( nullptr ), IDS_TRACK, trackStr, bufSize );
	const std::wregex regex( L"^([a-zA-Z]):\\\\" + std::wstring( trackStr ) + L" (\\d{1,2}).cdda$" );
	std::wsmatch match;
	if ( std::regex_match( filepath, match, regex ) && ( 3 == match.size() ) ) {
		const std::wstring& driveStr = match.str( 1 );
		if ( !driveStr.empty() ) {
			drive = driveStr.front();
			track = std::stol( match.str( 2 ) );
			success = ( track > 0 );
		}
	}
	return success;
}

bool CDDAMedia::ReadTOC()
{
	bool containsAudio = false;
	const HANDLE driveHandle = CreateFile( m_DrivePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL /*securityAttributes*/, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*template*/ );
	if ( INVALID_HANDLE_VALUE != driveHandle ) {
		DWORD bytesReturned = 0;
		if ( ( 0 != DeviceIoControl( driveHandle, IOCTL_CDROM_GET_DRIVE_GEOMETRY, NULL /*inputBuffer*/, 0 /*inputSize*/, &m_DiskGeometry, sizeof( DISK_GEOMETRY ), &bytesReturned, NULL /*overlapped*/ ) ) &&
				( 0 != DeviceIoControl( driveHandle, IOCTL_CDROM_READ_TOC, NULL /*inputBuffer*/, 0 /*inputSize*/, &m_TOC, sizeof( CDROM_TOC ), &bytesReturned, NULL /*overlapped*/ ) ) ) {	
			containsAudio = CalculateCDDBID();
		}
		CloseHandle( driveHandle );
	}
	return containsAudio;
}

bool CDDAMedia::CalculateCDDBID()
{
	bool containsAudio = false;
	if ( m_TOC.FirstTrack > 0 ) {
		unsigned long trackSum = 0;
		unsigned long trackIndex = 0;
		const unsigned long trackCount = m_TOC.LastTrack - m_TOC.FirstTrack + 1;
		if ( trackCount < MAXIMUM_NUMBER_TRACKS ) {
			while ( trackIndex < trackCount ) {
				containsAudio = ( 0 == ( m_TOC.TrackData[ trackIndex ].Control & 4 ) );
				trackSum += CDDBSum( ( m_TOC.TrackData[ trackIndex ].Address[ 1 ] * 60 ) + m_TOC.TrackData[ trackIndex ].Address[ 2 ] );
				++trackIndex;
			}
			if ( containsAudio ) {
				const unsigned long totalSize = ( ( m_TOC.TrackData[ trackCount ].Address[ 1 ] * 60 ) + m_TOC.TrackData[ trackCount ].Address[ 2 ] ) -
						( ( m_TOC.TrackData[ 0 ].Address[ 1 ] * 60 ) + m_TOC.TrackData[ 0 ].Address[ 2 ] );
				m_CDDB = ( ( trackSum % 255 ) << 24 ) | ( totalSize << 8 ) | trackCount;
			}
		}
	}
	return containsAudio;
}

unsigned long CDDAMedia::CDDBSum( unsigned long sectorCount ) const
{
	unsigned long sum = 0;
	while ( sectorCount > 0 ) {
		sum += ( sectorCount % 10 );
		sectorCount /= 10;
	}
	return sum;
}


long CDDAMedia::GetCDDB() const
{
	return m_CDDB;
}

long CDDAMedia::GetStartSector( const long track ) const
{
	long trackOffset = 0;
	if ( ( track >= static_cast<long>( m_TOC.FirstTrack ) ) && ( track <= static_cast<long>( m_TOC.LastTrack + 1 ) ) ) {
		const long index = track - m_TOC.FirstTrack;
		trackOffset = ( m_TOC.TrackData[ index ].Address[ 1 ] * 60 * 75 ) + ( m_TOC.TrackData[ index ].Address[ 2 ] * 75 ) + ( m_TOC.TrackData[ index ].Address[ 3 ] );
	}
	return trackOffset;
}

long CDDAMedia::GetSectorCount( const long track ) const
{
	long sectorCount = 0;
	if ( ( track >= m_TOC.FirstTrack ) && ( track <= m_TOC.LastTrack ) && ( 0 == ( m_TOC.TrackData[ track ].Control & 4 ) ) ) {
		const long offset1 = GetStartSector( track );
		const long offset2 = GetStartSector( 1 + track );
		if ( offset2 > offset1 ) {
			sectorCount = ( offset2 - offset1 );
		}
	}
	return sectorCount;
}

long CDDAMedia::GetTrackLength( const long track ) const
{
	const long trackLength = GetSectorCount( track ) * SECTORSIZE;
	return trackLength;
}

bool CDDAMedia::GeneratePlaylist( const wchar_t drive )
{
	bool success = false;
	if ( m_Playlist ) {
		bool isNewMedia = false;

		// Read CD-Text and just keep the first block.
		BlockMap cdTextBlocks;
		ReadCDText( cdTextBlocks );

		const StringPairMap& cdTextStrings = !cdTextBlocks.empty() ? cdTextBlocks.begin()->second : StringPairMap();
		std::wstring cdTextDiscArtist;
		auto cdTextIter = cdTextStrings.find( 0 );
		if ( cdTextStrings.end() != cdTextIter ) {
			cdTextDiscArtist = cdTextIter->second.first;
		}
		std::wstring cdTextDiscTitle;
		cdTextIter = cdTextStrings.find( 0 );
		if ( cdTextStrings.end() != cdTextIter ) {
			cdTextDiscTitle = cdTextIter->second.second;
		}

		for ( unsigned char track = m_TOC.FirstTrack; track <= m_TOC.LastTrack; track++ ) {
			const long trackBytes = GetTrackLength( track );
			if ( trackBytes > 0 ) {
				MediaInfo mediaInfo( GetCDDB() );
				MediaInfo previousMediaInfo( mediaInfo );

				mediaInfo.SetBitsPerSample( 16 );
				mediaInfo.SetChannels( 2 );
				mediaInfo.SetFilesize( trackBytes );
				mediaInfo.SetSampleRate( 44100 );
				mediaInfo.SetDuration( static_cast<float>( mediaInfo.GetFilesize() ) / ( 44100 * 4 ) );
				mediaInfo.SetTrack( track );
				mediaInfo.SetFilename( ToMediaFilepath( drive, track ) );

				if ( !m_Library.GetMediaInfo( mediaInfo, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ ) ) {
					mediaInfo.SetArtist( cdTextDiscArtist );
					mediaInfo.SetAlbum( cdTextDiscTitle );
					cdTextIter = cdTextStrings.find( track );
					if ( cdTextStrings.end() != cdTextIter ) {
						const std::wstring& artist = cdTextIter->second.first;
						if ( !artist.empty() ) {
							mediaInfo.SetArtist( artist );
						}
					}
					cdTextIter = cdTextStrings.find( track );
					if ( cdTextStrings.end() != cdTextIter ) {
						const std::wstring& title = cdTextIter->second.second;
						if ( !title.empty() ) {
							mediaInfo.SetTitle( title );
						}
					}

					m_Library.UpdateMediaTags( previousMediaInfo, mediaInfo );

					isNewMedia = true;
				}

				m_Playlist->AddItem( mediaInfo );

				if ( m_Playlist->GetName().empty() ) {
					m_Playlist->SetName( mediaInfo.GetAlbum() );
				}
			}
		}

		if ( m_Playlist->GetName().empty() ) {
			const int bufSize = 32;
			WCHAR buffer[ bufSize ] = {};
			LoadString( GetModuleHandle( nullptr ), IDS_CDAUDIO, buffer, bufSize );
			m_Playlist->SetName( std::wstring( buffer ) + L" (" + drive + L":)" );
		}

		success = ( m_Playlist->GetCount() > 0 );

		if ( success && isNewMedia ) {
			m_Gracenote.Query( GetGracenoteTOC(), false /*forceDialog*/ );	
		}
	}
	return success;
}

Playlist::Ptr CDDAMedia::GetPlaylist() const
{
	return m_Playlist;
}

HANDLE CDDAMedia::Open() const
{
	HANDLE driveHandle = CreateFile( m_DrivePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL /*securityAttributes*/, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*template*/ );
	if ( INVALID_HANDLE_VALUE == driveHandle ) {
		driveHandle = nullptr;
	}
	return driveHandle;
}

void CDDAMedia::Close( const HANDLE handle ) const
{
	if ( nullptr != handle ) {
		CloseHandle( handle );
	}
}

bool CDDAMedia::Read( const HANDLE handle, const long sector, const bool useCache, Data& data ) const
{
	bool success = false;
	if ( nullptr != handle ) {
		if ( useCache ) {
			success = m_Cache->GetData( sector, data );
		}
		
		if ( !success ) {
			data.resize( SECTORSIZE / 2 );

			RAW_READ_INFO info = {};
			info.SectorCount = 1;
			info.TrackMode = CDDA;
			info.DiskOffset.QuadPart = ( sector - PREGAP ) * m_DiskGeometry.BytesPerSector;

			DWORD bytesRead = 0;
			success = ( FALSE != DeviceIoControl( handle, IOCTL_CDROM_RAW_READ, &info, sizeof( RAW_READ_INFO ), &data[ 0 ], SECTORSIZE, &bytesRead, 0 ) ) && ( SECTORSIZE == bytesRead );

			if ( success && useCache ) {
				m_Cache->SetData( sector, data );
			}
		}
	}
	return success;
}

bool CDDAMedia::ReadCDText( BlockMap& blocks ) const
{
	bool success = false;
	const HANDLE handle = Open();
	if ( nullptr != handle ) {
		CDROM_READ_TOC_EX toc = {};
		toc.Format = CDROM_READ_TOC_EX_FORMAT_CDTEXT;
		CDROM_TOC_CD_TEXT_DATA textData;
		DWORD result = 0;
		if ( DeviceIoControl( handle, IOCTL_CDROM_READ_TOC_EX, &toc, sizeof( CDROM_READ_TOC_EX ), &textData, sizeof( CDROM_TOC_CD_TEXT_DATA ), &result, 0 ) ) {
			const int textSize = ( static_cast<int>( textData.Length[ 0 ] ) << 8 ) + textData.Length[ 1 ];
			const int blockSize = sizeof( CDROM_TOC_CD_TEXT_DATA_BLOCK );
			if ( ( textSize > 0 ) && ( 2 == ( textSize % blockSize ) ) ) {
				unsigned char* textbuffer = new unsigned char[ textSize ];
				PCDROM_TOC_CD_TEXT_DATA cdText = reinterpret_cast<PCDROM_TOC_CD_TEXT_DATA>( textbuffer );
				if ( DeviceIoControl( handle, IOCTL_CDROM_READ_TOC_EX, &toc, sizeof( CDROM_READ_TOC_EX ), cdText, textSize, &result, 0 ) ) {
					const int descriptorCount = textSize / blockSize;
					for ( int descriptorIndex = 0; descriptorIndex < descriptorCount; descriptorIndex++ ) {
						const CDROM_TOC_CD_TEXT_DATA_BLOCK& dataBlock = cdText->Descriptors[ descriptorIndex ];
						switch ( dataBlock.PackType ) {
							case CDROM_CD_TEXT_PACK_ALBUM_NAME : {
								auto blockIter = blocks.insert( BlockMap::value_type( dataBlock.BlockNumber, StringPairMap() ) ).first;
								if ( blocks.end() != blockIter ) {
									auto& block = blockIter->second;
									auto trackIter = block.insert( StringPairMap::value_type( dataBlock.TrackNumber, StringPair() ) ).first;
									if ( block.end() != trackIter ) {
										std::wstring& title = trackIter->second.second;
										StringMap extraTitles;
										GetDataBlockText( *cdText, descriptorIndex, title, extraTitles );
										for ( const auto& extra : extraTitles ) {
											trackIter = block.insert( StringPairMap::value_type( extra.first, StringPair() ) ).first;
											if ( block.end() != trackIter ) {
												trackIter->second.second = extra.second;
											}
										}
									}
								}
								break;
							}
							case CDROM_CD_TEXT_PACK_PERFORMER : {
								auto blockIter = blocks.insert( BlockMap::value_type( dataBlock.BlockNumber, StringPairMap() ) ).first;
								if ( blocks.end() != blockIter ) {
									auto& block = blockIter->second;
									auto trackIter = block.insert( StringPairMap::value_type( dataBlock.TrackNumber, StringPair() ) ).first;
									if ( block.end() != trackIter ) {
										std::wstring& artist = trackIter->second.first;
										StringMap extraTitles;
										GetDataBlockText( *cdText, descriptorIndex, artist, extraTitles );
										for ( const auto& extra : extraTitles ) {
											trackIter = block.insert( StringPairMap::value_type( extra.first, StringPair() ) ).first;
											if ( block.end() != trackIter ) {
												trackIter->second.first = extra.second;
											}
										}
									}
								}
								break;
							}
							default : {
								break;
							}
						}
					}
				}
				delete [] textbuffer;
			}
		}
		Close( handle );
	}
	return success;
}

void CDDAMedia::GetDataBlockText( const CDROM_TOC_CD_TEXT_DATA& data, const int descriptorIndex, std::wstring& text, StringMap& extraTitles ) const
{
	const CDROM_TOC_CD_TEXT_DATA_BLOCK& currentBlock = data.Descriptors[ descriptorIndex ];
	if ( ( currentBlock.CharacterPosition > 0 ) && ( currentBlock.CharacterPosition <= ( currentBlock.Unicode ? 6 : 12 ) ) && ( descriptorIndex > 0 ) ) {
		const CDROM_TOC_CD_TEXT_DATA_BLOCK& previousBlock = data.Descriptors[ descriptorIndex - 1 ];
		if ( ( previousBlock.PackType == currentBlock.PackType ) && ( previousBlock.Unicode == currentBlock.Unicode ) ) {
			text = currentBlock.Unicode ? std::wstring( previousBlock.WText + 6 - currentBlock.CharacterPosition, currentBlock.CharacterPosition ) : 
				CodePageToWideString( std::string( reinterpret_cast<const char*>( previousBlock.Text + 12 - currentBlock.CharacterPosition ), currentBlock.CharacterPosition ), 1252 );
		}
	}
	text += currentBlock.Unicode ? std::wstring( currentBlock.WText, 6 ) : CodePageToWideString( std::string( reinterpret_cast<const char*>( currentBlock.Text ), 12 ), 1252 );

	if ( descriptorIndex > 0 ) { 
		const CDROM_TOC_CD_TEXT_DATA_BLOCK& previousBlock = data.Descriptors[ descriptorIndex - 1 ];
		if ( ( ( currentBlock.TrackNumber - previousBlock.TrackNumber ) > 1 ) && !previousBlock.Unicode ) {
			// Deal with any null terminated strings that began and ended within the previous block.
			char buffer[ 14 ] = {};
			memcpy( buffer, previousBlock.Text, 12 );
			char* str = buffer + strlen( buffer ) + 1;
			long currentTrack = previousBlock.TrackNumber + 1;
			while ( ( 0 != *str ) && ( currentTrack < currentBlock.TrackNumber ) ) {
				extraTitles.insert( StringMap::value_type( currentTrack++, CodePageToWideString( str, 1252 ) ) );
				str += strlen( str ) + 1;
			}
		}
	}
}

std::string CDDAMedia::GetGracenoteTOC() const
{
	std::string toc;
	for ( long track = m_TOC.FirstTrack; track <= ( 1 + m_TOC.LastTrack ); track++ ) {
		toc += std::to_string( GetStartSector( track ) ) + " ";
	}
	if ( !toc.empty() ) {
		toc.pop_back();
	}
	return toc;
}
