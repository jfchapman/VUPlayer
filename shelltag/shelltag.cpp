#include "shelltag.h"

#include "ShellMetadata.h"
#include "Utility.h"

#include <wrl.h>

// Audio format descriptions
ShellTag::AudioFormatMap ShellTag::s_AudioFormatDescriptions;

// Maximum thumbnail size
constexpr int kMaxThumbnailBytes = 0x1000000;

DWORD ShellTag::Get( const std::wstring& filename, Tags& tags )
{
	tags.clear();
	Microsoft::WRL::ComPtr<IShellItem2> item;
	HRESULT hr = SHCreateItemFromParsingName( filename.c_str(), NULL /*bindContext*/, IID_PPV_ARGS( &item ) );
	if ( SUCCEEDED( hr ) ) {
		const GETPROPERTYSTOREFLAGS flags = GPS_DEFAULT;
		Microsoft::WRL::ComPtr<IPropertyStore> propStore;
		hr = item->GetPropertyStore( flags, IID_PPV_ARGS( &propStore ) );
		if ( SUCCEEDED( hr ) ) {
			DWORD propCount = 0;
			hr = propStore->GetCount( &propCount );
			if ( SUCCEEDED( hr ) ) {
				for ( DWORD i = 0; i < propCount; i++ ) {
					PROPERTYKEY propKey = {};
					if ( SUCCEEDED( propStore->GetAt( i, &propKey ) ) ) {
						PROPVARIANT propVar = {};
						if ( SUCCEEDED( propStore->GetValue( propKey, &propVar ) ) ) {
							if ( PKEY_Music_AlbumArtist == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Artist, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_Artist == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.erase( Tag::Artist );
									tags.insert( Tags::value_type( Tag::Artist, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Title == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Title, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_Genre == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Genre, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_AlbumTitle == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Album, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Comment == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Comment, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Audio_Format == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									const std::wstring version = ShellTag::GetAudioSubType( value );
									if ( !version.empty() ) {
										tags.insert( Tags::value_type( Tag::Version, WideStringToUTF8( version ) ) );
									}
								}
							} else if ( PKEY_Media_Year == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Year, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_TrackNumber == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Track, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_Composer == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Composer, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Music_Conductor == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Conductor, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_Media_Publisher == propKey ) {
								const std::wstring value = PropertyToString( propVar );
								if ( !value.empty() ) {
									tags.insert( Tags::value_type( Tag::Publisher, WideStringToUTF8( value ) ) );
								}
							} else if ( PKEY_ThumbnailStream == propKey ) {
								if ( VT_STREAM == propVar.vt ) {
									IStream* stream = propVar.pStream;
									if ( nullptr != stream ) {
										STATSTG stats = {};
										if ( SUCCEEDED( stream->Stat( &stats, STATFLAG_NONAME ) ) ) {
											if ( stats.cbSize.QuadPart <= kMaxThumbnailBytes ) {
												ULONG bytesRead = 0;
												std::vector<BYTE> buffer( static_cast<size_t>( stats.cbSize.QuadPart ) );
												if ( SUCCEEDED( stream->Read( buffer.data(), static_cast<ULONG>( buffer.size() ), &bytesRead ) ) ) {
													const std::string encodedImage = Base64Encode( buffer.data(), bytesRead );
													if ( !encodedImage.empty() ) {
														tags.insert( Tags::value_type( Tag::Artwork, encodedImage ) );
													}
												}
											}
										}
									}
								}
							}
							PropVariantClear( &propVar );
						}
					}
				}
			}
		}
	}
	return tags.empty() ? kShellTagExitCodeFail : kShellTagExitCodeOK;
}

DWORD ShellTag::Set( const std::wstring& filename, const Tags& tags )
{
	const std::set<Tag> supportedTags = { Tag::Album, Tag::Artist, Tag::Comment, Tag::Genre, Tag::Title, Tag::Track, Tag::Year, Tag::Artwork, Tag::Composer, Tag::Conductor, Tag::Publisher };
	bool anySupportedTags = false;
	for ( const auto& tag : tags ) {
		if ( supportedTags.end() != supportedTags.find( tag.first ) ) {
			anySupportedTags = true;
			break;
		}
	}
	if ( !anySupportedTags )
		return kShellTagExitCodeOK;

	DWORD result = kShellTagExitCodeFail;
	Microsoft::WRL::ComPtr<IShellItem2> item;
	HRESULT hr = SHCreateItemFromParsingName( filename.c_str(), NULL /*bindContext*/, IID_PPV_ARGS( &item ) );
	if ( SUCCEEDED( hr ) ) {
		// Releasing the thumbnail stream occasionally raises an exception for some iunknown reason, hence the main incentive for creating a separate process.
		Microsoft::WRL::ComPtr<IStream> thumbnailStream;

		const GETPROPERTYSTOREFLAGS flags = GPS_READWRITE;
		Microsoft::WRL::ComPtr<IPropertyStore> propStore;
		hr = item->GetPropertyStore( flags, IID_PPV_ARGS( &propStore ) );
		if ( SUCCEEDED( hr ) ) {
			result = kShellTagExitCodeOK;
			bool propStoreModified = false;
			for ( auto tagIter = tags.begin(); ( kShellTagExitCodeOK == result ) && ( tagIter != tags.end() ); tagIter++ ) {
				const Tag tag = tagIter->first;
				const std::wstring value = UTF8ToWideString( tagIter->second );
				PROPERTYKEY propKey = {};
				PROPVARIANT propVar = {};
				bool updateTag = true;
				switch ( tag ) {
					case Tag::Album: {
						propKey = PKEY_Music_AlbumTitle;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Artist: {
						propKey = PKEY_Music_Artist;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Comment: {
						propKey = PKEY_Comment;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Genre: {
						propKey = PKEY_Music_Genre;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Title: {
						propKey = PKEY_Title;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Composer: {
						propKey = PKEY_Music_Composer;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Conductor: {
						propKey = PKEY_Music_Conductor;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Publisher: {
						propKey = PKEY_Media_Publisher;
						if ( !value.empty() ) {
							hr = InitPropVariantFromString( value.c_str(), &propVar );
						}
						break;
					}
					case Tag::Track: {
						propKey = PKEY_Music_TrackNumber;
						if ( !value.empty() ) {
							try {
								hr = InitPropVariantFromUInt32( std::stoul( value ), &propVar );
							} catch ( const std::logic_error& ) {
							}
						}
						break;
					}
					case Tag::Year: {
						propKey = PKEY_Media_Year;
						if ( !value.empty() ) {
							try {
								hr = InitPropVariantFromUInt32( std::stoul( value ), &propVar );
							} catch ( const std::logic_error& ) {
							}
						}
						break;
					}
					case Tag::Artwork: {
						propKey = PKEY_ThumbnailStream;
						const std::vector<BYTE> imageBytes = Base64Decode( tagIter->second );
						const ULONG imageSize = static_cast<UINT>( imageBytes.size() );
						if ( imageSize > 0 ) {
							hr = CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &thumbnailStream );
							if ( SUCCEEDED( hr ) ) {
								hr = thumbnailStream->Write( imageBytes.data(), imageSize, NULL /*bytesWritten*/ );
								if ( SUCCEEDED( hr ) ) {
									hr = thumbnailStream->Seek( { 0 }, STREAM_SEEK_SET, NULL /*newPosition*/ );
									if ( SUCCEEDED( hr ) ) {
										propVar.vt = VT_STREAM;
										propVar.pStream = thumbnailStream.Get();
									}
								}
							}
						}
						break;
					}
					default: {
						updateTag = false;
						break;
					}
				}
				if ( updateTag && SUCCEEDED( hr ) ) {
					hr = propStore->SetValue( propKey, propVar );
					result = SUCCEEDED( hr ) ? kShellTagExitCodeOK : kShellTagExitCodeFail;
					propStoreModified = ( kShellTagExitCodeOK == result );
				}
				PropVariantClear( &propVar );
			}
			if ( propStoreModified && SUCCEEDED( hr ) ) {
				hr = propStore->Commit();
				result = SUCCEEDED( hr ) ? kShellTagExitCodeOK : kShellTagExitCodeFail;
			}
		}
	}
	if ( HRESULT_FROM_WIN32( ERROR_SHARING_VIOLATION ) == hr ) {
		// Another process has the file open (possibly the Windows indexing service or something similar)
		result = kShellTagExitCodeSharingViolation;
	}
	return result;
}

std::wstring ShellTag::GetAudioSubType( const std::wstring& guid )
{
	if ( s_AudioFormatDescriptions.empty() ) {
		LPOLESTR str = nullptr;
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_AAC, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"AAC" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_AMR_NB, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"AMR" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_AMR_WB, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"AMR" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_AMR_WP, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"AMR" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_Dolby_AC3, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"Dolby Digital" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_Dolby_AC3_SPDIF, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"Dolby Digital" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_Dolby_DDPlus, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"Dolby Digital Plus" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_DTS, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"DTS" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_Float, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"IEEE Float" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_MP3, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"MP3" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_MPEG, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"MPEG-1" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_MSP1, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"WMA 9 Voice" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_PCM, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"PCM" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_WMASPDIF, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"WMA 9 Pro" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_WMAudio_Lossless, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"WMA Lossless" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_WMAudioV8, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"WMA" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_WMAudioV9, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"WMA 9 Pro" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_ALAC, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"Apple Lossless" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_FLAC, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"FLAC" ) );
			CoTaskMemFree( str );
		}
		if ( SUCCEEDED( StringFromCLSID( MFAudioFormat_Opus, &str ) ) ) {
			s_AudioFormatDescriptions.insert( AudioFormatMap::value_type( WideStringToLower( str ), L"Opus" ) );
			CoTaskMemFree( str );
		}
	}
	std::wstring description;
	const auto iter = s_AudioFormatDescriptions.find( WideStringToLower( guid ) );
	if ( iter != s_AudioFormatDescriptions.end() ) {
		description = iter->second;
	}
	return description;
}

std::wstring ShellTag::PropertyToString( const PROPVARIANT& propVariant )
{
	std::wstring value;
	if ( VT_VECTOR & propVariant.vt ) {
		LPWSTR* strings = nullptr;
		ULONG numStrings = 0;
		if ( SUCCEEDED( PropVariantToStringVectorAlloc( propVariant, &strings, &numStrings ) ) ) {
			for ( ULONG index = 0; index < numStrings; index++ ) {
				if ( !value.empty() ) {
					value += L", ";
				}
				value += strings[ index ];
				CoTaskMemFree( strings[ index ] );
			}
			CoTaskMemFree( strings );
		}
	} else if ( VT_LPWSTR == propVariant.vt ) {
		value = propVariant.pwszVal;
	} else if ( VT_UI4 == propVariant.vt ) {
		value = std::to_wstring( propVariant.ulVal );
	}
	return value;
}
