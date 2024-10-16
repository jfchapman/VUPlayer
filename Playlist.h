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

	// Returns the playlist items.
	Items GetItems();

	// Returns the pending files.
	std::list<MediaInfo> GetPending();

	// Returns the number of pending files.
	int GetPendingCount();

	// Gets a playlist item.
	// 'item' - in/out, playlist item containing the ID of the item to get.
	// Returns true if the item was returned. 
	bool GetItem( Item& item );

	// Gets a playlist item.
	// 'item' - in/out, playlist item containing the ID of the item to get.
	// 'position' - out, item position.
	// Returns true if the item/position was returned. 
	bool GetItem( Item& item, int& position );

  // Gets a playlist item.
  // 'position' - item position.
  Item GetItem( const int position );

  // Gets a playlist item ID.
  // 'position' - item position.
  long GetItemID( const int position );

  // Gets the position of an item in the playlist.
  // 'itemID' - playlist item ID.
  // Returns the item position, or -1 if the item ID was not found.
  int GetItemPosition( const long itemID );

	// Gets the next playlist item.
	// 'currentItem' - the current item.
	// 'nextItem' - out, the next item.
	// 'wrap' - whether to wrap round to the first playlist item.
	// Returns true if a 'nextItem' was returned.
	bool GetNextItem( const Item& currentItem, Item& nextItem, const bool wrap = true );

	// Gets the previous playlist item.
	// 'currentItem' - the current item.
	// 'previousItem' - out, the previous item.
	// 'wrap' - whether to wrap round to the last playlist item.
	// Returns true if a 'previousItem' was returned.
	bool GetPreviousItem( const Item& currentItem, Item& previousItem, const bool wrap = true );

	// Gets the first playlist item.
	Item GetFirstItem();

	// Gets a random playlist item.
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

	// Returns the number of items in the playlist.
	long GetCount();

	// Returns the total playlist duration, in seconds.
	float GetDuration();

	// Returns the total playlist file size, in bytes.
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

	// Moves 'items' to a 'position' in the playlist.
	// Returns whether any items have effectively moved position.
	bool MoveItems( const int position, const std::list<long>& items );

	// Returns the playlist type.
	Type GetType() const;

	// Sets whether duplicate playlist items should be merged.
	void SetMergeDuplicates( const bool merge );

	// Updates the 'item' in the playlist.
	void UpdateItem( const Item& item );

	// Returns whether the playlist contains any items that can be converted (or extracted).
	bool CanConvertAnyItems();

	// Checks whether the playlist contains an item.
	// 'item' - playlist item containing the ID of the item to check.
	// Returns true if the item is in the playlist. 
	bool ContainsItem( const Item& item );

	// Returns whether the playlist contains 'filename' (with optional start & end cues).
	bool ContainsFile( const std::wstring& filename, const std::optional<long>& cueStart = std::nullopt, const std::optional<long>& cueEnd = std::nullopt );

  // Performs a case insensitive search for a title in the playlist.
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

  // Returns the position in the playlist of the 'itemID', or -1 if the item ID was not found.
  // (Internal method with no lock).
  int GetPositionNoLock( const long itemID );

	// Adds 'mediaInfo' to the playlist, returning the added item.
	// 'position' - out, 0-based index of the added item position.
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

  // Maps a playlist item ID to its position in the playlist.
  std::map<long, size_t> m_ItemIDPositions;
};

// A list of playlists.
typedef std::list<Playlist::Ptr> Playlists;
