#pragma once

#include "stdafx.h"

#include <shellapi.h>

#include "Output.h"
#include "Playlist.h"
#include "Settings.h"

class WndList
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'settings' - application settings.
	// 'output' - output object.
	WndList( HINSTANCE instance, HWND parent, Settings& settings, Output& output );

	virtual ~WndList();

	// Returns the default window procedure.
	WNDPROC GetDefaultWndProc();

	// Gets the list control handle.
	HWND GetWindowHandle();

	// Called when files are dropped onto the list.
	// 'hDrop' - handle describing the dropped files.
	void OnDropFiles( const HDROP hDrop );

	// Starts playback at the playlist 'itemID'.
	void OnPlay( const long itemID );

	// Starts playback at the currently selected item.
	void PlaySelected();

	// Displays the context menu at the specified 'position', in screen coordinates.
	void OnContextMenu( const POINT& position );

	// Called when a 'command' is received.
	void OnCommand( const UINT command );

	// Saves the current list control settings.
	void SaveSettings();

	// Deletes the currently selected items from the list.
	void DeleteSelectedItems();

	// Sorts the playlist by 'column' type.
	void SortPlaylist( const Playlist::Column column );

	// Sets the current 'playlist'.
	// 'initSelection' - whether to select the first playlist item (or the currently playing item if it's in the list).
	// 'fileToSelect' - the file to select.
	void SetPlaylist( const Playlist::Ptr playlist, const bool initSelection = true, const std::optional<MediaInfo>& fileToSelect = std::nullopt );

	// Called when an 'item' is added to the 'playlist' at a (0-based) 'position'.
	void OnFileAdded( Playlist* playlist, const Playlist::Item& item, const int position );

	// Called when an 'item' is removed from the 'playlist'.
	void OnFileRemoved( Playlist* playlist, const Playlist::Item& item );

	// Called when an 'item' is updated in the 'playlist'.
	void OnItemUpdated( Playlist* playlist, const Playlist::Item& item );

	// Returns the highlight colour (which is applied when not in high contrast mode).
	COLORREF GetHighlightColour() const;

	// Returns the status icon colour (which is applied when not in high contrast mode).
	COLORREF GetStatusIconColour() const;

	// Return whether the status icon is enabled.
	bool GetStatusIconEnabled() const;

	// Called when label editing begins on an 'item'.
	// Returns TRUE if label editing is allowed, FALSE if not.
	BOOL OnBeginLabelEdit( const LVITEM& item );

	// Called when label editing ends on an 'item'.
	void OnEndLabelEdit( const LVITEM& item );

	// Called when a column has been dragged to a different position.
	void OnEndDragColumn();

	// Ensures the dummy column is at the zero position after a column has been dragged.
	void ReorderDummyColumn();

	// Called when a list control drag operation is started on the 'itemIndex'.
	void OnBeginDrag( const int itemIndex );

	// Called when a list control drag operation finishes.
	void OnEndDrag();

	// Called when the mouse moves during a drag operation.
	// 'point' - mouse position, in client coordinates.
	void OnDrag( const POINT& point );

	// Called when the drag timer interval elapses.
	void OnDragTimer();

	// Updates the list control media information.
	// 'mediaInfo' - updated media information
	void OnUpdatedMedia( const MediaInfo& mediaInfo );

	// Returns the playlist item ID corresponding to the list control 'itemIndex'.
	long GetPlaylistItemID( const int itemIndex );

	// Returns the current playlist associated with the list control.
	Playlist::Ptr GetPlaylist();

	// Returns the currently focused playlist item.
	Playlist::Item GetCurrentSelectedItem();

	// Returns the currently focused list control item index.
	int GetCurrentSelectedIndex();

	// Selects the previous item in the playlist.
	void SelectPreviousItem();

	// Selects the next item in the playlist.
	void SelectNextItem();

	// Ensures a playlist 'item' is visible, and optionally selected.
	void EnsureVisible( const Playlist::Item& item, const bool select = true );

	// Returns the list of selected playlist items.
	Playlist::Items GetSelectedPlaylistItems();

	// Returns the number of selected playlist items.
	int GetSelectedCount() const;

	// Returns the number of playlist items.
	int GetCount() const;

	// Launches a file selector dialog to add a folder to the playlist.
	void OnCommandAddFolder();

	// Launches a file selector dialog to add files to the playlist.
	void OnCommandAddFiles();

	// Launches a dialog to add a network stream to the playlist.
	void OnCommandAddStream();

	// Called when a 'command' is received to show or hide a column.
	void OnShowColumn( const UINT command );

	// Called when a 'command' is received to sort the list.
	void OnSortPlaylist( const UINT command );

	// Gets the command IDs of the 'shown' & 'hidden' columns.
	void GetColumnVisibility( std::set<UINT>& shown, std::set<UINT>& hidden );

	// Displays the font selection dialog for changing the list control font.
	void OnSelectFont();

	// Displays the colour selection dialog for changing the list control colours.
	// 'commandID' - command ID indicating which colour to change.
	void OnSelectColour( const UINT commandID );

	// Cuts or copies selected files from the playlist into the clipboard (if a list item label is not being edited).
	// 'cut' - true to cut the files, false to copy the files.
	void OnCutCopy( const bool cut );

	// Pastes files into the playlist from the clipboard (if a list item label is not being edited).
	void OnPaste();

	// Selects all list control items (if a list item label is not being edited).
	void OnSelectAll();

	// Selects, if present, the 'itemID', and deselects all other items.
  // Returns whether the item was selected.
	bool SelectPlaylistItem( const long itemID );

	// Updates the status icon.
	void UpdateStatusIcon();

	// Called on a system colour change event.
	// 'isHighContrast' - indicates whether high contrast mode is active.
	void OnSysColorChange( const bool isHighContrast );

  // Called when display information for 'lvItem' needs updating in the virtual list control.
  void OnDisplayInfo( LVITEM& lvItem );

  // Called when an item needs to be found in the virtual list control.
  // 'findInfo' - the information to find.
  // 'startIndex' - the index at which to start the search.
  // Returns the list control index of the found item, or -1 if there is no match.
  int OnFindItem( const LVFINDINFO& findInfo, const int startIndex );

  // Refreshes the list control.
  void RefreshPlaylist();

