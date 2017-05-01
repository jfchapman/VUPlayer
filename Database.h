#pragma once

#include "stdafx.h"

#include <sqlite3.h>

#include <list>
#include <mutex>
#include <string>

class Database
{
public:
	// 'filename' - database file name.
	Database( const std::wstring& filename );

	virtual ~Database();

	// Returns the SQLite database.
	sqlite3* GetDatabase();

	// Appends an 'errorCode' & 'message' entry to the error log.
	void AppendToErrorLog( const int errorCode, const std::string& message );

private:
	// SQLite database.
	sqlite3* m_Database;

	// Error log mutex.
	std::mutex m_LogMutex;

	// Error log.
	std::list<std::string> m_Log;
};

