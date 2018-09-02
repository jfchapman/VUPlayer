#include "Handlers.h"

#include "HandlerBass.h"
#include "HandlerFlac.h"
#include "HandlerMP3.h"
#include "HandlerOpus.h"
#include "HandlerPCM.h"
#include "HandlerWavpack.h"

#include "ShellMetadata.h"
#include "Utility.h"

Handlers::Handlers() :
	m_Handlers( {
		Handler::Ptr( new HandlerFlac() ),
		Handler::Ptr( new HandlerWavpack() ),
		Handler::Ptr( new HandlerOpus() ),
		Handler::Ptr( new HandlerMP3() ),
		Handler::Ptr( new HandlerPCM() ),
		Handler::Ptr( new HandlerBass() ) } ),
	m_Decoders(),
	m_Encoders()
{
	for ( const auto& handler : m_Handlers ) {
		if ( handler->IsDecoder() ) {
			m_Decoders.push_back( handler );
		}
		if ( handler->IsEncoder() ) {
			m_Encoders.push_back( handler );
		}
	}
}

Handlers::~Handlers()
{
}

Handler::Ptr Handlers::FindDecoderHandler( const std::wstring& filename ) const
{
	Handler::Ptr handler;
	const size_t extPos = filename.rfind( '.' );
	if ( std::wstring::npos != extPos ) {
		const std::wstring fileExt = WideStringToLower( filename.substr( extPos + 1 ) );
		for ( auto handlerIter = m_Decoders.begin(); !handler && ( handlerIter != m_Decoders.end() ); handlerIter++ ) {
			std::set<std::wstring> extensions = handlerIter->get()->GetSupportedFileExtensions();
			for ( const auto& extIter : extensions ) {
				if ( 0 == WideStringToLower( extIter ).compare( fileExt ) ) {
					handler = *handlerIter; 
					break;
				}
			}
		}
	}
	return handler;
}

Decoder::Ptr Handlers::OpenDecoder( const std::wstring& filename ) const
{
	Decoder::Ptr decoder;
	Handler::Ptr handler = FindDecoderHandler( filename );
	if ( handler ) {
		decoder = handler->OpenDecoder( filename );
	}
	if ( !decoder ) {
		// Try any handler.
		for ( auto handlerIter = m_Decoders.begin(); !decoder && ( handlerIter != m_Decoders.end() ); handlerIter++ ) {
			decoder = handlerIter->get()->OpenDecoder( filename );
		}
	}
	return decoder;
}

bool Handlers::GetTags( const std::wstring& filename, Handler::Tags& tags ) const
{
	bool success = false;
	tags.clear();
	Handler::Ptr handler = FindDecoderHandler( filename );
	if ( handler ) {
		success = handler->GetTags( filename, tags );
	}
	if ( !success ) {
		success = ShellMetadata::Get( filename, tags );
	}
	return success;
}

bool Handlers::SetTags( const std::wstring& filename, const Handler::Tags& tags ) const
{
	bool success = false;
	Handler::Ptr handler = FindDecoderHandler( filename );
	if ( handler ) {
		success = handler->SetTags( filename, tags );
	}
	if ( !success ) {
		success = ShellMetadata::Set( filename, tags );
	}
	return success;
}

std::set<std::wstring> Handlers::GetAllSupportedFileExtensions() const
{
	std::set<std::wstring> fileExtensions;
	for ( const auto& handler : m_Decoders ) {
		std::set<std::wstring> handlerExtensions = handler->GetSupportedFileExtensions();
		fileExtensions.insert( handlerExtensions.begin(), handlerExtensions.end() );
	}
	return fileExtensions;
}

std::wstring Handlers::GetBassVersion() const
{
	std::wstring version;
	for ( const auto& handler : m_Decoders ) {
		if ( nullptr != dynamic_cast<HandlerBass*>( handler.get() ) ) {
			version = handler->GetDescription();
			break;
		}
	}
	return version; 
}

void Handlers::AddHandler( Handler::Ptr handler )
{
	if ( handler ) {
		m_Handlers.push_back( handler );
		if ( handler->IsDecoder() ) {
			m_Decoders.push_back( handler );
		}
		if ( handler->IsEncoder() ) {
			m_Encoders.push_back( handler );
		}
	}
}

Handler::List Handlers::GetEncoders() const
{
	return m_Encoders;
}

Encoder::Ptr Handlers::OpenEncoder( const std::wstring& /*description*/ ) const
{
	Encoder::Ptr encoder;
	return encoder;
}
