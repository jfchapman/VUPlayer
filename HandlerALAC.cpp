#include "HandlerALAC.h"

#include "EncoderALAC.h"
#include "ShellMetadata.h"

HandlerALAC::HandlerALAC() :
	Handler()
{
}

HandlerALAC::~HandlerALAC()
{
}

std::wstring HandlerALAC::GetDescription() const
{
	constexpr wchar_t description[] = L"Apple Lossless (ALAC)";
	return description;
}

std::set<std::wstring> HandlerALAC::GetSupportedFileExtensions() const
{
	return { L"m4a" };
}

bool HandlerALAC::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerALAC::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	return SetShellMetadata( filename, tags );
}

Decoder::Ptr HandlerALAC::OpenDecoder( const std::wstring& /*filename*/, const Decoder::Context /*context*/ ) const
{
	return nullptr;
}

Encoder::Ptr HandlerALAC::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderALAC() );
	return encoder;
}

bool HandlerALAC::IsDecoder() const
{
	return false;
}

bool HandlerALAC::IsEncoder() const
{
	return true;
}

bool HandlerALAC::CanConfigureEncoder() const
{
	return false;
}

bool HandlerALAC::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerALAC::SettingsChanged( Settings& /*settings*/ )
{
}
