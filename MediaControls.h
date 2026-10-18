#pragma once

#include "stdafx.h"

#include "Output.h"

#include <wrl/client.h>

#include <windows.media.h>

#include <string>
#include <vector>

class Library;
class WndToolbarPlayback;

// Message ID for signalling that a system media transport control button has been pressed.
// 'wParam' : MediaControls::Button value.
// 'lParam' : unused.
static constexpr UINT MSG_MEDIACONTROLBUTTON = WM_APP + 81;

// Integrates with the Windows System Media Transport Controls (Windows 10 and later).
// This allows media keys to control playback when the application does not have focus,
// and shows the current track in the Windows media flyout & lock screen.
// On earlier versions of Windows, all functionality is disabled.
class MediaControls
{
public:
	// Media control buttons.
	enum class Button {
		Play,
		Pause,
		Stop,
		Previous,
		Next
	};

	// 'parent' - main application window handle (which receives MSG_MEDIACONTROLBUTTON messages).
	MediaControls( HWND parent );

	virtual ~MediaControls();

	// Sets whether the media controls are enabled.
	void Enable( const bool enable );

	// Returns whether the media controls are active (enabled, and supported by the operating system).
	bool IsActive() const;

	// Updates the media control state.
	// 'toolbarPlayback' - playback toolbar.
	// 'output' - output object.
	// 'library' - media library.
	// 'playlist' - the currently displayed playlist.
	void Update( WndToolbarPlayback& toolbarPlayback, Output& output, Library& library, const Playlist::Ptr& playlist );

private:
	// Media control button pressed event handler.
	class ButtonPressedHandler;

	// Function signatures for the dynamically loaded WinRT functions.
	using RoGetActivationFactoryFunc = HRESULT ( WINAPI* )( HSTRING, REFIID, void** );
	using RoActivateInstanceFunc = HRESULT ( WINAPI* )( HSTRING, IInspectable** );
	using WindowsCreateStringFunc = HRESULT ( WINAPI* )( PCNZWCH, UINT32, HSTRING* );
	using WindowsDeleteStringFunc = HRESULT ( WINAPI* )( HSTRING );
	using CreateRandomAccessStreamOverStreamFunc = HRESULT ( WINAPI* )( IStream*, UINT, REFIID, void** );

	// Loads the WinRT functions and initialises the media controls.
	// Returns whether the media controls were initialised.
	bool Init();

	// Clears the media controls and releases all resources.
	void Release();

	// Updates the playback status & enabled buttons.
	// 'toolbarPlayback' - playback toolbar.
	// 'state' - output state.
	// 'hasDisplayItem' - whether there is an item to display.
	void UpdateStatus( WndToolbarPlayback& toolbarPlayback, const Output::State state, const bool hasDisplayItem );

	// Updates the track information & artwork.
	// 'item' - the item to display (the playing item, or the selected item when stopped).
	// 'playlistName' - the playlist name to display when there is no item to display (when stopped with no selected item).
	// 'library' - media library.
	void UpdateDisplay( const Output::Item& item, const std::wstring& playlistName, Library& library );

	// Updates the timeline (position & duration).
	// 'item' - the playing item (or an empty item when stopped).
	void UpdateTimeline( const Output::Item& item );

	// Sets the 'artwork' image as the display thumbnail, or clears the thumbnail if 'artwork' is empty.
	void SetThumbnail( ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater* displayUpdater, const std::vector<BYTE>& artwork );

	// Returns an HSTRING for the 'str', which must be freed with WindowsDeleteString (nullptr for an empty string, or on failure).
	HSTRING CreateHString( const std::wstring& str ) const;

	// Main application window handle.
	const HWND m_hWnd;

	// WinRT library module.
	HMODULE m_CombaseModule = nullptr;

	// Shell core library module.
	HMODULE m_ShcoreModule = nullptr;

	// Dynamically loaded WinRT functions.
	RoGetActivationFactoryFunc m_RoGetActivationFactory = nullptr;
	RoActivateInstanceFunc m_RoActivateInstance = nullptr;
	WindowsCreateStringFunc m_WindowsCreateString = nullptr;
	WindowsDeleteStringFunc m_WindowsDeleteString = nullptr;
	CreateRandomAccessStreamOverStreamFunc m_CreateRandomAccessStreamOverStream = nullptr;

	// System media transport controls.
	Microsoft::WRL::ComPtr<ABI::Windows::Media::ISystemMediaTransportControls> m_Controls;

	// Timeline properties.
	Microsoft::WRL::ComPtr<ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties> m_TimelineProperties;

	// Button pressed event registration token.
	EventRegistrationToken m_ButtonPressedToken = {};

	// Current playback status.
	ABI::Windows::Media::MediaPlaybackStatus m_PlaybackStatus = ABI::Windows::Media::MediaPlaybackStatus_Closed;

	// Current enabled state of the play/pause, stop, previous & next buttons.
	bool m_PlayEnabled = false;
	bool m_StopEnabled = false;
	bool m_PreviousEnabled = false;
	bool m_NextEnabled = false;

	// Current displayed track information.
	long m_DisplayItemID = -1;
	std::wstring m_DisplayTitle;
	std::wstring m_DisplayArtist;
	std::wstring m_DisplayAlbum;
	std::wstring m_DisplayArtworkID;

	// Current timeline duration & position, in whole seconds (-1 when no timeline is shown).
	long long m_TimelineDuration = -1;
	long long m_TimelinePosition = -1;
};
