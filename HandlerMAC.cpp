#include "HandlerMAC.h"

#include "DecoderMAC.h"

// Supported APE tags.
static const std::map<std::wstring,Tag> s_SupportedAPETags = {
	{ L"ALBUM",									Tag::Album },
	{ L"ARTIST",								Tag::Artist },
	{ L"COMMENT",								Tag::Comment },
	{ L"REPLAYGAIN_ALBUM_GAIN", Tag::GainAlbum },
	{ L"REPLAYGAIN_TRACK_GAIN", Tag::GainTrack },
	{ L"GENRE",									Tag::Genre },
	{ L"TITLE",									Tag::Title },
	{ L"TRACKNUMBER",						Tag::Track },
	{ L"DATE",									Tag::Year }
};

HandlerMAC::HandlerMAC()
{
}

HandlerMAC::~HandlerMAC()
{
}

std::wstring HandlerMAC::GetDescription() const
{
	const std::wstring description = L"Monkey's Audio";
	return description;
}

std::set<std::wstring> HandlerMAC::GetSupportedFileExtensions() const
{
	const std::set<std::wstring> filetypes = { L"ape", L"apl" };
	return filetypes;
}

bool HandlerMAC::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	std::unique_ptr<APE::IAPEDecompress> decompress( CreateIAPEDecompress( filename.c_str() ) );
	if ( decompress ) {
		auto apeTag = reinterpret_cast<APE::CAPETag*>( decompress->GetInfo( APE::APE_INFO_TAG ) );
		if ( ( nullptr != apeTag ) && apeTag->GetHasAPETag() ) {
			int tagIndex = 0;
			auto tagField = apeTag->GetTagField( tagIndex );
			while ( nullptr != tagField ) {
				const auto fieldName = tagField->GetFieldName();
				const auto tagIter = ( nullptr != fieldName ) ? s_SupportedAPETags.find( fieldName ) : s_SupportedAPETags.end();
				if ( s_SupportedAPETags.end() != tagIter ) {
					const auto fieldValue = tagField->GetFieldValue();
					const int fieldValueSize = tagField->GetFieldValueSize();
					if ( ( nullptr != fieldValue ) && ( fieldValueSize > 0 ) && tagField->GetIsUTF8Text() ) {
						const std::string value( fieldValue, fieldValueSize );
						if ( !value.empty() ) {
							tags.insert( Tags::value_type( tagIter->second, value ) );
						}
					}
				}
				tagField = apeTag->GetTagField( ++tagIndex );
			}
		}
		success = true;
	}
	return success;
}

bool HandlerMAC::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	bool success = false;
	std::unique_ptr<APE::IAPEDecompress> decompress( CreateIAPEDecompress( filename.c_str(), nullptr /*errorCode*/, false /*readOnly*/ ) );
	if ( decompress ) {
		auto apeTag = reinterpret_cast<APE::CAPETag*>( decompress->GetInfo( APE::APE_INFO_TAG ) );
		if ( nullptr != apeTag ) {
			for ( const auto& tagIter : tags ) {
				const Tag tagType = tagIter.first;
				const std::string& tagValue = tagIter.second;
				const auto fieldName = std::find_if( s_SupportedAPETags.begin(), s_SupportedAPETags.end(),
					[ tagType ] ( const std::map<std::wstring,Tag>::value_type& entry )
				{
					return ( tagType == entry.second );
				} );

				if ( s_SupportedAPETags.end() != fieldName ) {
					if ( tagValue.empty() ) {
						apeTag->RemoveField( fieldName->first.c_str() );
					} else {
						apeTag->SetFieldString( fieldName->first.c_str(), tagIter.second.c_str(), true /*UTF-8*/ );
					}
				}
			}
			success = ( ERROR_SUCCESS == apeTag->Save() );
		}
	}
	return success;
}

Decoder::Ptr HandlerMAC::OpenDecoder( const std::wstring& filename ) const
{
	DecoderMAC* decoderMAC = nullptr;
	try {
		decoderMAC = new DecoderMAC( filename );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( decoderMAC );
	return stream;
}

Encoder::Ptr HandlerMAC::OpenEncoder() const
{
	return nullptr;
}

bool HandlerMAC::IsDecoder() const
{
	return true;
}

bool HandlerMAC::IsEncoder() const
{
	return false;
}

bool HandlerMAC::CanConfigureEncoder() const
{
	return false;
}

bool HandlerMAC::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerMAC::SettingsChanged( Settings& /*settings*/ )
{
}
