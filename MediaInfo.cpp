#include "MediaInfo.h"

#include "Utility.h"

#include <cmath>
#include <set>
#include <tuple>

MediaInfo::MediaInfo( const std::wstring& filename ) :
	m_Filename( filename ),
	m_Filetime( 0 ),
	m_Filesize( 0 ),
	m_Duration( 0 ),
	m_SampleRate( 0 ),
	m_BitsPerSample( 0 ),
	m_Channels( 0 ),
	m_Artist(),
	m_Title(),
	m_Album(),
	m_Genre(),
	m_Year( 0 ),
	m_Comment(),
	m_Track( 0 ),
	m_Version(),
	m_GainTrack( NAN ),
	m_GainAlbum( NAN ),
	m_ArtworkID(),
	m_Source( Source::File ),
	m_CDDB( 0 )
{
}

MediaInfo::MediaInfo( const long cddbID ) :
	MediaInfo()
{
	m_Source = Source::CDDA;
	m_CDDB = cddbID;
}

MediaInfo::~MediaInfo()
{
}

bool MediaInfo::operator<( const MediaInfo& other ) const
{
	const bool lessThan = std::tie( m_Filename, m_Filetime, m_Filesize, m_Duration, m_SampleRate, m_BitsPerSample, m_Channels, m_Artist,
		m_Title, m_Album, m_Genre, m_Year, m_Comment, m_Track, m_Version, m_GainTrack,
		m_GainAlbum, m_ArtworkID, m_Source, m_CDDB ) <

		std::tie( other.m_Filename, other.m_Filetime, other.m_Filesize, other.m_Duration, other.m_SampleRate, other.m_BitsPerSample, other.m_Channels, other.m_Artist,
		other.m_Title, other.m_Album, other.m_Genre, other.m_Year, other.m_Comment, other.m_Track, other.m_Version, other.m_GainTrack,
		other.m_GainAlbum, other.m_ArtworkID, other.m_Source, other.m_CDDB );

	return lessThan;
}

MediaInfo::operator Tags() const
{
	Tags tags;
	if ( !m_Album.empty() ) {
		tags.insert( Tags::value_type( Tag::Album, WideStringToUTF8( m_Album ) ) );	
	}
	if ( !m_Artist.empty() ) {
		tags.insert( Tags::value_type( Tag::Artist, WideStringToUTF8( m_Artist ) ) );
	}
	if ( !m_Comment.empty() ) {
		tags.insert( Tags::value_type( Tag::Comment, WideStringToUTF8( m_Comment ) ) );
	}
	if ( !m_Genre.empty() ) {
		tags.insert( Tags::value_type( Tag::Genre, WideStringToUTF8( m_Genre ) ) );		
	}
	if ( !m_Title.empty() ) {
		tags.insert( Tags::value_type( Tag::Title, WideStringToUTF8( m_Title ) ) );
	}
	if ( m_Track > 0 ) {
		tags.insert( Tags::value_type( Tag::Track, std::to_string( m_Track ) ) );
	}
	if ( m_Year > 0 ) {
		tags.insert( Tags::value_type( Tag::Year, std::to_string( m_Year ) ) );
	}
	const std::string gainAlbum = GainToString( m_GainAlbum );
	if ( !gainAlbum.empty() ) {
		tags.insert( Tags::value_type( Tag::GainAlbum, gainAlbum ) );
	}
	const std::string gainTrack = GainToString( m_GainTrack );
	if ( !gainTrack.empty() ) {
		tags.insert( Tags::value_type( Tag::GainTrack, gainTrack ) );
	}
	return tags;
}

const std::wstring& MediaInfo::GetFilename() const
{
	return m_Filename;
}

void MediaInfo::SetFilename( const std::wstring& filename )
{
	m_Filename = filename;
}

long long MediaInfo::GetFiletime() const
{
	return m_Filetime;
}

void MediaInfo::SetFiletime( const long long filetime )
{
	m_Filetime = filetime;
}

long long MediaInfo::GetFilesize() const
{
	return m_Filesize;
}

void MediaInfo::SetFilesize( const long long filesize )
{
	m_Filesize = filesize;
}

float MediaInfo::GetDuration() const
{
	return m_Duration;
}

void MediaInfo::SetDuration( const float duration )
{
	m_Duration = duration;
}

long MediaInfo::GetSampleRate() const
{
	return m_SampleRate;
}

void MediaInfo::SetSampleRate( const long sampleRate )
{
	m_SampleRate = sampleRate;
}

long MediaInfo::GetBitsPerSample() const
{
	return m_BitsPerSample;
}

void MediaInfo::SetBitsPerSample( const long bitsPerSample )
{
	m_BitsPerSample = bitsPerSample;
}

long MediaInfo::GetChannels() const
{
	return m_Channels;
}

void MediaInfo::SetChannels( const long channels )
{
	m_Channels = channels;
}

const std::wstring& MediaInfo::GetArtist() const
{
	return m_Artist;
}

void MediaInfo::SetArtist( const std::wstring& artist )
{
	m_Artist = artist;
}

void MediaInfo::SetTitle( const std::wstring& title )
{
	m_Title = title;
}

const std::wstring& MediaInfo::GetAlbum() const
{
	return m_Album;
}

void MediaInfo::SetAlbum( const std::wstring& album )
{
	m_Album = album;
}

const std::wstring& MediaInfo::GetGenre() const
{
	return m_Genre;
}

void MediaInfo::SetGenre( const std::wstring& genre )
{
	m_Genre = genre;
}

long MediaInfo::GetYear() const
{
	return m_Year;
}

