#include "Handlers.h"

#include "HandlerBass.h"
#include "HandlerFlac.h"
#include "HandlerMP3.h"
#include "HandlerOpus.h"
#include "HandlerPCM.h"
#include "HandlerWavpack.h"
#include "HandlerMAC.h"
#include "HandlerMPC.h"

#include "ShellMetadata.h"
#include "Utility.h"

Handlers::Handlers() :
	m_HandlerBASS( new HandlerBass() ),
	m_Handlers( {
		Handler::Ptr( new HandlerFlac() ),
		Handler::Ptr( new HandlerOpus() ),
		Handler::Ptr( new HandlerWavpack() ),
		Handler::Ptr( new HandlerMAC() ),
		Handler::Ptr( new HandlerMPC() ),
		Handler::Ptr( new HandlerMP3() ),
		Handler::Ptr( new HandlerPCM() ),
		Handler::Ptr( m_HandlerBASS ) } ),
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
	const std::wstring extension = GetFileExtension( filename );
	for ( auto decoder = m_Decoders.begin(); !handler && ( decoder != m_Decoders.end() ); decoder++ ) {
		std::set<std::wstring> extensions = decoder->get()->GetSupportedFileExtensions();
		const auto foundHandler = std::find( extensions.begin(), extensions.end(), extension );
		if ( extensions.end() != foundHandler ) {
			handler = *decoder;
			break;
		}
	}
	return handler;
}

Decoder::Ptr Handlers::OpenDecoder( const std::wstring& filename ) const
{
	Decoder::Ptr decoder;
	if ( IsURL( filename ) ) {
		decoder = m_HandlerBASS ? m_HandlerBASS->OpenDecoder( filename ) : nullptr;
	} else if ( !filename.empty() ) {
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
	}
	return decoder;
}

bool Handlers::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	if ( !IsURL( filename ) ) {
		tags.clear();
		Handler::Ptr handler = FindDecoderHandler( filename );
		if ( handler ) {
			success = handler->GetTags( filename, tags );
		}
		if ( !success ) {
			// Try any handler.
			for ( auto handlerIter = m_Decoders.begin(); !success && ( handlerIter != m_Decoders.end() ); handlerIter++ ) {
				success = handlerIter->get()->GetTags( filename, tags );
			}
		}
		if ( !success ) {
			success = ShellMetadata::Get( filename, tags );
		}
	}
	return success;
}

bool Handlers::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	bool success = false;
	if ( !IsURL( filename ) ) {
		Handler::Ptr handler = FindDecoderHandler( filename );
		if ( handler ) {
			success = handler->SetTags( filename, tags );
		}
		if ( !success ) {
			// Try any handler.
			for ( auto handlerIter = m_Decoders.begin(); !success && ( handlerIter != m_Decoders.end() ); handlerIter++ ) {
				success = handlerIter->get()->SetTags( filename, tags );
			}
		}
		if ( !success ) {
			success = ShellMetadata::Set( filename, tags );
		}
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

void Handlers::SettingsChanged( Settings& settings ) const
{
	for ( const auto& handler : m_Handlers ) {
		if ( handler ) {
			handler->SettingsChanged( settings );
		}
	}
}

void Handlers::Init( Settings& settings )
{
	SettingsChanged( settings );
}
