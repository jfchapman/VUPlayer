#include "Database.h"

#include "Utility.h"

// SQLite error log callback procedure.
static void ErrorLogCallback( void* arg, int errorCode, const char* message )
{
	Database* db = reinterpret_cast<Database*>( arg );
	if ( nullptr != db ) {
		db->AppendToErrorLog( errorCode, message );
	}
}

Database::Database( const std::wstring& filename ) :
	m_Database( nullptr ),
	m_LogMutex(),
	m_Log()
{
	int result = sqlite3_config( SQLITE_CONFIG_LOG, ErrorLogCallback, this );
	result = sqlite3_initialize();

	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	if ( SQLITE_OK != sqlite3_open_v2( WideStringToUTF8( filename ).c_str(), &m_Database, flags, NULL /*vfs*/ ) ) {
		throw std::runtime_error( "Could not open database" );
	}
}

Database::~Database()
{
	sqlite3_close( m_Database );
}

sqlite3* Database::GetDatabase()
{
	return m_Database;
}

void Database::AppendToErrorLog( const int errorCode, const std::string& message )
{
	std::lock_guard<std::mutex> lock( m_LogMutex );
	const std::string entry = std::to_string( errorCode ) + ": " + message;
	m_Log.push_back( entry );
}
