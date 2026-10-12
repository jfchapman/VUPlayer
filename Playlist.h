#pragma once

#include "stdafx.h"

#include "Library.h"

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <optional>
#include <set>
#include <filesystem>

class Playlist
{
public:
	// Playlist type.
	enum class Type {
		User = 1,
		All,
		Artist,
		Album,
		Genre,
		Year,
		Favourites,
		CDDA,
		Folder,
		Streams,
		Publisher,
		Composer,
		Conductor,

		_Undefined
	};

	// 'library' - media library
	// 'type' - playlist type
	Playlist( Library& library, const Type& type );

	// 'library' - media library
	// 'id' - playlist ID
	// 'type' - playlist type
	Playlist( Library& library, const std::string& id, const Type& type );

	// 'library' - media library
	// 'type' - playlist type
	// 'mergeDuplicates' - whether to merge duplicate items into a single playlist entry
	Playlist( Library& library, const Type& type, const bool mergeDuplicates );

	virtual ~Playlist();

	// Returns the supported playlist file extensions.
	static std::set<std::wstring> GetSupportedPlaylistExtensions();

	// Returns whether 'filename' is a supported playlist type.
	static bool IsSupportedPlaylist( const std::wstring& filename );

	// Extracts CUE files, and all files referenced by CUE files, from a set of 'filenames'.
	// Returns just the CUE files, or nullopt if no CUE files were found.
	static std::optional<std::set<std::filesystem::path>> ExtractCueFiles( std::set<std::filesystem::path>& filenames );

	// Column type.
	enum class Column {
		Filepath = 1,
		Filetime,
		Filesize,
		Duration,
		SampleRate,
		BitsPerSample,
		Channels,
		Artist,
		Title,
		Album,
		Genre,
		Year,
		Track,
		Type,
		Version,
		GainTrack,
		GainAlbum,
		Bitrate,
		Filename,
		Composer,
		Conductor,
		Publisher,
		PlayCount,

		_Undefined
	};

	// Playlist item information.
	struct Item {
		long ID = 0;
		MediaInfo Info = {};
		std::set<std::wstring> Duplicates = {};
	};

	// Vector of playlist items.
	using Items = std::vector<Item>;

	// Playlist shared pointer type.
	using Ptr = std::shared_ptr<Playlist>;

	// A set of playlists.
	using Set = std::set<Playlist::Ptr>;

	// Gets the playlist ID.
	const std::string& GetID() const;

	// Gets the playlist name.
	const std::wstring& GetName() const;

	// Sets the playlist name.
	void SetName( const std::wstring& name );

	// Note that, when a filter is set (and enabled), methods which deal with item positions or playback order (e.g. GetCount, GetItem by position, GetNextItem)
	// only consider the items which match the filter. Methods which deal with the playlist as a whole (e.g. GetItems, SavePlaylist, ContainsFile)
	// consider all items, regardless of the filter.

	// Returns all the playlist items (regardless of any filter).
	Items GetItems();

	// Returns the playlist items which match the filter (or all the playlist items, if no filter is set).
	Items GetFilteredItems();

	// Sets the playlist 'filter', which is split into whitespace separated terms that each need to be (case & accent insensitively) contained
	// in one of the item's text fields for the item to be shown. An empty filter shows all items.
	// The filter is only applied while it is enabled (see SetFilterEnabled).
	// Returns whether the set of filtered items has changed (in which case the caller is responsible for refreshing any displayed items).
	bool SetFilter( const std::wstring& filter );

	// Sets whether the filter is 'enabled', so that the filter can be suspended (e.g. while the search bar is hidden) without losing the filter text.
	// Filters are enabled by default.
	// Returns whether the set of filtered items has changed (in which case the caller is responsible for refreshing any displayed items).
	bool SetFilterEnabled( const bool enabled );

	// Returns whether the filter is enabled.
	bool IsFilterEnabled();

