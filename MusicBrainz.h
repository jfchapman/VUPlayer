#pragma once

#include "stdafx.h"

#include "Settings.h"

#include <atomic>
#include <tuple>

// MusicBrainz handler for CD audio metadata.
class MusicBrainz
{
public:
	// Album information.
	struct Album
	{
		// Maps a track number to Title/Artist/Year information.
		std::map<long,std::tuple<std::wstring /*title*/, std::wstring /*artist*/, long /*year*/>> Tracks;

		std::wstring Title;
		std::wstring Artist;
		long Year;		
		std::vector<unsigned char> Artwork;
	};

	// An array of albums.
	using Albums = std::vector<Album>;

	// Query result.
	struct Result
	{
		std::string DiscID;		// MusicBrainz Disc ID.
		Albums Albums;				// Matching albums.
	};

	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'settings' - application settings.
	MusicBrainz( const HINSTANCE instance, const HWND hwnd, Settings& settings );

	virtual ~MusicBrainz();

	// Performs a MusicBrainz query.
	// 'discID' - MusicBrainz Disc ID
	// 'toc' - CD table of contents.
	// 'forceDialog' - whether to show a dialog even for an exact match.
	void Query( const std::string& discID, const std::string& toc, const bool forceDialog );

	// Displays a dialog allowing one of the matches to be selected from the 'result'.
	// Returns the album index of the selected match, or -1 if a match was not selected.
	int ShowMatchesDialog( const Result& result );

	// Returns whether there is an active query.
	bool IsActive() const;

	// Returns whether MusicBrainz functionality is available.
	bool IsAvailable() const;

private:
	// Match dialog initialisation information.
	struct MatchInfo {
		// Query result.
		const Result& m_Result;

		// MusicBrainz object.
		MusicBrainz* m_MusicBrainz;
	};

	// Query ID.
	struct QueryID
	{
		std::string DiscID;		// MusicBrainz Disc ID.
		std::string TOC;			// CD table of contents.
	};

	// Pairs a query ID with a flag to indicate whether a dialog should be shown even for a single match.
	using PendingQuery = std::pair<QueryID,bool>;

	// A list of pending queries.
	using PendingQueryList = std::list<PendingQuery>;

		// A callback which returns true to continue.
	using CanContinue = std::function<bool()>;

	// Query thread procedure.
	// 'lParam' - thread parameter.
	static DWORD WINAPI QueryThreadProc( LPVOID lParam );

	// Match dialog box procedure.
	static INT_PTR CALLBACK MatchDialogProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Query thread handler.
	void QueryHandler();

	// Called when the match dialog is initialised.
	// 'hwnd' - dialog window handle.
	// 'result' - query result.
	void OnInitMatchDialog( const HWND hwnd, const Result& result );

	// Updates the dialog track list based on the currenty selected match.
	// 'hwnd' - dialog window handle.
	// 'result' - query result.
	void UpdateDialogTrackList( const HWND hwnd, const Result& result );

	// Updates the dialog artwork based on the currenty selected match.
	// 'hwnd' - dialog window handle.
	// 'result' - query result.
	void UpdateDialogArtwork( const HWND hwnd, const Result& result );

	// Called when painting is required for the artwork control.
	void OnDrawArtwork( DRAWITEMSTRUCT* drawItemStruct );

	// Performs a MusicBrainz API lookup using the 'discID' & 'toc'.
	// Returns the response, or an empty string if the lookup failed for any reason.
	std::string LookupDisc( const std::string& discID, const std::string& toc ) const;

	// Performs Cover Art Archive API lookup using the 'releaseID'.
	// Returns the front cover art, or an empty vector if the lookup failed for any reason.
	std::vector<unsigned char> LookupCoverArt( const std::string& releaseID ) const;

	// Parses an API disc lookup 'response' into a 'result'.
	// 'canContinue' - callback which returns whether to continue.
	// Returns whether the response was parsed successfully.
	bool ParseDiscResponse( const std::string& response, Result& result, CanContinue canContinue ) const;

	// Module instance handle.
	const HINSTANCE m_hInst;

	// Application window handle.
	const HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Pending queries.
	PendingQueryList m_PendingQueries;

	// Pending querires mutex.
	std::mutex m_PendingQueriesMutex;

	// Query thread handle.
	HANDLE m_QueryThread;

	// Event handle with which to stop the query thread.
	HANDLE m_StopEvent;

	// Event handle with which to wake the query thread.
	HANDLE m_WakeEvent;

	// Indicates whether there is an active query.
	std::atomic<bool> m_ActiveQuery;

	// Internet session handle.
	HINTERNET m_InternetSession = nullptr;

	// MusicBrainz internet connection handle.
	HINTERNET m_InternetConnectionMusicBrainz = nullptr;

	// Cover Art Archive internet connection handle.
	HINTERNET m_InternetConnectionCoverArtArchive = nullptr;

	// Current artwork.
	std::unique_ptr<Gdiplus::Bitmap> m_Artwork = nullptr;
};