private:
	// Column format information.
	struct ColumnFormat {
		UINT ShowID;					// Show command ID.
		UINT SortID;					// Sort command ID.
		UINT HeaderID;				// Column header string resource ID.
		UINT Alignment;				// Column alignment.
		int Width;						// Default column width.
		bool CanEdit;					// Whether column entries can be edited.
	};

	// Added item information.
	struct AddedItem {
		Playlist* Playlist;   // Playlist pointer.
		Playlist::Item Item;	// Playlist item.
		int Position;					// Added item position (0-based).
	};

	// Window procedure
	static LRESULT CALLBACK ListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Edit control window procedure
	static LRESULT CALLBACK EditControlProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Maps a column type to the column format.
	using ColumnFormats = std::map<Playlist::Column,ColumnFormat>;

	// Maps a filename (with optional cues) to an item ID collection.
	using FilenameToIDs = std::map<std::tuple<std::wstring /*filename*/, std::optional<long> /*cueStart*/, std::optional<long> /*cueEnd*/>, std::set<long>>;

	// Maps an output state to an icon index.
	using IconMap = std::map<Output::State, int>;

	// Returns the default edit control window procedure.
	WNDPROC GetEditControlWndProc();

	// Adjusts the edit control 'position' when item editing commences.
	void RepositionEditControl( WINDOWPOS* position );

	// Applies the current settings to the list.
	void ApplySettings();

	// Adds 'filename' to the list of files to add to the playlist.
	void AddFileToPlaylist( const std::wstring& filename );

	// Adds 'folder' to the list of files to add to the playlist.
	void AddFolderToPlaylist( const std::wstring& folder );

	// Inserts a 'playlistItem' into the list control.
	// 'position' item position, or -1 to append the item to the end of the list control.
	void InsertListViewItem( const Playlist::Item& playlistItem, const int position = -1 );

	// Returns whether the 'column' is shown.
	bool IsColumnShown( const Playlist::Column& column ) const;

	// Updates the sort indicator on the list control header.
	void UpdateSortIndicator();

	// Gets the current list control font.
	LOGFONT GetFont();

	// Sets the current list control font.
	void SetFont( const LOGFONT& logFont );

	// Moves selected items to 'insertionIndex'.
	void MoveSelectedItems( const int insertionIndex );

	// Shows or hides a column.
	// 'column' - column type.
	// 'width' - column width.
	// 'show' - true to show, false to hide.
	void ShowColumn( const Playlist::Column column, const int width, const bool show );

	// Adds a playlist item to the list control.
	// 'addedItem' - added item information.
	void AddFileHandler( const AddedItem* addedItem );

	// Removes a playlist item from the list control.
	// 'removedItemID' - removed item ID.
	void RemoveFileHandler( const long removedItemID );

	// Updates a playlist item in the list control.
	// 'item' - updated playlist item.
	void ItemUpdatedHandler( const Playlist::Item* item );

	// Creates the image list for the status icon.
	void CreateImageList();

	// Returns the list control index of the playlist 'itemID', or -1 if the item was not found.
	int FindItemIndex( const long itemID );

	// Updates the visibility of the status icon column.
	void ShowStatusIconColumn();

	// Returns the size of the status icon.
	int GetStatusIconSize() const;

	// Sets the list control colours.
	void SetColours();

	// Returns the font colour (which is applied when not in high contrast mode).
	COLORREF GetFontColour() const;

	// Returns the background colour (which is applied when not in high contrast mode).
	COLORREF GetBackgroundColour() const;

  // Deselects all list control items.
  void DeselectAllItems();

  // Refreshes the list control item at the 'itemIndex'.
  void RefreshItem( const int itemIndex );

	// Column format information.
	static ColumnFormats s_ColumnFormats;

	// Next available child window ID.
	static UINT_PTR s_WndListID;

	// Module instance handle.
	HINSTANCE m_hInst;

	// Window handle.
	HWND m_hWnd;

	// Default window procedure.
	WNDPROC m_DefaultWndProc;

	// Playlist.
	Playlist::Ptr m_Playlist;

	// Application settings.
	Settings& m_Settings;

	// Output object.
	Output& m_Output;

	// Font colour.
	COLORREF m_ColourFont;

	// Background colour.
	COLORREF m_ColourBackground;

	// Highlight colour.
	COLORREF m_ColourHighlight;

	// Status icon colour.
	COLORREF m_ColourStatusIcon;

	// The font resulting from the font selection dialog.
	HFONT m_ChosenFont;

	// Indicates the item index being edited.
	int m_EditItem;

	// Indicates the subitem index being edited.
	int m_EditSubItem;

	// Default edit control window procedure.
	WNDPROC m_EditControlWndProc;

	// Indicates whether list control item(s) are being dragged.
	bool m_IsDragging;

	// Image list when dragging.
	HIMAGELIST m_DragImage;

	// Cursor to set back when a drag operation finishes.
	HCURSOR m_OldCursor;

	// Maps a filename (with optional cues) to an item ID collection.
	FilenameToIDs m_FilenameToIDs;

	// The file to select when setting the playlist.
	std::optional<MediaInfo> m_FileToSelect;

  // Whether to select the first list control item when a playlist item is added to the playlist.
  bool m_SelectFirstItem = false;

	// Maps an output state to an icon index.
	IconMap m_IconMap;

	// Indicates the item ID that currently has a status icon, and the state.
	std::pair<int, Output::State> m_IconStatus;

	// Indicates whether the status icon is enabled.
	bool m_EnableStatusIcon;

	// Indicates whether high contrast mode is active.
	bool m_IsHighContrast;
};