	// Returns the playlist filter text (regardless of whether the filter is enabled).
	std::wstring GetFilter();

	// Returns whether a filter is being applied (i.e. a non-empty filter is set, and enabled).
	bool IsFiltered();

	// Returns whether an 'item' matches the playlist filter.
	bool MatchesFilter( const Item& item );

	// Returns the pending files.
	std::list<MediaInfo> GetPending();

	// Returns the number of pending files.
	int GetPendingCount();

	// Gets a playlist item (regardless of any filter).
	// 'item' - in/out, playlist item containing the ID of the item to get.
	// Returns true if the item was returned.
	bool GetItem( Item& item );

	// Gets a playlist item which matches the filter.
	// 'item' - in/out, playlist item containing the ID of the item to get.
	// 'position' - out, item position in the filtered playlist.
	// Returns true if the item/position was returned.
	bool GetItem( Item& item, int& position );

	// Gets a playlist item.
	// 'position' - item position in the filtered playlist.
	Item GetItem( const int position );

	// Gets a playlist item ID.
	// 'position' - item position in the filtered playlist.
	long GetItemID( const int position );

	// Gets the position of an item in the filtered playlist.
	// 'itemID' - playlist item ID.
	// Returns the item position, or -1 if the item ID was not found (or does not match the filter).
	int GetItemPosition( const long itemID );

	// Gets the next playlist item which matches the filter.
	// 'currentItem' - the current item (which does not itself need to match the filter).
	// 'nextItem' - out, the next item.
	// 'wrap' - whether to wrap round to the first playlist item.
	// Returns true if a 'nextItem' was returned.
	bool GetNextItem( const Item& currentItem, Item& nextItem, const bool wrap );

	// Gets the previous playlist item which matches the filter.
	// 'currentItem' - the current item (which does not itself need to match the filter).
	// 'previousItem' - out, the previous item.
	// 'wrap' - whether to wrap round to the last playlist item.
	// Returns true if a 'previousItem' was returned.
	bool GetPreviousItem( const Item& currentItem, Item& previousItem, const bool wrap );

	// Gets the first playlist item which matches the filter.
	Item GetFirstItem();

	// Gets a random playlist item which matches the filter.
	// 'currentItem' - the current item.
	Item GetRandomItem( const Item& currentItem );

	// Adds 'mediaInfo' to the playlist, returning the added item.
	Item AddItem( const MediaInfo& mediaInfo );

	// Adds 'mediaInfo' to the list of pending media to be added to the playlist.
	// 'startPendingThread' - whether to start the background thread to process pending files.
	void AddPending( const MediaInfo& mediaInfo, const bool startPendingThread = true );

	// Adds a playlist 'filename' to this playlist.
	// 'startPendingThread' - whether to start the background thread to process pending files.
	// Returns whether any pending files were added to this playlist.
	bool AddPlaylist( const std::wstring& filename, const bool startPendingThread = true );

	// Starts the thread for adding pending files to the playlist.
	void StartPendingThread();

	// Stops the thread for adding pending files to the playlist.
	void StopPendingThread();

	// Returns the media library.
	Library& GetLibrary();

	// Saves the playlist to 'filename'.
	void SavePlaylist( const std::wstring& filename );

	// Removes a playlist item.
	// 'item' - playlist item containing the ID of the item to remove.
	// Returns whether the item was removed.
	bool RemoveItem( const Item& item );

	// Removes 'mediaInfo' from the playlist.
	// Returns whether the media information was removed.
	bool RemoveItem( const MediaInfo& mediaInfo );

	// Removes all filenames in the 'mediaList' from the playlist.
	// Returns whether any files were removed.
	bool RemoveFiles( const MediaInfo::List& mediaList );

	// Returns the number of items in the filtered playlist.
	long GetCount();

	// Returns the total filtered playlist duration, in seconds.
	float GetDuration();

	// Returns the total filtered playlist file size, in bytes.
	long long GetFilesize();

