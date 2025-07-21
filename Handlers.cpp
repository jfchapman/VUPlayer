#include "Handlers.h"

#include "HandlerBass.h"
#include "HandlerFFmpeg.h"
#include "HandlerFlac.h"
#include "HandlerMP3.h"
#include "HandlerOpus.h"
#include "HandlerPCM.h"
#include "HandlerWavpack.h"
#include "HandlerOpenMPT.h"

#include "DecoderBin.h"

#include "Library.h"
#include "Settings.h"
#include "ShellMetadata.h"
#include "Utility.h"
#include "TagReader.h"

Handlers::Handlers() :
	m_HandlerBASS( new HandlerBass() ),
	m_HandlerFFmpeg( new HandlerFFmpeg() ),
#ifndef _DEBUG
	m_HandlerOpenMPT( new HandlerOpenMPT() ),
#endif
	m_Handlers( {
		Handler::Ptr( new HandlerFlac() ),
		Handler::Ptr( new HandlerOpus() ),
		Handler::Ptr( new HandlerWavpack() ),
		Handler::Ptr( new HandlerMP3() ),
		Handler::Ptr( new HandlerPCM() ),
		Handler::Ptr( m_HandlerBASS ),
#ifndef _DEBUG
		Handler::Ptr( m_HandlerOpenMPT ),
#endif
		Handler::Ptr( m_HandlerFFmpeg )
		} ),
	m_Decoders(),
	m_Encoders()
{
	for ( const auto& handler : m_Handlers ) {
		if ( handler ) {
			if ( handler->IsDecoder() ) {
				m_Decoders.push_back( handler );
			}
			if ( handler->IsEncoder() ) {
				m_Encoders.push_back( handler );
			}
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
	std::lock_guard<std::mutex> lock( m_MutexDecoders );
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

Decoder::Ptr Handlers::OpenDecoder( const MediaInfo& mediaInfo, const Decoder::Context context, const bool applyCues ) const
{
	Decoder::Ptr decoder;
	const auto& filename = mediaInfo.GetFilename();
	if ( IsURL( filename ) ) {
		decoder = m_HandlerBASS ? m_HandlerBASS->OpenDecoder( filename, context ) : nullptr;
	} else if ( !filename.empty() ) {
		if ( mediaInfo.GetCueStart() && ( L"bin" == GetFileExtension( filename ) ) ) {
			// Use the built-in raw data reader.
			try {
				decoder = std::make_shared<DecoderBin>( filename, context );
			} catch ( const std::runtime_error& ) {}
		} else {
			Handler::Ptr handler = FindDecoderHandler( filename );
			if ( handler ) {
				decoder = handler->OpenDecoder( filename, context );
			}
			if ( !decoder && m_HandlerFFmpeg && ( handler != m_HandlerFFmpeg ) ) {
				// Try the FFmpeg handler as a catch all.
				decoder = m_HandlerFFmpeg->OpenDecoder( filename, context );
			}
		}
		if ( decoder && applyCues ) {
			decoder->SetCues( mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() );
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
			TagReader tagReader( filename );
			if ( const auto fileTags = tagReader.GetTags(); fileTags.has_value() ) {
				tags = *fileTags;
				success = true;
			}
			if ( !success ) {
				if ( const auto shellTags = GetShellMetadata( filename ); shellTags.has_value() ) {
					tags = *shellTags;
					success = true;
				}
			}
		}
	}
	return success;
}

bool Handlers::SetTags( const MediaInfo& mediaInfo, Library& library ) const
{
	bool success = true;
	const auto& filename = mediaInfo.GetFilename();
	if ( m_WriteTags && ( MediaInfo::Source::File == mediaInfo.GetSource() ) && !IsURL( filename ) && !mediaInfo.GetCueStart() ) {
		Tags tagsToWrite = library.GetTags( mediaInfo );

		Tags fileTags;
		GetTags( filename, fileTags );

		for ( auto libraryTag = tagsToWrite.begin(); tagsToWrite.end() != libraryTag; ) {
			const auto fileTag = fileTags.find( libraryTag->first );
			if ( fileTags.end() != fileTag ) {
				if ( libraryTag->second == fileTag->second ) {
					libraryTag = tagsToWrite.erase( libraryTag );
				} else {
					++libraryTag;
				}
				fileTags.erase( fileTag );
			} else {
				++libraryTag;
			}
		}

		for ( const auto& [tag, value] : fileTags ) {
			tagsToWrite.insert( Tags::value_type( tag, {} ) );
		}

		if ( !tagsToWrite.empty() ) {
			const FILETIME lastModified = m_PreserveLastModifiedTime ? GetLastModifiedTime( filename ) : FILETIME();
			Handler::Ptr handler = FindDecoderHandler( filename );
			success = handler ? handler->SetTags( filename, tagsToWrite ) : false;
			if ( !success && m_HandlerFFmpeg && ( handler != m_HandlerFFmpeg ) ) {
				// Try the FFmpeg handler as a catch all.
				m_HandlerFFmpeg->SetTags( filename, tagsToWrite );
			}
			if ( success && m_PreserveLastModifiedTime ) {
				SetLastModifiedTime( filename, lastModified );
			}
		}
	}
	return success;
}

std::set<std::wstring> Handlers::GetAllSupportedFileExtensions() const
{
	std::set<std::wstring> fileExtensions;
	std::lock_guard<std::mutex> lock( m_MutexDecoders );
	for ( const auto& handler : m_Decoders ) {
		std::set<std::wstring> handlerExtensions = handler->GetSupportedFileExtensions();
		fileExtensions.insert( handlerExtensions.begin(), handlerExtensions.end() );
	}
	return fileExtensions;
}

std::wstring Handlers::GetBassVersion() const
{
	const std::wstring version = m_HandlerBASS ? m_HandlerBASS->GetDescription() : std::wstring();
	return version;
}

std::wstring Handlers::GetOpenMPTVersion() const
{
	const std::wstring version = m_HandlerOpenMPT ? m_HandlerOpenMPT->GetDescription() : std::wstring();
	return version;
}

void Handlers::AddHandler( Handler::Ptr handler )
{
	if ( handler ) {
		m_Handlers.push_back( handler );
		if ( handler->IsDecoder() ) {
			std::lock_guard<std::mutex> lock( m_MutexDecoders );
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

void Handlers::SettingsChanged( Settings& settings )
{
	for ( const auto& handler : m_Handlers ) {
		if ( handler ) {
			handler->SettingsChanged( settings );
		}
	}
	m_WriteTags = settings.GetWriteFileTags();
	m_PreserveLastModifiedTime = settings.GetPreserveLastModifiedTime();

	if ( m_HandlerOpenMPT ) {
		std::lock_guard<std::mutex> lock( m_MutexDecoders );
		auto bassDecoder = std::find( m_Decoders.begin(), m_Decoders.end(), m_HandlerBASS );
		auto openMPTDecoder = std::find( m_Decoders.begin(), m_Decoders.end(), m_HandlerOpenMPT );
		if ( ( m_Decoders.end() != bassDecoder ) && ( m_Decoders.end() != openMPTDecoder ) ) {
			const Settings::MODDecoder preferredDecoder = ( std::distance( bassDecoder, openMPTDecoder ) > 0 ) ? Settings::MODDecoder::BASS : Settings::MODDecoder::OpenMPT;
			const Settings::MODDecoder preferredSetting = settings.GetMODDecoder();
			if ( preferredDecoder != preferredSetting ) {
				std::iter_swap( bassDecoder, openMPTDecoder );
			}
		}
	}
}

void Handlers::Init( Settings& settings )
{
	SettingsChanged( settings );
}
