#include "MediaInfo.h"

#include "Utility.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <set>
#include <tuple>
#include <regex>

MediaInfo::MediaInfo( const std::wstring& filename ) :
	m_Filename( filename )
{
}

MediaInfo::MediaInfo( const long cddbID ) :
	m_Source( Source::CDDA ),
	m_CDDB( cddbID )
{
}

bool MediaInfo::operator<( const MediaInfo& o ) const
{
	const bool lessThan =
		std::tie( m_Filename, m_Filetime, m_Filesize, m_Duration, m_SampleRate, m_BitsPerSample, m_Channels, m_Bitrate,
			m_Artist, m_Title, m_Album, m_Genre, m_Year, m_Comment, m_Track, m_Version, m_ArtworkID, m_Composer, m_Conductor, m_Publisher,
			m_Source, m_CDDB, m_GainTrack, m_GainAlbum, m_CueStart, m_CueEnd, m_PlayCount ) <

		std::tie( o.m_Filename, o.m_Filetime, o.m_Filesize, o.m_Duration, o.m_SampleRate, o.m_BitsPerSample, o.m_Channels, o.m_Bitrate,
			o.m_Artist, o.m_Title, o.m_Album, o.m_Genre, o.m_Year, o.m_Comment, o.m_Track, o.m_Version, o.m_ArtworkID, o.m_Composer, o.m_Conductor, o.m_Publisher,
			o.m_Source, o.m_CDDB, o.m_GainTrack, o.m_GainAlbum, o.m_CueStart, o.m_CueEnd, o.m_PlayCount );

	return lessThan;
}

bool MediaInfo::operator==( const MediaInfo& o ) const
{
	const bool equals =
		std::tie( m_Filename, m_Filetime, m_Filesize, m_Duration, m_SampleRate, m_BitsPerSample, m_Channels, m_Bitrate,
			m_Artist, m_Title, m_Album, m_Genre, m_Year, m_Comment, m_Track, m_Version, m_ArtworkID, m_Composer, m_Conductor, m_Publisher,
			m_Source, m_CDDB, m_GainTrack, m_GainAlbum, m_CueStart, m_CueEnd, m_PlayCount ) ==

		std::tie( o.m_Filename, o.m_Filetime, o.m_Filesize, o.m_Duration, o.m_SampleRate, o.m_BitsPerSample, o.m_Channels, o.m_Bitrate,
			o.m_Artist, o.m_Title, o.m_Album, o.m_Genre, o.m_Year, o.m_Comment, o.m_Track, o.m_Version, o.m_ArtworkID, o.m_Composer, o.m_Conductor, o.m_Publisher,
			o.m_Source, o.m_CDDB, o.m_GainTrack, o.m_GainAlbum, o.m_CueStart, o.m_CueEnd, o.m_PlayCount );

	return equals;
}

