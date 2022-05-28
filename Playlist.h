#pragma once

#include "stdafx.h"

#include "Library.h"

#include <atomic>
#include <list>
#include <mutex>
#include <string>

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

		_Undefined
	};

	// Playlist item information.
	struct Item {
		long ID = 0;
		MediaInfo Info = {};
		std::list<std::wstring> Duplicates = {};
	};

	// List of playlist items.
	typedef std::list<Item> ItemList;

	// Playlist shared pointer type.
	typedef std::shared_ptr<Playlist> Ptr;

	// A set of playlists.
	typedef std::set<Playlist::Ptr> Set;	

	// Gets the playlist ID.
	const std::string& GetID() const;

	// Gets the playlist name.
	const std::wstring& GetName() const;

	// Sets the playlist name.
	void SetName( const std::wstring& name );

	// Returns the playlist items.
	ItemList GetItems();

	// Returns the pending files.
	std::list<std::wstring> GetPending();

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

	// Adds 'mediaInfo' to the playlist, returning the added item.
	// 'position' - out, 0-based index of the added item position.
	// 'addedAsDuplicate' - out, whether the item was added as a duplicate of an existing item (which is returned).
	Item AddItem( const MediaInfo& mediaInfo, int& position, bool& addedAsDuplicate );

	// Adds 'filename' to the list of pending files to be added to the playlist.
	// 'startPendingThread' - whether to start the background thread to process pending files.
	void AddPending( const std::wstring& filename, const bool startPendingThread = true );

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

	// Returns whether the playlist contains 'filename'.
	bool ContainsFilename( const std::wstring& filename );

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

	// Playlist ID.
	const std::string m_ID;

	// Playlist name.
	std::wstring m_Name;

	// The playlist.
	ItemList m_Playlist;

	// Pending files to be added to the playlist.
	std::list<std::wstring> m_Pending;

	// Playlist mutex.
	std::mutex m_MutexPlaylist;

	// Pending files mutex.
	std::mutex m_MutexPending;

	// The thread for adding pending files to the playlist.
	HANDLE m_PendingThread;

	// Event handle for terminating the pending file thread.
	HANDLE m_PendingStopEvent;

	// Event handle for waking the pending file thread.
	HANDLE m_PendingWakeEvent;

	// Indicates whether the pending files thread should be restarted.
	std::atomic<bool> m_RestartPendingThread;

	// Media library.
	Library& m_Library;

	// Current sort column.
	Column m_SortColumn;

	// Whether the list is sorted in ascending order.
	bool m_SortAscending;

	// Playlist type.
	Type m_Type;

	// Whether duplicate items should be merged into a single playlist entry.
	bool m_MergeDuplicates;

	// A shuffled playlist.
	ItemList m_ShuffledPlaylist;

	// Shuffled playlist mutex.
	std::mutex m_MutexShuffled;
};

// A list of playlists.
typedef std::list<Playlist::Ptr> Playlists;
