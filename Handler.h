#pragma once

#include "Decoder.h"

#include <map>
#include <memory>
#include <set>
#include <string>

// Audio format handler interface.
class Handler
{
public:
	Handler()
	{
	}

	virtual ~Handler()
	{
	}

	// Handler shared pointer type.
	typedef std::shared_ptr<Handler> Ptr;

	// Tag types.
	enum class Tag {
		Artist,
		Title,
		Album,
		Genre,
		Year,
		Comment,
		Track,
		Version,
		GainTrack,
		GainAlbum,
		PeakTrack,
		PeakAlbum,
		Artwork
	};

	// Handler tags.
	typedef std::map<Tag,std::wstring> Tags;

	// Returns a description of the handler.
	virtual std::wstring GetDescription() const = 0;

	// Returns the supported file extensions.
	virtual std::set<std::wstring> GetSupportedFileExtensions() const = 0;

	// Reads 'tags' from 'filename', returning true if the tags were read.
	virtual bool GetTags( const std::wstring& filename, Tags& tags ) const = 0;

	// Writes 'tags' to 'filename', returning true if the tags were written.
	virtual bool SetTags( const std::wstring& filename, const Tags& tags ) const = 0;

	// Returns a decoder for 'filename', or nullptr if a decoder cannot be created.
	virtual Decoder::Ptr OpenDecoder( const std::wstring& filename ) const = 0;
};
