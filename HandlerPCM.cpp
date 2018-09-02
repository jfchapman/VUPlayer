#include "HandlerPCM.h"

#include "EncoderPCM.h"

HandlerPCM::HandlerPCM() :
	Handler()
{
}

HandlerPCM::~HandlerPCM()
{
}

std::wstring HandlerPCM::GetDescription() const
{
	std::wstring description = L"Waveform Audio File (PCM)";
	return description;
}

std::set<std::wstring> HandlerPCM::GetSupportedFileExtensions() const
{
	return std::set<std::wstring>();
}

bool HandlerPCM::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerPCM::SetTags( const std::wstring& /*filename*/, const Handler::Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerPCM::OpenDecoder( const std::wstring& /*filename*/ ) const
{
	return nullptr;
}

Encoder::Ptr HandlerPCM::OpenEncoder() const
{
	Encoder::Ptr encoder( new EncoderPCM() );
	return encoder;
}

bool HandlerPCM::IsDecoder() const
{
	return false;
}

bool HandlerPCM::IsEncoder() const
{
	return true;
}

bool HandlerPCM::CanConfigureEncoder() const
{
	return false;
}

bool HandlerPCM::ConfigureEncoder( const HINSTANCE /*instance*/, const HWND /*parent*/, std::string& /*settings*/ ) const
{
	return false;
}
