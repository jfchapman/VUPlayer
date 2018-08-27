#pragma once

#include "stdafx.h"

#include "Settings.h"

#include "gnsdk_vuplayer.h"

#include <atomic>
#include <tuple>

// Gracenote wrapper.
class Gracenote
{
public:
	// Track information.
	struct Track
	{
		std::wstring Title;
		std::wstring Artist;
		std::wstring Genre;
		long Year;		
	};

	// Album information.
	struct Album
	{
		// Maps a track number to its information
		typedef std::map<long,Track> TrackMap;

		TrackMap Tracks;

		std::wstring Title;
		std::wstring Artist;
		std::wstring Genre;
		long Year;		
		std::vector<BYTE> Artwork;
	};

	// An array of albums.
	typedef std::vector<Album> Albums;

	// Query result.
	struct Result
	{
		std::string TOC;	// CD table of contents.
		bool ExactMatch;	// Indicates whether there was an exact match.
		Albums Albums;		// Matching albums.
	};

	// 'instance' - module instance handle.
	// 'hwnd' - application window handle.
	// 'settings' - application settings.
	Gracenote( const HINSTANCE instance, const HWND hwnd, Settings& settings );

	virtual ~Gracenote();

	// Returns whether Gracenote functionality is available.
	bool IsAvailable() const;

	// Performs a Gracenote query.
	// 'toc' - CD table of contents.
	// 'forceDialog' - whether to show a dialog even for an exact match.
	void Query( const std::string& toc, const bool forceDialog );

	// Displays a dialog allowing one of the matches to be selected from the 'result'.
	// Returns the album index of the selected match, or -1 if a match was not selected.
	int ShowMatchesDialog( const Result& result );

	// Closes Gracenote functionality.
	void Close();

	// Returns whether there is an active query.
	bool IsActive() const;

private:
	// Match dialog initialisation information.
	struct MatchInfo {
		// Query result.
		const Result& m_Result;

		// Gracenote object.
		Gracenote* m_Gracenote;
	};

	// Initialisation function type.
	typedef const char* (*gn_init)( const char*, const char*, const char* );

	// Close function type.
	typedef void (*gn_close)( const char* );

	// Query function type.
	typedef const gn_result* (*gn_query)( const char*, const char* );

	// Free result function type.
	typedef void (*gn_free_result)( const gn_result* );

	// Pairs a table of contents with a flag to indicate whether a dialog even for an exact match.
	typedef std::pair<std::string,bool> PendingQuery;

	// A list of pending queries.
	typedef std::list<PendingQuery> PendingQueryList;

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

	// Module instance handle.
	const HINSTANCE m_hInst;

	// Application window handle.
	const HWND m_hWnd;

	// Application settings.
	Settings& m_Settings;

	// Gracenote library handle.
	HMODULE m_Handle;

	// Initialisation function.
	gn_init m_Init;

	// Close function.
	gn_close m_Close;

	// Query function.
	gn_query m_Query;

	// Free result function.
	gn_free_result m_FreeResult;

	// Indicates whether Gracenote has been initialised.
	bool m_Initialised;

	// Current Gracenote user ID.
	std::string m_UserID;

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
};
