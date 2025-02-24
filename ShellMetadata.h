#pragma once
#include <stdint.h>

// Exit codes returned by the shelltag console process
constexpr uint32_t kShellTagExitCodeOK = 99;                // OK
constexpr uint32_t kShellTagExitCodeFail = 98;              // Failed to read/write metadata
constexpr uint32_t kShellTagExitCodeSharingViolation = 97;  // Sharing violation when writing metadata (file locked by another process)

#ifndef _CONSOLE
#include "Tag.h"

#include <string>
#include <optional>

// Returns tags if any were read from the file, nullopt otherwise.
std::optional<Tags> GetShellMetadata( const std::wstring& filename );

// Returns true if any tags were written to the file, or if none of the tags are supported.
bool SetShellMetadata( const std::wstring& filename, const Tags& tags );
#endif