	// Gets the current playlist sort information.
	// 'column' - out, sort type (or 'undefined' if not sorted).
	// 'ascending' - out, true if sorted in ascending order, false if in descending order (only valid if sorted).
	void GetSort( Column& column, bool& ascending ) const;

	// Sorts the playlist by 'column', in ascending order if not already sorted by 'column', descending order otherwise.
	void Sort( const Column column );

	// Updates the playlist media information.
	// 'mediaInfo' - media information
	// Returns true if any playlist items matched the 'mediaInfo' filename, and were updated.
	bool OnUpdatedMedia( const MediaInfo& mediaInfo );

	// Moves 'items' to a 'position' in the filtered playlist.
	// Returns whether any items have effectively moved position.
	bool MoveItems( const int position, const std::list<long>& items );

	// Returns the playlist type.
	Type GetType() const;

	// Sets whether duplicate playlist items should be merged.
	void SetMergeDuplicates( const bool merge );

	// Returns whether the playlist contains any items that can be converted (or extracted).
	bool CanConvertAnyItems();

	// Checks whether the filtered playlist contains an item.
	// 'item' - playlist item containing the ID of the item to check.
	// Returns true if the item is in the playlist, and matches the filter.
	bool ContainsItem( const Item& item );

	// Returns whether the playlist contains 'filename' (with optional start & end cues).
	bool ContainsFile( const std::wstring& filename, const std::optional<long>& cueStart = std::nullopt, const std::optional<long>& cueEnd = std::nullopt );

	// Performs a case insensitive search for a title in the filtered playlist.
	// 'startIndex' - the index at which to start the search.
	// 'searchTitle' - the title to search for.
	// 'partial' - true to match any title which begins with the search string, false to perform an exact match. 
	// 'wrap' - wraps round to the start of the playlist if a match is not found.
	// Returns the matching item position, or -1 if there was no match.
	int FindItem( const int startIndex, const std::wstring& searchTitle, const bool partial, const bool wrap );

	// Returns whether MusicBrainz queries can be performed using this playlist.
	bool AllowMusicBrainzQueries();

	// Updates a playlist item using 'mediaInfo', or adds the item if a matching file was not found.
	// Returns whether the item was found and updated.
	bool UpdateOrAddItem( const MediaInfo& mediaInfo );

private:
	// Pending file thread proc.
	static DWORD WINAPI PendingThreadProc( LPVOID lpParam );

	// Returns true if 'item1' is less than 'item2' when comparing by 'column' type.
	static bool LessThan( const Item& item1, const Item& item2, const Column column );

	// Returns true if 'item1' is greater than 'item2' when comparing by 'column' type.
	static bool GreaterThan( const Item& item1, const Item& item2, const Column column );

	// Next available playlist item ID.
	static long s_NextItemID;

	// Thread handler for processing the list of pending files.
	void OnPendingThreadHandler();

	// Merges any duplicate items.
	void MergeDuplicates();

	// Splits out any duplicates into separate items.
	void SplitDuplicates();

	// Closes the thread and event handles.
	void CloseHandles();

	// Adds a VPL playlist 'filename' to this playlist.
	// Returns whether any pending files were added to this playlist.
	bool AddVPL( const std::wstring& filename );

	// Adds an M3U playlist 'filename' to this playlist.
	// Returns whether any pending files were added to this playlist.
	bool AddM3U( const std::wstring& filename );

	// Adds a PLS playlist 'filename' to this playlist.
	// Returns whether any pending files were added to this playlist.
	bool AddPLS( const std::wstring& filename );

	// Adds a CUE playlist 'filename' to this playlist.
	// Returns whether any files were added to this playlist.
	bool AddCUE( const std::wstring& filename );

	// Returns the position in the full (unfiltered) playlist of the 'itemID', or -1 if the item ID was not found.
	// (Internal method with no lock).
	int GetPositionNoLock( const long itemID );

