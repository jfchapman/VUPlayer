#pragma once

#include "stdafx.h"

#include "CDDAManager.h"
#include "FolderMonitor.h"
#include "Library.h"
#include "Output.h"
#include "Settings.h"

class WndTree
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'library' - media library.
	// 'settings' - application settings.
	// 'cddaManager' - CD audio manager.
	// 'output' - output object.
	WndTree( HINSTANCE instance, HWND parent, Library& library, Settings& settings, CDDAManager& cddaManager, Output& output );

	virtual ~WndTree();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Gets the tree control handle.
	HWND GetWindowHandle();

	// Returns the playlist associated with a tree 'node', or a null playlist if it's not a playlist node.
	Playlist::Ptr GetPlaylist( const HTREEITEM node );

	// Returns the currently selected playlist, or a null playlist if one isn't selected.
	Playlist::Ptr GetSelectedPlaylist();

	// Called when a 'command' is received.
	void OnCommand( const UINT command );

	// Displays the context menu at the specified 'position', in screen coordinates.
	void OnContextMenu( const POINT& position );

	// Called when item label editing begins.
	// Returns TRUE if label editing is to be prevented.
	LPARAM OnBeginLabelEdit( WPARAM wParam, LPARAM lParam );

	// Called when item label editing ends.
	// Returns TRUE to accept the new item label, FALSE to revert to the original label.
	BOOL OnEndLabelEdit( WPARAM wParam, LPARAM lParam );

	// Called when the window is destroyed.
	void OnDestroy();

	// Gets the playlists.
	Playlists GetPlaylists();

	// Adds a 'playlist'.
	void AddPlaylist( const Playlist::Ptr playlist );

	// Creates and returns a new playlist.
	// 'editTitle' - whether to begin label edit mode on creation.
	Playlist::Ptr NewPlaylist( const bool editTitle = false );

	// Deletes the currently selected playlist.
	void DeleteSelectedPlaylist();

	// Imports a playlist.
	// 'filename' - the playlist to import, or an empty string to launch a file selection dialog.
	void ImportPlaylist( const std::wstring& filename = std::wstring() );

	// Launches a file selector dialog to export the currently selected playlist.
	void ExportSelectedPlaylist();

	// Selects a 'playlist'.
	// 'showNode' - shows the playlist node, if it is not already shown.
	void SelectPlaylist( const Playlist::Ptr playlist, const bool showNode = true );

	// Selects, and shows if necessary, the 'All Tracks' playlist.
	void SelectAllTracks();

	// Displays the font selection dialog for changing the tree control font.
	void OnSelectFont();

	// Displays the colour selection dialog for changing the tree control colours.
	// 'commandID' - command ID indicating which colour to change.
	void OnSelectColour( const UINT commandID );

	// Initialises the tree control playlist structure.
	void Initialise();

	// Called when media information has been updated.
	// 'previousMediaInfo' - previous media information.
	// 'updatedMediaInfo' - updated media information.
	// Returns the playlists that have been updated.
	Playlist::Set OnUpdatedMedia( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo );

	// Called when the 'mediaList' has been removed from the media library.
	void OnRemovedMedia( const MediaInfo::List& mediaList );

	// Called when the available CD audio discs have been refreshed.
	void OnCDDARefreshed();

	// Toggles 'Favourites' on the tree control.
	void OnFavourites();

	// Toggles 'Streams' on the tree control.
	void OnStreams();

	// Toggles 'All Tracks' on the tree control.
	void OnAllTracks();

	// Toggles artists on the tree control.
	void OnArtists();

	// Toggles albums on the tree control.
	void OnAlbums();

	// Toggles genres on the tree control.
	void OnGenres();

	// Toggles years on the tree control.
	void OnYears();

	// Returns whether the tree control node represented by 'commandID' is shown.
	bool IsShown( const UINT commandID ) const;

	// Saves the startup 'playlist' to application settings.
	void SaveStartupPlaylist( const Playlist::Ptr playlist );

	// Returns the highlight colour (which is applied when not in high contrast mode).
	COLORREF GetHighlightColour() const;

	// Returns the icon colour (which is applied when not in high contrast mode).
	COLORREF GetIconColour() const;

	// Returns the Favourites playlist.
	Playlist::Ptr GetPlaylistFavourites() const;

	// Returns the Streams playlist.
	Playlist::Ptr GetPlaylistStreams() const;

	// Returns the All Tracks playlist.
	Playlist::Ptr GetPlaylistAll() const;

	// Returns whether it is possible to delete the currently selected item.
	bool IsPlaylistDeleteEnabled();

	// Returns whether it is possible to export the currently selected item.
	bool IsPlaylistExportEnabled();

	// Returns whether it is possible to rename the currently selected item.
	bool IsPlaylistRenameEnabled();

	// Starts editing the name of the currently selected playlist.
	void RenameSelectedPlaylist();

	// Called when a tree 'item' is about to be expanded.
	void OnItemExpanding( const HTREEITEM item );

	// Called when a logical 'drive' has arrived.
	void OnDriveArrived( const wchar_t drive );
	
	// Called when a logical 'drive' has been removed.
	void OnDriveRemoved( const wchar_t drive );

	// Called when a file system device 'handle' is being removed.
	void OnDeviceHandleRemoved( const HANDLE handle );

	// Sets and selects the playlist used for files passed via the shell (e.g on startup).
	// 'mediaList' - media list with which to populate the scratch list.
	// Returns the scratch list.
	Playlist::Ptr SetScratchList( const MediaInfo::List& mediaList );

	// Sets whether duplicate tracks are merged into a single entry (for Artist/Album/Genre/Year playlists).
	void SetMergeDuplicates( const bool mergeDuplicates );

	// Saves the tree control settings to application settings.
	void SaveSettings();

	// Selects the Audio CD 'drive'.
	// Returns the playlist if the Audio CD was selected, or nullptr if not.
	Playlist::Ptr SelectAudioCD( const wchar_t drive );

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	void OnSysColorChange( const bool isHighContrast );

	// Called when the current output 'playlist' changes.
	void OnOutputPlaylistChange( const Playlist::Ptr playlist );

	// Updates the icon the for current output playlist node.
	void UpdateOutputIcon();

	// Cuts or copies text to the clipboard (only when a tree item label is being edited).
	// 'cut' - true to cut the files, false to copy the files.
	void OnCutCopy( const bool cut );

	// Pastes text from the clipboard (only when a tree item label is being edited).
	void OnPaste();

	// Selects all text (only when a tree item label is being edited).
	void OnSelectAll();

