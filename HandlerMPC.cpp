#include "HandlerMPC.h"

#include "DecoderMPC.h"

HandlerMPC::HandlerMPC()
{
}

HandlerMPC::~HandlerMPC()
{
}

std::wstring HandlerMPC::GetDescription() const
{
	const std::wstring description = L"Musepack";
	return description;
}

std::set<std::wstring> HandlerMPC::GetSupportedFileExtensions() const
{
	const std::set<std::wstring> filetypes = { L"mpc", L"mp+", L"mpp" };
	return filetypes;
}

bool HandlerMPC::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerMPC::SetTags( const std::wstring& /*filename*/, const Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerMPC::OpenDecoder( const std::wstring& filename ) const
{
	DecoderMPC* decoderMPC = nullptr;
	try {
		decoderMPC = new DecoderMPC( filename );
	} catch ( const std::runtime_error& ) {

	}
	const Decoder::Ptr stream( decoderMPC );
	return stream;
}

Encoder::Ptr HandlerMPC::OpenEncoder() const
{
	return nullptr;
}

bool HandlerMPC::IsDecoder() const
{
	return true;
}

bool HandlerMPC::IsEncoder() const
{
	return false;
}

bool HandlerMPC::CanConfigureEncoder() const
{
	return false;
}

bool HandlerMPC::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerMPC::SettingsChanged( Settings& /*settings*/ )
{
}
