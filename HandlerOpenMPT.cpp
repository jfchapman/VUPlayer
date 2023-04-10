#include "HandlerOpenMPT.h"

#include "DecoderOpenMPT.h"
#include "Utility.h"

HandlerOpenMPT::HandlerOpenMPT()
{
}

HandlerOpenMPT::~HandlerOpenMPT()
{
}

std::wstring HandlerOpenMPT::GetDescription() const
{
  const uint32_t version = openmpt::get_library_version();
  const uint32_t major = version >> 24;
  const uint32_t minor = ( version >> 16 ) & 0xff;
  const uint32_t patch = version & 0xff;
	const std::wstring description = L"libopenmpt " + std::to_wstring( major ) + L"." + std::to_wstring( minor ) + L"." + std::to_wstring( patch );
	return description;
}

std::set<std::wstring> HandlerOpenMPT::GetSupportedFileExtensions() const
{
  std::set<std::wstring> result;
  const auto extensions = openmpt::get_supported_extensions();
  for ( const auto& extension : extensions ) {
    result.insert( UTF8ToWideString( extension ) );
  }
  return result;
}

bool HandlerOpenMPT::GetTags( const std::wstring& filename, Tags& tags ) const
{
  std::ifstream stream( filename, std::ios::binary );
  openmpt::module mod( stream );
  if ( const auto artist = mod.get_metadata( "artist" ); !artist.empty() ) {
    tags[ Tag::Artist ] = artist;
  }
  if ( const auto title = mod.get_metadata( "title" ); !title.empty() ) {
    tags[ Tag::Title ] = title;
  }
  if ( const auto message = mod.get_metadata( "message" ); !message.empty() ) {
    tags[ Tag::Comment ] = message;
  }
  if ( const auto version = mod.get_metadata( "type_long" ); !version.empty() ) {
    tags[ Tag::Version ] = version;
  }
	return true;
}

bool HandlerOpenMPT::SetTags( const std::wstring& /*filename*/, const Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerOpenMPT::OpenDecoder( const std::wstring& filename, const Decoder::Context context ) const
{
	DecoderOpenMPT* decoderOpenMPT = nullptr;
	try {
		decoderOpenMPT = new DecoderOpenMPT( filename, context );
	} catch ( const std::exception& ) {

	}
	const Decoder::Ptr stream( decoderOpenMPT );
	return stream;
}

Encoder::Ptr HandlerOpenMPT::OpenEncoder() const
{
	return nullptr;
}

bool HandlerOpenMPT::IsDecoder() const
{
	return true;
}

bool HandlerOpenMPT::IsEncoder() const
{
	return false;
}

bool HandlerOpenMPT::CanConfigureEncoder() const
{
	return false;
}

bool HandlerOpenMPT::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerOpenMPT::SettingsChanged( Settings& /*settings*/ )
{
}
