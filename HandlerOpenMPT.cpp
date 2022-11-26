#include "HandlerOpenMPT.h"

#include "DecoderOpenMPT.h"

HandlerOpenMPT::HandlerOpenMPT()
{
}

HandlerOpenMPT::~HandlerOpenMPT()
{
}

std::wstring HandlerOpenMPT::GetDescription() const
{
	const std::wstring description = L"OpenMPT";
	return description;
}

std::set<std::wstring> HandlerOpenMPT::GetSupportedFileExtensions() const
{
	return { L"669", L"amf", L"ams", L"c67", L"dbm", L"digi", L"dmf", L"dsm", L"dsym", L"dtm", L"far", L"fmt", L"gdm", L"ice", L"st26", L"imf", L"it", L"itp", L"j2b", L"m15", L"stk", L"mdl", L"med", L"mo3", L"mod", L"mptm", L"mt2", L"mtm", L"mus", L"okt", L"oxm", L"plm", L"psm", L"pt36", L"ptm", L"s3m", L"sfx", L"sfx2", L"mms", L"stm", L"stx", L"stp", L"symmod", L"ult", L"umx", L"wow", L"xm" };
}

bool HandlerOpenMPT::GetTags( const std::wstring& /*filename*/, Tags& /*tags*/ ) const
{
	return false;
}

bool HandlerOpenMPT::SetTags( const std::wstring& /*filename*/, const Tags& /*tags*/ ) const
{
	return false;
}

Decoder::Ptr HandlerOpenMPT::OpenDecoder( const std::wstring& filename ) const
{
	DecoderOpenMPT* decoderOpenMPT = nullptr;
	try {
		decoderOpenMPT = new DecoderOpenMPT( filename );
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