	// Returns the number of items in the filtered playlist.
	// (Internal method with no lock).
	size_t GetFilteredCountNoLock() const;

	// Converts a filtered playlist 'position' to a full playlist position, returning -1 if the position is out of range.
	// (Internal method with no lock).
	int FilteredToFullPositionNoLock( const int position ) const;

	// Converts a full playlist 'position' to a filtered playlist position, returning -1 if the item at that position does not match the filter.
	// (Internal method with no lock).
	int FullToFilteredPositionNoLock( const int position ) const;

	// Returns whether an 'item' matches the filter 'terms'.
	static bool MatchesFilter( const Item& item, const std::vector<std::wstring>& terms );

	// Updates the 'filter' text and/or whether the filter is 'enabled' (leaving unspecified values unchanged), and reapplies the filter.
	// Returns whether the set of filtered items has changed.
	bool UpdateFilter( const std::optional<std::wstring>& filter, const std::optional<bool> enabled );

	// Rebuilds the item ID to position map, and the filtered positions, from scratch.
	// (Internal method with no lock).
	void UpdatePositionsNoLock();

	// Updates the item ID to position map, and the filtered positions, after an item has been inserted at the full playlist 'position'.
	// Returns the filtered position of the inserted item, or -1 if the item does not match the filter.
	// (Internal method with no lock).
	int OnItemInsertedNoLock( const size_t position );

	// Updates the item ID to position map, and the filtered positions, after the 'itemID' has been erased from the full playlist 'position'.
	// (Internal method with no lock).
	void OnItemErasedNoLock( const size_t position, const long itemID );

	// Adds 'mediaInfo' to the playlist, returning the added item.
	// 'position' - out, 0-based index of the added item position in the filtered playlist (or -1 if the item does not match the filter).
	// 'addedAsDuplicate' - out, whether the item was added as a duplicate of an existing item (which is returned).
	Item AddItem( const MediaInfo& mediaInfo, int& position, bool& addedAsDuplicate );

	// Playlist ID.
	const std::string m_ID;

	// Playlist name.
	std::wstring m_Name;

	// The playlist.
	Items m_Playlist;

	// Pending media to be added to the playlist.
	std::list<MediaInfo> m_Pending;

	// Playlist mutex.
	std::mutex m_MutexPlaylist;

	// Pending media mutex.
	std::mutex m_MutexPending;

	// The thread for adding pending media to the playlist.
	HANDLE m_PendingThread;

	// Event handle for terminating the pending media thread.
	HANDLE m_PendingStopEvent;

	// Event handle for waking the pending media thread.
	HANDLE m_PendingWakeEvent;

	// Indicates whether the pending media thread should be restarted.
	std::atomic<bool> m_RestartPendingThread;

	// Media library.
	Library& m_Library;

	// Current sort column.
	Column m_SortColumn;

	// Whether the list is sorted in ascending order.
	bool m_SortAscending;

	// Playlist type.
	const Type m_Type;

	// Whether duplicate items should be merged into a single playlist entry.
	bool m_MergeDuplicates;

	// A shuffled playlist.
	Items m_ShuffledPlaylist;

	// Shuffled playlist mutex.
	std::mutex m_MutexShuffled;

	// Maps a playlist item ID to its position in the full (unfiltered) playlist.
	std::map<long, size_t> m_ItemIDPositions;

	// Playlist filter.
	std::wstring m_Filter;

	// Whether the filter is enabled.
	bool m_FilterEnabled;

	// The filter terms being applied, all of which must match for an item to be shown (empty if there is no filter, or the filter is disabled).
	std::vector<std::wstring> m_FilterTerms;

	// The full playlist positions of the items which match the filter, in ascending order (only used when there are filter terms).
	std::vector<size_t> m_FilteredPositions;
};

// A list of playlists.
typedef std::list<Playlist::Ptr> Playlists;
