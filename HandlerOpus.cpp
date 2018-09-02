#include "HandlerOpus.h"

#include "DecoderOpus.h"

#include "Utility.h"

HandlerOpus::HandlerOpus() :
	Handler()
{
}

HandlerOpus::~HandlerOpus()
{
}

std::wstring HandlerOpus::GetDescription() const
{
	const std::wstring description = L"OPUS";
	return description;
}

std::set<std::wstring> HandlerOpus::GetSupportedFileExtensions() const
{
	const std::set<std::wstring> fileTypes = { L"opus" };
	return fileTypes;
}

bool HandlerOpus::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	int error = 0;
	const std::string filepath = WideStringToAnsiCodePage( filename );
	OggOpusFile* opusFile = op_open_file( filepath.c_str(), &error );
	if ( nullptr != opusFile ) {
		const OpusTags* opusTags = op_tags( opusFile, -1 /*link*/ );
		if ( nullptr != opusTags ) {
			success = true;
			const char* vendor = opusTags->vendor;
			if ( nullptr != vendor ) {
				tags.insert( Tags::value_type( Tag::Version, UTF8ToWideString( vendor ) ) );
			}
			const int commentCount = opusTags->comments;
			const int* commentLengths = opusTags->comment_lengths;
			for ( int commentIndex = 0; commentIndex < commentCount; commentIndex++ ) {
				const std::wstring comment = UTF8ToWideString( std::string( opusTags->user_comments[ commentIndex ], commentLengths[ commentIndex ] ) );
				const size_t delimiter = comment.find( '=' );
				if ( std::wstring::npos != delimiter ) {
					const std::wstring field = comment.substr( 0 /*offset*/, delimiter /*count*/ );
					const std::wstring value = comment.substr( delimiter + 1 /*offset*/ );
					if ( !field.empty() && !value.empty() ) {
						if ( 0 == _wcsicmp( field.c_str(), L"artist" ) ) {
							tags.insert( Tags::value_type( Tag::Artist, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"title" ) ) {
							tags.insert( Tags::value_type( Tag::Title, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"album" ) ) {
							tags.insert( Tags::value_type( Tag::Album, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"genre" ) ) {
							tags.insert( Tags::value_type( Tag::Genre, value ) );
						} else if ( ( 0 == _wcsicmp( field.c_str(), L"year" ) ) || ( 0 == _wcsicmp( field.c_str(), L"date" ) ) ) {
							tags.insert( Tags::value_type( Tag::Year, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"comment" ) ) {
							tags.insert( Tags::value_type( Tag::Comment, value ) );
						} else if ( ( 0 == _wcsicmp( field.c_str(), L"track" ) ) || ( 0 == _wcsicmp( field.c_str(), L"tracknumber" ) ) ) {
							tags.insert( Tags::value_type( Tag::Track, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"replaygain_track_gain" ) ) {
							tags.insert( Tags::value_type( Tag::GainTrack, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"replaygain_track_peak" ) ) {
							tags.insert( Tags::value_type( Tag::PeakTrack, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"replaygain_album_gain" ) ) {
							tags.insert( Tags::value_type( Tag::GainAlbum, value ) );
						} else if ( 0 == _wcsicmp( field.c_str(), L"replaygain_album_peak" ) ) {
							tags.insert( Tags::value_type( Tag::PeakAlbum, value ) );
						}
					}
				}
			}
		}
		op_free( opusFile );
	}
	return success;
}

bool HandlerOpus::SetTags( const std::wstring& /*filename*/, const Handler::Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerOpus::OpenDecoder( const std::wstring& filename ) const
{
	DecoderOpus* streamOpus = nullptr;
	try {
		streamOpus = new DecoderOpus( filename );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( streamOpus );
	return stream;
}

Encoder::Ptr HandlerOpus::OpenEncoder() const
{
	return nullptr;
}

bool HandlerOpus::IsDecoder() const
{
	return true;
}

bool HandlerOpus::IsEncoder() const
{
	return false;
}

bool HandlerOpus::CanConfigureEncoder() const
{
	return false;
}

bool HandlerOpus::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}
