#include "Database.h"

#include "Utility.h"

Database::Database( const std::wstring& filename, const Mode mode ) :
	m_Database( nullptr ),
	m_Filename( filename ),
	m_Mode( ( filename.empty() && ( Mode::Disk == mode ) ) ? Mode::Memory : mode ),
	m_LogMutex(),
	m_Log()
{
	int result = sqlite3_config( SQLITE_CONFIG_LOG, ErrorLogCallback, this );
	result = sqlite3_initialize();

	std::string databaseName;
	switch ( m_Mode ) {
		case Mode::Disk: {
			databaseName = WideStringToUTF8( m_Filename );
			break;
		}
		case Mode::Memory: {
			databaseName = ":memory:";
			break;
		}
		case Mode::Temp:
		default: {
			break;
		}
	}

	result = sqlite3_open_v2( databaseName.c_str(), &m_Database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL /*vfs*/ );
	if ( SQLITE_OK == result ) {
		if ( !m_Filename.empty() ) {
			if ( Mode::Disk != m_Mode ) {
				// Restore the contents from the on-disk database.
				sqlite3* srcDatabase = nullptr;
				result = sqlite3_open_v2( WideStringToUTF8( m_Filename ).c_str(), &srcDatabase, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL /*vfs*/ );
				if ( SQLITE_OK == result ) {
					sqlite3_backup* backup = sqlite3_backup_init( m_Database /*dest*/, "main", srcDatabase /*src*/, "main" );
					if ( nullptr != backup ) {
						sqlite3_backup_step( backup, -1 /*allPages*/ );
						sqlite3_backup_finish( backup );
						backup = nullptr;
					}
					sqlite3_close( srcDatabase );
					srcDatabase = nullptr;
					result = sqlite3_errcode( m_Database );
				}
			}

			if ( SQLITE_OK != result ) {
				m_Log.clear();
				sqlite3_close( m_Database );
				m_Database = nullptr;

				if ( SQLITE_CORRUPT == result ) {
					// Make a copy of the corrupt database file, if possible.
					_wrename( filename.c_str(), std::wstring( filename + L".corrupt" ).c_str() );
				}
				_wunlink( filename.c_str() );
			}

			if ( nullptr == m_Database ) {
				// Something has gone wrong restoring the on-disk database, so create a new database.
				result = sqlite3_open_v2( databaseName.c_str(), &m_Database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL /*vfs*/ );
			}
		}
	}
}

Database::~Database()
{
	if ( nullptr != m_Database ) {
		if ( !m_Filename.empty() && ( Mode::Disk != m_Mode ) ) {
			// Write out the temporary database to disk.
			sqlite3* diskDatabase = nullptr;
			const std::wstring tmpName = m_Filename + L".tmp";
			int result = sqlite3_open_v2( WideStringToUTF8( tmpName ).c_str(), &diskDatabase, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL /*vfs*/ );
			if ( SQLITE_OK == result ) {
				sqlite3_backup* backup = sqlite3_backup_init( diskDatabase /*dest*/, "main", m_Database /*src*/, "main" );
				if ( nullptr != backup ) {
					sqlite3_backup_step( backup, -1 /*allPages*/ );
					sqlite3_backup_finish( backup );
					backup = nullptr;
				}
				result = sqlite3_errcode( diskDatabase );
				sqlite3_close( diskDatabase );
				diskDatabase = nullptr;

				if ( SQLITE_OK == result ) {
					_wunlink( m_Filename.c_str() );
					_wrename( tmpName.c_str(), m_Filename.c_str() );
				} else {
					_wunlink( tmpName.c_str() );
				}
			}
		}
		sqlite3_close( m_Database );
		m_Database = nullptr;
	}
}

void Database::ErrorLogCallback( void* arg, int errorCode, const char* message )
{
	Database* db = reinterpret_cast<Database*>( arg );
	if ( nullptr != db ) {
		db->AppendToErrorLog( errorCode, message );
	}
}

sqlite3* Database::GetDatabase()
{
	return m_Database;
}

void Database::AppendToErrorLog( const int errorCode, const std::string& message )
{
	std::lock_guard<std::mutex> lock( m_LogMutex );
	m_Log.push_back( std::make_pair( errorCode, message ) );
}