private:
	// Window procedure
	static LRESULT CALLBACK TreeProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Edit control window procedure
	static LRESULT CALLBACK EditControlProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Thread for updating media information in the scratch list.
	static DWORD WINAPI ScratchListUpdateProc( LPVOID lpParam );

	// Thread for handling file modification events from the folder monitor.
	static DWORD WINAPI FileModifiedProc( LPVOID lpParam );

	// Information for the scratch list update thread.
	struct ScratchListUpdateInfo {
		// 'library' - media library.
		// 'stopEvent' - indicates whether the thread should stop.
		// 'mediaList' - the media to update.
		ScratchListUpdateInfo( Library& library, const HANDLE stopEvent, const MediaInfo::List& mediaList ) :
			MediaLibrary( library ),
			StopEvent( stopEvent ),
			MediaList( mediaList ) {}

		Library& MediaLibrary;
		HANDLE StopEvent;
		MediaInfo::List MediaList;
	};

	// Maps a tree item to a playlist.
	using PlaylistMap = std::map<HTREEITEM,Playlist::Ptr>;

	// Maps a playlist type to an icon index.
	using IconMap = std::map<Playlist::Type,int>;

	// Maps a playlist type to an item order value.
	using OrderMap = std::map<Playlist::Type,int>;

	// Root folder type.
	enum class RootFolderType { UserFolder, Drive };

	// Root folder information.
	struct RootFolderInfo {
		std::wstring Name;				// Folder name.
		std::wstring Path;				// Folder path.
		int IconIndex;						// Folder icon index.
		RootFolderType Type;			// Folder type.

		bool operator<( const RootFolderInfo& other ) const
		{
			return std::tie( Name, Path, IconIndex, Type ) < std::tie( other.Name, other.Path, other.IconIndex, other.Type );
		}

		bool operator==( const RootFolderInfo& other ) const
		{
			return std::tie( Name, Path, IconIndex, Type ) == std::tie( other.Name, other.Path, other.IconIndex, other.Type );
		}
	};

	// A folder information list.
	using RootFolderInfoList = std::list<RootFolderInfo>;

	// Maps a tree item to folder information.
	using RootFolderInfoMap = std::map<HTREEITEM,RootFolderInfo>;

	// Maps a string to a set of tree item nodes.
	using StringToNodesMap = std::map<std::wstring,std::set<HTREEITEM>>;

	// Maps a menu command ID to a playlist.
	using PlaylistMenuMap = std::map<UINT,Playlist::Ptr>;

	// Searches the 'playlistMap' for the 'playlist'.
	// Returns the corresponding tree item, or nullptr if the playlist was not found.
	static HTREEITEM FindPlaylist( const PlaylistMap& playlistMap, const Playlist::Ptr& playlist );

	// Creates a tree item parameter value from 'playlistType', 'originalIconIndex' & 'itemOrder'.
	static LPARAM CreateItemData( const Playlist::Type playlistType, const int originalIconIndex, const int itemOrder = 0 );

	// Loads playlists
	void LoadPlaylists();

	// Gets the current tree control font.
	LOGFONT GetFont();

	// Sets the current tree control font.
	void SetFont( const LOGFONT& logFont );

	// Initialises the tree control from application settings.
	void ApplySettings();

	// Adds 'Favourites' to the tree control.
	void AddFavourites();

	// Adds 'Streams' to the tree control.
	void AddStreams();

	// Adds 'All Tracks' to the tree control.
	void AddAllTracks();

	// Adds artists to the tree control.
	void AddArtists();

	// Adds albums to the tree control.
	void AddAlbums();

	// Adds genres to the tree control.
	void AddGenres();

	// Adds years to the tree control.
	void AddYears();

	// Adds CD audio discs to the tree control.
	void AddCDDA();

	// Removes 'Favourites' from the tree control.
	void RemoveFavourites();

	// Removes 'Streams' from the tree control.
	void RemoveStreams();

	// Removes 'All Tracks' from the tree control.
	void RemoveAllTracks();

	// Removes artists from the tree control.
	void RemoveArtists();

	// Removes albums from the tree control.
	void RemoveAlbums();

	// Removes genres from the tree control.
	void RemoveGenres();

	// Removes years from the tree control.
	void RemoveYears();

	// Removes CD audio discs from the tree control.
	void RemoveCDDA();

	// Adds an item to the tree control.
	// 'parentItem' - parent tree item.
	// 'label' - item label.
	// 'type' - item type.
	// 'redraw' - whether to redraw the control.
	// Returns the added item.
	HTREEITEM AddItem( const HTREEITEM parentItem, const std::wstring& label, const Playlist::Type type, const bool redraw = true );

	// Removes an item from the tree control.
	// 'item' - item to remove.
	void RemoveItem( const HTREEITEM item );

	// Gets the tree 'item' type.
	Playlist::Type GetItemType( const HTREEITEM item ) const;

	// Returns the item order for the root 'item'.
	int GetItemOrder( const HTREEITEM item ) const;

	// Returns the original icon index for the 'item'.
	int GetOriginalIconIndex( const HTREEITEM item ) const;

	// Gets the tree 'item' label.
	std::wstring GetItemLabel( const HTREEITEM item ) const;

	// Sets the tree 'item' 'label'.
	void SetItemLabel( const HTREEITEM item, const std::wstring& label ) const;

	// Returns all the playlist names.
	std::set<std::wstring> GetPlaylistNames() const;

	// Processes any playlists with pending files.
	void ProcessPendingPlaylists();

	// Updates artists when media information has been updated.
	// 'previousMediaInfo' - previous media information.
	// 'updatedMediaInfo' - updated media information.
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdateArtists( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Updates albums when media information has been updated.
	// 'parentItem' - the parent tree item containing the albums.
	// 'previousMediaInfo' - previous media information
	// 'updatedMediaInfo' - updated media information
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdateAlbums( const HTREEITEM parentItem, const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Updates genres when media information has been updated.
	// 'previousMediaInfo' - previous media information
	// 'updatedMediaInfo' - updated media information
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdateGenres( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Updates years when media information has been updated.
	// 'previousMediaInfo' - previous media information
	// 'updatedMediaInfo' - updated media information
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdateYears( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Updates playlists when media information has been updated.
	// 'updatedMediaInfo' - updated media information.
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdatePlaylists( const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Updates CD audio playlists when CD audio information has been updated.
	// 'updatedMediaInfo' - updated media information.
	// 'updatedPlaylists' - in/out, the playlists that have been updated.
	void UpdateCDDA( const MediaInfo& updatedMediaInfo, Playlist::Set& updatedPlaylists );

	// Returns the initial selected tree item from application settings.
	HTREEITEM GetStartupItem();

	// Populates the tree.
	void Populate();

	// Creates the image list.
	void CreateImageList();

	// Gets the icon index corresponding to a playlist 'type'.
	int GetIconIndex( const Playlist::Type type ) const;

	// Loads the 'All Tracks' playlist.
	void LoadAllTracks();

	// Loads the 'Favourites' playlist.
	void LoadFavourites();

	// Loads the 'Streams' playlist.
	void LoadStreams();

	// Returns the tree insertion position for a root item corresponding to a playlist 'type'.
	HTREEITEM GetInsertAfter( const Playlist::Type type ) const;

	// Refreshes the computer tree control item.
	void RefreshComputerNode();

	// Returns the user folders.
	RootFolderInfoList GetUserFolders() const;

	// Returns the logical drives.
	RootFolderInfoList GetComputerDrives() const;

	// Returns the sub folders of the 'parent' folder.
	std::set<std::wstring> GetSubFolders( const std::wstring& parent ) const;

	// Gets the folder 'path' of the tree 'item'.
	void GetFolderPath( const HTREEITEM item, std::wstring& path ) const;

	// Adds sub folders to the tree 'item'.
	void AddSubFolders( const HTREEITEM item );

	// Adds tracks to the folder 'playlist' represented by the tree 'item'.
	void AddFolderTracks( const HTREEITEM item, Playlist::Ptr playlist ) const;

	// Folder monitor callback.
	// 'monitorEvent' - event type.
	// 'oldFilename' - the old file or folder name.
	// 'newFilename' - the new file or folder name.
	void OnFolderMonitorCallback( const FolderMonitor::Event monitorEvent, const std::wstring& oldFilename, const std::wstring& newFilename );

	// File modification thread handler.
	void OnFileModifiedHandler();

	// Returns the 'children' (including all sub-children) of the tree 'item'.
	void GetAllChildren( const HTREEITEM item, std::set<HTREEITEM>& children ) const;

	// Called when a 'folder' needs to be added to the 'parent' tree item. 
	// Returns the added folder tree item.
	HTREEITEM OnFolderAdd( const HTREEITEM parent, const std::wstring& folder );

	// Called when a folder 'item' needs to be deleted from the tree.
	void OnFolderDelete( const HTREEITEM item );

	// Called when an 'oldFolderPath' has been renamed to 'newFolderPath'.
	void OnFolderRename( const std::wstring& oldFolderPath, const std::wstring& newFolderPath );

	// Adds the tree 'item' to the folder nodes map.
	void AddToFolderNodesMap( const HTREEITEM item );

	// Removes a tree 'item from the folder nodes map.
	void RemoveFromFolderNodesMap( const HTREEITEM item );

	// Returns the tree item corresponding to the computer 'folder', or nullptr if the node was not found.
	HTREEITEM GetComputerFolderNode( const std::wstring& folder );

	// Starts the thread which updates the media information for the 'scratchList'.
	void StartScratchListUpdateThread( Playlist::Ptr scratchList );

	// Stops the thread which updates the media information for the scratch list.
	void StopScratchListUpdateThread();

	// Starts the thread which handles file modification notifications.
	void StartFileModifiedThread();

	// Stops the thread which handles file modification notifications.
	void StopFileModifiedThread();

	// Returns whether to ignore a file monitor event for the 'filename'.
	bool IgnoreFileMonitorEvent( const std::wstring& filename ) const;

	// Returns the default edit control window procedure.
	WNDPROC GetEditControlWndProc();

	// Returns the icon size for the image list.
	int GetIconSize() const;

	// Sets the tree control colours.
	void SetColours();

	// Returns the font colour (which is applied when not in high contrast mode).
	COLORREF GetFontColour() const;

	// Returns the background colour (which is applied when not in high contrast mode).
	COLORREF GetBackgroundColour() const;

	// Updates the 'item' icon, based on the output 'state'.
	void UpdateIcon( const HTREEITEM item, const Output::State state );

	// Returns the playlist type, original icon index & item order values for the 'item'.
	std::tuple<Playlist::Type, int /*originalIconIndex*/, int /*itemOrder*/> GetItemData( const HTREEITEM item ) const;

	// Inserts the Add to Playlists context sub menu into the 'menu'.
	// 'insertAfterItemID' - the menu item ID after which to add the sub menu.
	// 'insertSeparator' - whether to insert a separator to the 'menu' before adding the sub menu.
	void InsertAddToPlaylists( const HMENU menu, const UINT insertAfterItemID, const bool insertSeparator );

	// Called when the Add to Playlist 'command' is received.
	void OnAddToPlaylist( const UINT command );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Playlists node.
	HTREEITEM m_NodePlaylists;

	// Artists node.
	HTREEITEM m_NodeArtists;

	// Albums node.
	HTREEITEM m_NodeAlbums;

	// Genres node.
	HTREEITEM m_NodeGenres;

	// Years node.
	HTREEITEM m_NodeYears;

	// All tracks node.
	HTREEITEM m_NodeAll;

	// Favourites node.
	HTREEITEM m_NodeFavourites;

	// Streams node.
	HTREEITEM m_NodeStreams;

	// Computer node.
	HTREEITEM m_NodeComputer;

	// Current output playlist node.
	HTREEITEM m_NodeCurrentOutput;

	// Media library.
	Library& m_Library;

	// Application settings.
	Settings& m_Settings;

	// CD audio manager.
	CDDAManager& m_CDDAManager;

	// Output object.
	Output& m_Output;

	// Playlists.
	PlaylistMap m_PlaylistMap;

	// Artists.
	PlaylistMap m_ArtistMap;

	// Albums.
	PlaylistMap m_AlbumMap;

	// Genres.
	PlaylistMap m_GenreMap;

	// Years.
	PlaylistMap m_YearMap;

	// CD audio playlists.
	PlaylistMap m_CDDAMap;

	// Folder playlists.
	PlaylistMap m_FolderPlaylistMap;

	// All Tracks playlist.
	Playlist::Ptr m_PlaylistAll;

	// Favourites playlist.
	Playlist::Ptr m_PlaylistFavourites;

	// Streams playlist.
	Playlist::Ptr m_PlaylistStreams;

	// The font resulting from the font selection dialog.
	HFONT m_ChosenFont;

	// Font colour.
	COLORREF m_ColourFont;

	// Background colour.
	COLORREF m_ColourBackground;

	// Highlight colour.
	COLORREF m_ColourHighlight;

	// Icon colour.
	COLORREF m_ColourIcon;

	// Image list.
	HIMAGELIST m_ImageList;

	// Icon map.
	IconMap m_IconMap;

	// Icon index for the computer icon.
	int m_IconIndexComputer;

	// Icon index for the folder icon.
	int m_IconIndexFolder;

	// Icon index for the drive icon.
	int m_IconIndexDrive;

	// Icon index for the currently playing icon.
	int m_IconIndexPlaying;

	// Icon index for the currently paused icon.
	int m_IconIndexPaused;

	// The root computer folders.
	RootFolderInfoMap m_RootComputerFolders;

	// Maps a folder path to the set of tree items.
	StringToNodesMap m_FolderNodesMap;

	// Mutex for the folder nodes map.
	std::mutex m_FolderNodesMapMutex;

	// Mutex for the folder playlists map.
	std::mutex m_FolderPlaylistMapMutex;

	// Folder monitor.
	FolderMonitor m_FolderMonitor;

	// File modification thread handle.
	HANDLE m_FileModifiedThread;

	// File modification thread stop event.
	HANDLE m_FileModifiedStopEvent;

	// File modification thread wake event.
	HANDLE m_FileModifiedWakeEvent;

	// Files modified.
	std::set<std::wstring> m_FilesModified;

	// Mutex for the files modified.
	std::mutex m_FilesModifiedMutex;

	// Scratch list update thread handle.
	HANDLE m_ScratchListUpdateThread;

	// Scratch list stop event handle.
	HANDLE m_ScratchListUpdateStopEvent;

	// Whether duplicate tracks are merged into a single entry (for Artist/Album/Genre/Year playlists).
	bool m_MergeDuplicates;

	// All file extensions supported by the (decoder) handlers.
	const std::set<std::wstring> m_SupportedFileExtensions;

	// Default edit control window procedure.
	WNDPROC m_EditControlWndProc;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;

	// Maps a menu command ID to a playlist for the Add to Playlist context sub menu.
	PlaylistMenuMap m_AddToPlaylistMenuMap;

	// Root item ordering.
	static OrderMap s_RootOrder;
};

