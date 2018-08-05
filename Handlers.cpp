#include "Handlers.h"

#include "HandlerBass.h"
#include "HandlerFlac.h"
#include "HandlerOpus.h"
#include "HandlerWavpack.h"

#include "ShellMetadata.h"
#include "Utility.h"

Handlers::Handlers() :
	m_Handlers( {
		Handler::Ptr( new HandlerFlac() ),
		Handler::Ptr( new HandlerWavpack() ),
		Handler::Ptr( new HandlerOpus() ),
		Handler::Ptr( new HandlerBass() ) } )
{
}

Handlers::~Handlers()
{
}

Handler::Ptr Handlers::FindHandler( const std::wstring& filename ) const
{
	Handler::Ptr handler;
	const size_t extPos = filename.rfind( '.' );
	if ( std::wstring::npos != extPos ) {
		const std::wstring fileExt = WideStringToLower( filename.substr( extPos + 1 ) );
		for ( auto handlerIter = m_Handlers.begin(); !handler && ( handlerIter != m_Handlers.end() ); handlerIter++ ) {
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
	Handler::Ptr handler = FindHandler( filename );
	if ( handler ) {
		decoder = handler->OpenDecoder( filename );
	}
	if ( !decoder ) {
		// Try any handler.
		for ( auto handlerIter = m_Handlers.begin(); !decoder && ( handlerIter != m_Handlers.end() ); handlerIter++ ) {
			decoder = handlerIter->get()->OpenDecoder( filename );
		}
	}
	return decoder;
}

bool Handlers::GetTags( const std::wstring& filename, Handler::Tags& tags ) const
{
	bool success = false;
	tags.clear();
	Handler::Ptr handler = FindHandler( filename );
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
	Handler::Ptr handler = FindHandler( filename );
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
	for ( const auto& handler : m_Handlers ) {
		std::set<std::wstring> handlerExtensions = handler->GetSupportedFileExtensions();
		fileExtensions.insert( handlerExtensions.begin(), handlerExtensions.end() );
	}
	return fileExtensions;
}

std::wstring Handlers::GetBassVersion() const
{
	std::wstring version;
	for ( const auto& handler : m_Handlers ) {
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
	}
}
