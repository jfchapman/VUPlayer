#include "HandlerWavpack.h"

#include "resource.h"

#include "DecoderWavpack.h"
#include "Utility.h"

// Supported tags and their WavPack equivalents.
static const std::map<Tag,std::string> s_SupportedTags = {
	{ Tag::Album,				"ALBUM" },
	{ Tag::Artist,			"ARTIST" },
	{ Tag::Comment,			"COMMENT" },
	{ Tag::GainAlbum,		"REPLAYGAIN_ALBUM_GAIN" },
	{ Tag::GainTrack,		"REPLAYGAIN_TRACK_GAIN" },
	{ Tag::Genre,				"GENRE" },
	{ Tag::Title,				"TITLE" },
	{ Tag::Track,				"TRACKNUMBER" },
	{ Tag::Year,				"DATE" }
};

HandlerWavpack::HandlerWavpack()
{
}

HandlerWavpack::~HandlerWavpack()
{
}

std::wstring HandlerWavpack::GetDescription() const
{
	const std::wstring description = L"WavPack " + UTF8ToWideString( WavpackGetLibraryVersionString() );
	return description;
}

std::set<std::wstring> HandlerWavpack::GetSupportedFileExtensions() const
{
	const std::set<std::wstring> filetypes = { L"wv" };
	return filetypes;
}

bool HandlerWavpack::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	char* error = nullptr;
	const int flags = OPEN_TAGS | OPEN_WVC | OPEN_NORMALIZE | OPEN_DSD_AS_PCM | OPEN_FILE_UTF8;
	const int offset = 0;
	WavpackContext* context = WavpackOpenFileInput( WideStringToUTF8( filename ).c_str(), error, flags, offset );
	if ( nullptr != context ) {
		const int tagCount = WavpackGetNumTagItems( context );
		for ( const auto& tagIter : s_SupportedTags ) {
			const std::string& tagField = tagIter.second;
			const int tagLength = WavpackGetTagItem( context, tagField.c_str(), nullptr /*buffer*/, 0 /*bufferSize*/ );
			if ( tagLength > 0 ) {
				char* buffer = new char[ tagLength + 1 ];
				memset( buffer, 0 /*value*/, tagLength + 1 );
				if ( tagLength == WavpackGetTagItem( context, tagField.c_str(), buffer, tagLength + 1 ) ) {
					tags.insert( Tags::value_type( tagIter.first, buffer ) );
				}
				delete [] buffer;
			}
		}

		const unsigned char format = WavpackGetFileFormat( context );
		UINT stringID = 0;
		switch ( format ) {
			case WP_FORMAT_WAV : {
				const int mode = WavpackGetMode( context );
				stringID = ( mode & MODE_LOSSLESS ) ? ( ( mode & MODE_HYBRID ) ? IDS_WAVPACK_HYBRID : IDS_WAVPACK_LOSSLESS ) : IDS_WAVPACK_LOSSY;
				break;
			}
			case WP_FORMAT_W64 : {
				stringID = IDS_WAVPACK_W64;
				break;
			}
			case WP_FORMAT_CAF : {
				stringID = IDS_WAVPACK_CAF;
				break;
			}
			case WP_FORMAT_DFF : {
				stringID = IDS_WAVPACK_DFF;
				break;
			}
			case WP_FORMAT_DSF : {
				stringID = IDS_WAVPACK_DSD;
				break;
			}
			default : {
				break;
			}
		}
		if ( 0 != stringID ) {
			const int bufferSize = 32;
			char buffer[ bufferSize ] = {};
			if ( 0 != LoadStringA( GetModuleHandle( NULL ), stringID, buffer, bufferSize ) ) {
				tags.insert( Tags::value_type( Tag::Version, buffer ) );
			}
		}

		WavpackCloseFile( context );
		success = true;
	}
	return success;
}

bool HandlerWavpack::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	bool success = false;
	char* error = nullptr;
	const int flags = OPEN_TAGS | OPEN_EDIT_TAGS | OPEN_WVC | OPEN_NORMALIZE | OPEN_DSD_AS_PCM | OPEN_FILE_UTF8;
	const int offset = 0;
	WavpackContext* context = WavpackOpenFileInput( WideStringToUTF8( filename ).c_str(), error, flags, offset );
	if ( nullptr != context ) {
		success = true;
		bool writeTags = false;
		for ( const auto& tagIter : tags ) {
			const Tag tagField = tagIter.first;
			const auto mapIter = s_SupportedTags.find( tagField );
			if ( s_SupportedTags.end() != mapIter ) {
				const Tag mapField = mapIter->first;
				const std::string& fieldName = mapIter->second;
				const std::string& value = tagIter.second;
				if ( value.empty() ) {
					WavpackDeleteTagItem( context, fieldName.c_str() );
					writeTags = true;
				} else {
					WavpackAppendTagItem( context, fieldName.c_str(), value.c_str(), static_cast<int>( value.size() ) );
					writeTags = true;
				}
			}
		}
		if ( writeTags ) {
			success = ( 0 != WavpackWriteTag( context ) );
		}
		WavpackCloseFile( context );
	}
	return success;
}

Decoder::Ptr HandlerWavpack::OpenDecoder( const std::wstring& filename ) const
{
	DecoderWavpack* decoderWavpack = nullptr;
	try {
		decoderWavpack = new DecoderWavpack( filename );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( decoderWavpack );
	return stream;
}

Encoder::Ptr HandlerWavpack::OpenEncoder() const
{
	return nullptr;
}

bool HandlerWavpack::IsDecoder() const
{
	return true;
}

bool HandlerWavpack::IsEncoder() const
{
	return false;
}

bool HandlerWavpack::CanConfigureEncoder() const
{
	return false;
}

bool HandlerWavpack::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerWavpack::SettingsChanged( Settings& /*settings*/ )
{
}