void MediaInfo::SetYear( const long year )
{
	if ( ( year >= MINYEAR ) && ( year <= MAXYEAR ) ) {
		m_Year = year;
	} else {
		m_Year = 0;
	}
}

const std::wstring& MediaInfo::GetComment() const
{
	return m_Comment;
}

void MediaInfo::SetComment( const std::wstring& comment )
{
	m_Comment = comment;
}

long MediaInfo::GetTrack() const
{
	return m_Track;
}

void MediaInfo::SetTrack( const long track )
{
	m_Track = track;
}

const std::wstring& MediaInfo::GetVersion() const
{
	return m_Version;
}

void MediaInfo::SetVersion( const std::wstring& version )
{
	m_Version = version;
}

float MediaInfo::GetGainTrack() const
{
	return m_GainTrack;
}

void MediaInfo::SetGainTrack( const float gain )
{
	m_GainTrack = gain;
}

float MediaInfo::GetGainAlbum() const
{
	return m_GainAlbum;
}

void MediaInfo::SetGainAlbum( const float gain )
{
	m_GainAlbum = gain;
}

std::wstring MediaInfo::GetTitle( const bool filenameAsTitle ) const
{
	std::wstring title = m_Title;
	if ( title.empty() && filenameAsTitle ) {
		title = m_Filename;
		size_t pos = title.rfind( '.' );
		if ( std::wstring::npos != pos ) {
			title = title.substr( 0 /*offset*/, pos /*count*/ );
		}
		pos = title.find_last_of( L"/\\" );
		if ( std::wstring::npos != pos ) {
			title = title.substr( pos + 1 /*offset*/ );
		}
	}
	return title;
}

std::wstring MediaInfo::GetType() const
{
	std::wstring type;
	const size_t pos = m_Filename.rfind( '.' );
	if ( std::wstring::npos != pos ) {
		type = WideStringToUpper( m_Filename.substr( pos + 1 /*offset*/ ) );
	}
	return type;
}

long MediaInfo::GetBitrate() const
{
	const long bitrate = ( m_Duration > 0 ) ? static_cast<long>( 0.5f + ( m_Filesize * 8 ) / ( m_Duration * 1000 ) ) : 0;
	return bitrate;
}

const std::wstring& MediaInfo::GetArtworkID() const
{
	return m_ArtworkID;
}

void MediaInfo::SetArtworkID( const std::wstring& id )
{
	m_ArtworkID = id;
}

MediaInfo::Source MediaInfo::GetSource() const
{
	return m_Source;
}

long MediaInfo::GetCDDB() const
{
	return m_CDDB;
}

bool MediaInfo::IsDuplicate( const MediaInfo& o ) const
{
	const bool isDuplicate = 
		std::tie( m_Filesize, m_Duration, m_SampleRate, m_BitsPerSample, m_Channels, m_Artist, m_Title, m_Album, m_Genre, m_Year,
			m_Comment, m_Track, m_Version, m_GainTrack, m_GainAlbum, m_ArtworkID ) ==
		std::tie( o.m_Filesize, o.m_Duration, o.m_SampleRate, o.m_BitsPerSample, o.m_Channels, o.m_Artist, o.m_Title, o.m_Album, o.m_Genre, o.m_Year,
			o.m_Comment, o.m_Track, o.m_Version, o.m_GainTrack, o.m_GainAlbum, o.m_ArtworkID );
	return isDuplicate;
}

bool MediaInfo::GetCommonInfo( const List& mediaList, MediaInfo& commonInfo )
{
	commonInfo = MediaInfo();

	std::set<std::wstring> artists;
	std::set<std::wstring> titles;
	std::set<std::wstring> albums;
	std::set<std::wstring> genres;
	std::set<std::wstring> comments;
	std::set<std::wstring> artworks;
	std::set<long> years;
	std::set<long> tracks;

	for ( const auto& mediaInfo : mediaList ) {
		artists.insert( mediaInfo.GetArtist() );
		titles.insert( mediaInfo.GetTitle() );
		albums.insert( mediaInfo.GetAlbum() );
		genres.insert( mediaInfo.GetGenre() );
		comments.insert( mediaInfo.GetComment() );
		artworks.insert( mediaInfo.GetArtworkID() );
		years.insert( mediaInfo.GetYear() );
		tracks.insert( mediaInfo.GetTrack() );

		if ( ( artists.size() > 1 ) && ( titles.size() > 1 ) && ( albums.size() > 1 ) && ( genres.size() > 1 ) && ( comments.size() > 1 ) && ( artworks.size() > 1 ) && ( years.size() > 1 ) && ( tracks.size() > 1 ) ) {
			break;
		}
	}

	bool hasCommonInfo = false;

	if ( ( 1 == artists.size() ) && !artists.begin()->empty() ) {
		commonInfo.SetArtist( *artists.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == titles.size() ) && !titles.begin()->empty() ) {
		commonInfo.SetTitle( *titles.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == albums.size() ) && !albums.begin()->empty() ) {
		commonInfo.SetAlbum( *albums.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == genres.size() ) && !genres.begin()->empty() ) {
		commonInfo.SetGenre( *genres.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == comments.size() ) && !comments.begin()->empty() ) {
		commonInfo.SetComment( *comments.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == artworks.size() ) && !artworks.begin()->empty() ) {
		commonInfo.SetArtworkID( *artworks.begin() );
		hasCommonInfo = true;
	}
	if ( 1 == years.size() ) {
		commonInfo.SetYear( *years.begin() );
		hasCommonInfo = true;
	}
	if ( 1 == tracks.size() ) {
		commonInfo.SetTrack( *tracks.begin() );
		hasCommonInfo = true;
	}

	return hasCommonInfo;
}