bool MediaInfo::operator!=( const MediaInfo& o ) const
{
	return !operator==( o );
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
	if ( !m_Version.empty() ) {
		tags.insert( Tags::value_type( Tag::Version, WideStringToUTF8( m_Version ) ) );
	}
	if ( !m_Composer.empty() ) {
		tags.insert( Tags::value_type( Tag::Composer, WideStringToUTF8( m_Composer ) ) );
	}
	if ( !m_Conductor.empty() ) {
		tags.insert( Tags::value_type( Tag::Conductor, WideStringToUTF8( m_Conductor ) ) );
	}
	if ( !m_Publisher.empty() ) {
		tags.insert( Tags::value_type( Tag::Publisher, WideStringToUTF8( m_Publisher ) ) );
	}
	if ( m_Track > 0 ) {
		tags.insert( Tags::value_type( Tag::Track, std::to_string( m_Track ) ) );
	}
	if ( m_Year > 0 ) {
		tags.insert( Tags::value_type( Tag::Year, std::to_string( m_Year ) ) );
	}
	const std::string gainAlbum = GainToString( GetModuleHandle( nullptr ), m_GainAlbum );
	if ( !gainAlbum.empty() ) {
		tags.insert( Tags::value_type( Tag::GainAlbum, gainAlbum ) );
	}
	const std::string gainTrack = GainToString( GetModuleHandle( nullptr ), m_GainTrack );
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

long long MediaInfo::GetFilesize( const bool applyCues ) const
{
	if ( m_CueStart && applyCues && ( m_Duration > 0 ) ) {
		const float start = *m_CueStart / 75.f;
		const float end = m_CueEnd ? ( *m_CueEnd / 75.f ) : m_Duration;
		return std::clamp( static_cast<long long>( m_Filesize * ( end - start ) / m_Duration ), 0ll, m_Filesize );
	}
	return m_Filesize;
}

void MediaInfo::SetFilesize( const long long filesize )
{
	m_Filesize = filesize;
}

float MediaInfo::GetDuration( const bool applyCues ) const
{
	if ( m_CueStart && applyCues ) {
		const float start = *m_CueStart / 75.f;
		const float end = m_CueEnd ? ( *m_CueEnd / 75.f ) : m_Duration;
		return ( start <= end ) ? ( end - start ) : 0.f;
	}
	return m_Duration;
}

void MediaInfo::SetDuration( const float duration )
{
	m_Duration = ( std::isfinite( duration ) && ( duration > 0 ) ) ? duration : 0;
}

long MediaInfo::GetSampleRate() const
{
	return m_SampleRate;
}

void MediaInfo::SetSampleRate( const long sampleRate )
{
	m_SampleRate = sampleRate;
}

std::optional<long> MediaInfo::GetBitsPerSample() const
{
	return m_BitsPerSample;
}

void MediaInfo::SetBitsPerSample( const std::optional<long> bitsPerSample )
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

const std::wstring& MediaInfo::GetComposer() const
{
	return m_Composer;
}

void MediaInfo::SetComposer( const std::wstring& composer )
{
	m_Composer = composer;
}

const std::wstring& MediaInfo::GetConductor() const
{
	return m_Conductor;
}

void MediaInfo::SetConductor( const std::wstring& conductor )
{
	m_Conductor = conductor;
}

const std::wstring& MediaInfo::GetPublisher() const
{
	return m_Publisher;
}

void MediaInfo::SetPublisher( const std::wstring& publisher )
{
	m_Publisher = publisher;
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

std::optional<float> MediaInfo::GetGainTrack() const
{
	return m_GainTrack;
}

void MediaInfo::SetGainTrack( const std::optional<float> gain )
{
	m_GainTrack = ( gain.has_value() && std::isfinite( gain.value() ) ) ? gain : std::nullopt;
}

std::optional<float> MediaInfo::GetGainAlbum() const
{
	return m_GainAlbum;
}

void MediaInfo::SetGainAlbum( const std::optional<float> gain )
{
	m_GainAlbum = ( gain.has_value() && std::isfinite( gain.value() ) ) ? gain : std::nullopt;
}

std::wstring MediaInfo::GetTitle( const bool filenameAsTitle ) const
{
	std::wstring title = m_Title;
	if ( title.empty() && filenameAsTitle ) {
		const std::filesystem::path path( m_Filename );
		title = IsURL( m_Filename ) ? path.filename().native() : GetFilenameWithCues( false /*fullPath*/, true /*removeExtension*/ );
	}
	return title;
}

std::wstring MediaInfo::GetType() const
{
	std::wstring type;
	if ( !IsURL( m_Filename ) ) {
		const size_t pos = m_Filename.rfind( '.' );
		if ( std::wstring::npos != pos ) {
			type = WideStringToUpper( m_Filename.substr( pos + 1 /*offset*/ ) );
		}
	}
	return type;
}

std::optional<float> MediaInfo::GetBitrate( const bool calculate ) const
{
	std::optional<float> bitrate = m_Bitrate;
	if ( calculate && !bitrate.has_value() && ( m_Duration > 0 ) ) {
		bitrate = ( m_Filesize * 8 ) / ( m_Duration * 1000 );
	}
	return bitrate;
}

void MediaInfo::SetBitrate( const std::optional<float> bitrate )
{
	m_Bitrate = ( bitrate.has_value() && std::isfinite( bitrate.value() ) ) ? bitrate : std::nullopt;
}

std::wstring MediaInfo::GetArtworkID( const bool checkFolder ) const
{
	std::wstring artworkID = m_ArtworkID;
	if ( checkFolder && artworkID.empty() && !GetFilename().empty() && ( Source::File == GetSource() ) ) {
		const std::array<std::wstring, 2> artworkFileNames = { L"cover", L"folder" };
		const std::array<std::wstring, 2> artworkFileTypes = { L"jpg", L"png" };
		const std::filesystem::path filePath( GetFilename() );
		for ( auto artworkFileName = artworkFileNames.begin(); artworkID.empty() && ( artworkFileNames.end() != artworkFileName ); artworkFileName++ ) {
			for ( auto artworkFileType = artworkFileTypes.begin(); artworkID.empty() && ( artworkFileTypes.end() != artworkFileType ); artworkFileType++ ) {
				std::filesystem::path artworkPath = filePath.parent_path() / *artworkFileName;
				artworkPath.replace_extension( *artworkFileType );
				if ( std::filesystem::exists( artworkPath ) ) {
					artworkID = artworkPath;
					break;
				}
			}
		}
	}
	return artworkID;
}

void MediaInfo::SetArtworkID( const std::wstring& id )
{
	m_ArtworkID = id;
}

const std::optional<long>& MediaInfo::GetCueStart() const
{
	return m_CueStart;
}

void MediaInfo::SetCueStart( const std::optional<long>& frames )
{
	m_CueStart = ( frames.has_value() && ( frames.value() < 0 ) ) ? std::nullopt : frames;
}

const std::optional<long>& MediaInfo::GetCueEnd() const
{
	return m_CueEnd;
}

void MediaInfo::SetCueEnd( const std::optional<long>& frames )
{
	m_CueEnd = ( frames.has_value() && ( frames.value() < 0 ) ) ? std::nullopt : frames;
}

long MediaInfo::GetPlayCount() const
{
	return m_PlayCount;
}

void MediaInfo::SetPlayCount( const long count )
{
	m_PlayCount = count;
}

void MediaInfo::IncrementPlayCount()
{
	++m_PlayCount;
}

std::wstring MediaInfo::GetFilenameWithCues( const bool fullPath, const bool removeExtension ) const
{
	const auto filepath = ( fullPath && !removeExtension ) ? std::filesystem::path( m_Filename ) : std::filesystem::path( m_Filename ).filename();
	return FormatCues( m_CueStart, m_CueEnd, removeExtension ? filepath.stem() : filepath );
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
		std::tie( m_Filesize, m_Duration, m_SampleRate, m_Channels,
			m_Artist, m_Title, m_Album, m_Genre, m_Year, m_Comment, m_Track, m_Version, m_ArtworkID, m_Composer, m_Conductor, m_Publisher,
			m_Source, m_CDDB, m_GainTrack, m_GainAlbum, m_CueStart, m_CueEnd ) ==

		std::tie( o.m_Filesize, o.m_Duration, o.m_SampleRate, o.m_Channels,
			o.m_Artist, o.m_Title, o.m_Album, o.m_Genre, o.m_Year, o.m_Comment, o.m_Track, o.m_Version, o.m_ArtworkID, o.m_Composer, o.m_Conductor, o.m_Publisher,
			o.m_Source, o.m_CDDB, o.m_GainTrack, o.m_GainAlbum, o.m_CueStart, o.m_CueEnd );
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
	std::set<std::wstring> composers;
	std::set<std::wstring> conductors;
	std::set<std::wstring> publishers;
	std::set<long> years;
	std::set<long> tracks;

	for ( const auto& mediaInfo : mediaList ) {
		artists.insert( mediaInfo.GetArtist() );
		titles.insert( mediaInfo.GetTitle() );
		albums.insert( mediaInfo.GetAlbum() );
		genres.insert( mediaInfo.GetGenre() );
		comments.insert( mediaInfo.GetComment() );
		artworks.insert( mediaInfo.GetArtworkID() );
		composers.insert( mediaInfo.GetComposer() );
		conductors.insert( mediaInfo.GetConductor() );
		publishers.insert( mediaInfo.GetPublisher() );
		years.insert( mediaInfo.GetYear() );
		tracks.insert( mediaInfo.GetTrack() );

		if ( ( artists.size() > 1 ) && ( titles.size() > 1 ) && ( albums.size() > 1 ) && ( genres.size() > 1 ) && ( comments.size() > 1 ) && ( artworks.size() > 1 ) &&
			( composers.size() > 1 ) && ( conductors.size() > 1 ) && ( publishers.size() > 1 ) && ( years.size() > 1 ) && ( tracks.size() > 1 ) ) {
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
	if ( ( 1 == composers.size() ) && !composers.begin()->empty() ) {
		commonInfo.SetComposer( *composers.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == conductors.size() ) && !conductors.begin()->empty() ) {
		commonInfo.SetConductor( *conductors.begin() );
		hasCommonInfo = true;
	}
	if ( ( 1 == publishers.size() ) && !publishers.begin()->empty() ) {
		commonInfo.SetPublisher( *publishers.begin() );
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

std::wstring MediaInfo::FormatCues( const std::optional<long>& cueStart, const std::optional<long>& cueEnd, const std::wstring& value )
{
	if ( !cueStart )
		return value;

	const int startMinutes = *cueStart / 75 / 60;
	const int startSeconds = *cueStart / 75 % 60;
	const int startFrames = *cueStart % 75;
	const int endMinutes = cueEnd ? ( *cueEnd / 75 / 60 ) : 99;
	const int endSeconds = cueEnd ? ( *cueEnd / 75 % 60 ) : 99;
	const int endFrames = cueEnd ? ( *cueEnd % 75 ) : 99;
	return std::format( LR"({} [{:02}:{:02}:{:02} - {:02}:{:02}:{:02}])", value, startMinutes, startSeconds, startFrames, endMinutes, endSeconds, endFrames );
}

MediaInfo MediaInfo::ExtractCues( const std::wstring& filepath )
{
	const std::wregex kCues( LR"((.+)\[(\d{2}):(\d{2}):(\d{2}) - (\d{2}):(\d{2}):(\d{2})\]$)" );
	std::wsmatch match;
	if ( std::regex_match( filepath, match, kCues ) && ( 8 == match.size() ) ) {
		try {
			MediaInfo info( StripWhitespace( match[ 1 ].str() ) );
			const int startMinutes = std::stoi( match[ 2 ] );
			const int startSeconds = std::stoi( match[ 3 ] );
			const int startFrames = std::stoi( match[ 4 ] );
			const int endMinutes = std::stoi( match[ 5 ] );
			const int endSeconds = std::stoi( match[ 6 ] );
			const int endFrames = std::stoi( match[ 7 ] );
			info.SetCueStart( startMinutes * 60 * 75 + startSeconds * 75 + startFrames );
			if ( !( ( 99 == endMinutes ) && ( 99 == endSeconds ) && ( 99 == endFrames ) ) ) {
				info.SetCueEnd( endMinutes * 60 * 75 + endSeconds * 75 + endFrames );
			}
			return info;
		} catch ( const std::logic_error& ) {}
	}
	return filepath;
}
