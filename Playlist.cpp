#include "Playlist.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <regex>

// Next available playlist item ID.
long Playlist::s_NextItemID = 0;

// Supported playlist file extensions.
constexpr std::array s_SupportedExtensions{ L"vpl", L"m3u", L"m3u8", L"pls", L"cue" };

DWORD WINAPI Playlist::PendingThreadProc( LPVOID lpParam )
{
	Playlist* playlist = reinterpret_cast<Playlist*>( lpParam );
	if ( nullptr != playlist ) {
		CoInitializeEx( NULL /*reserved*/, COINIT_APARTMENTTHREADED );
		playlist->OnPendingThreadHandler();
		CoUninitialize();
	}
	return 0;
}

Playlist::Playlist( Library& library, const std::string& id, const Type& type ) :
	m_ID( id ),
	m_Name(),
	m_Playlist(),
	m_Pending(),
	m_MutexPlaylist(),
	m_MutexPending(),
	m_PendingThread( NULL ),
	m_PendingStopEvent( NULL ),
	m_PendingWakeEvent( NULL ),
	m_RestartPendingThread( false ),
	m_Library( library ),
	m_SortColumn( ( Type::Folder == type ) ? Column::Filepath : Column::_Undefined ),
	m_SortAscending( ( Type::Folder == type ) ? true : false ),
	m_Type( type ),
	m_MergeDuplicates( false ),
	m_ShuffledPlaylist(),
	m_MutexShuffled()
{
}

Playlist::Playlist( Library& library, const Type& type ) :
	Playlist( library, GenerateGUIDString(), type )
{
}

Playlist::Playlist( Library& library, const Type& type, const bool mergeDuplicates ) :
	Playlist( library, type )
{
	SetMergeDuplicates( mergeDuplicates );
}

Playlist::~Playlist()
{
	StopPendingThread();
	CloseHandles();
}

const std::string& Playlist::GetID() const
{
	return m_ID;
}

const std::wstring& Playlist::GetName() const
{
	return m_Name;
}

void Playlist::SetName( const std::wstring& name )
{
	m_Name = name;
}

Playlist::Items Playlist::GetItems()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	return m_Playlist;
}

std::list<MediaInfo> Playlist::GetPending()
{
	std::lock_guard<std::mutex> lock( m_MutexPending );
	return m_Pending;
}

int Playlist::GetPendingCount()
{
	std::lock_guard<std::mutex> lock( m_MutexPending );
	const int pendingCount = static_cast<int>( m_Pending.size() );
	return pendingCount;
}

bool Playlist::GetItem( Item& item )
{
	int position = 0;
	const bool success = GetItem( item, position );
	return success;
}

bool Playlist::GetItem( Item& item, int& position )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	position = GetPositionNoLock( item.ID );
	if ( position >= 0 ) {
		item = m_Playlist[ position ];
		return true;
	}
	return false;
}

Playlist::Item Playlist::GetItem( const int position )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	if ( static_cast<size_t>( position ) < m_Playlist.size() ) {
		return m_Playlist[ position ];
	}
	return {};
}

long Playlist::GetItemID( const int position )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	if ( static_cast<size_t>( position ) < m_Playlist.size() ) {
		return m_Playlist[ position ].ID;
	}
	return 0;
}

int Playlist::GetItemPosition( const long itemID )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	return GetPositionNoLock( itemID );
}

int Playlist::GetPositionNoLock( const long itemID )
{
	if ( const auto it = m_ItemIDPositions.find( itemID ); ( m_ItemIDPositions.end() != it ) && ( it->second < m_Playlist.size() ) )
		return static_cast<int>( it->second );
	return -1;
}

bool Playlist::GetNextItem( const Item& currentItem, Item& nextItem, const bool wrap )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	bool success = false;
	if ( const int currentPosition = GetPositionNoLock( currentItem.ID ); currentPosition >= 0 ) {
		if ( static_cast<size_t>( 1 + currentPosition ) < m_Playlist.size() ) {
			nextItem = m_Playlist[ 1 + currentPosition ];
			success = true;
		} else if ( wrap ) {
			nextItem = m_Playlist.front();
			success = true;
		}
	}
	return success;
}

bool Playlist::GetPreviousItem( const Item& currentItem, Item& previousItem, const bool wrap )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	bool success = false;
	if ( const int currentPosition = GetPositionNoLock( currentItem.ID ); currentPosition >= 0 ) {
		if ( currentPosition > 0 ) {
			previousItem = m_Playlist[ currentPosition - 1 ];
			success = true;
		} else if ( wrap ) {
			previousItem = m_Playlist.back();
			success = true;
		}
	}
	return success;
}

Playlist::Item Playlist::GetFirstItem()
{
	Item result = {};
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	if ( !m_Playlist.empty() ) {
		result = m_Playlist.front();
	}
	return result;
}

Playlist::Item Playlist::GetRandomItem( const Item& currentItem )
{
	Item result = {};

	std::lock_guard<std::mutex> shuffleLock( m_MutexShuffled );
	while ( !m_ShuffledPlaylist.empty() ) {
		const Item nextItem = m_ShuffledPlaylist.back();
		m_ShuffledPlaylist.pop_back();
		if ( ( currentItem.ID != nextItem.ID ) && ContainsItem( nextItem ) ) {
			result = nextItem;
			break;
		}
	}

	if ( 0 == result.ID ) {
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		std::vector<Item> allItems( m_Playlist.begin(), m_Playlist.end() );
		std::shuffle( allItems.begin(), allItems.end(), GetRandomEngine() );
		m_ShuffledPlaylist = { allItems.begin(), allItems.end() };
		for ( auto item = m_ShuffledPlaylist.begin(); item != m_ShuffledPlaylist.end(); item++ ) {
			if ( item->ID == currentItem.ID ) {
				m_ShuffledPlaylist.erase( item );
				break;
			}
		}

		if ( !m_ShuffledPlaylist.empty() ) {
			result = m_ShuffledPlaylist.back();
			m_ShuffledPlaylist.pop_back();
		}
	}

	return result;
}

Playlist::Item Playlist::AddItem( const MediaInfo& mediaInfo )
{
	int position = 0;
	bool addedAsDuplicate = false;
	return AddItem( mediaInfo, position, addedAsDuplicate );
}

Playlist::Item Playlist::AddItem( const MediaInfo& mediaInfo, int& position, bool& addedAsDuplicate )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );

	Item item = {};
	position = 0;
	addedAsDuplicate = false;

	if ( m_MergeDuplicates && !mediaInfo.GetCueStart() ) {
		for ( auto& itemIter : m_Playlist ) {
			if ( itemIter.Info.IsDuplicate( mediaInfo ) ) {
				if ( itemIter.Info.GetFilename() != mediaInfo.GetFilename() ) {
					const auto foundDuplicate = std::find( itemIter.Duplicates.begin(), itemIter.Duplicates.end(), mediaInfo.GetFilename() );
					if ( itemIter.Duplicates.end() == foundDuplicate ) {
						itemIter.Duplicates.insert( mediaInfo.GetFilename() );
					}
				}
				item = itemIter;
				addedAsDuplicate = true;
				break;
			}
		}
	}

	if ( !addedAsDuplicate ) {
		item = { ++s_NextItemID, mediaInfo };
		if ( Column::_Undefined == m_SortColumn ) {
			position = static_cast<int>( m_Playlist.size() );
			m_Playlist.push_back( item );
			m_ItemIDPositions[ item.ID ] = static_cast<int>( m_Playlist.size() - 1 );
		} else {
			auto insertIter = m_Playlist.begin();
			while ( insertIter != m_Playlist.end() ) {
				if ( m_SortAscending ? LessThan( item, *insertIter, m_SortColumn ) : GreaterThan( item, *insertIter, m_SortColumn ) ) {
					break;
				} else {
					++insertIter;
					++position;
				}
			}
			m_Playlist.insert( insertIter, item );
			for ( size_t i = static_cast<size_t>( position ); i < m_Playlist.size(); i++ ) {
				m_ItemIDPositions[ m_Playlist[ i ].ID ] = i;
			}
		}
	}
	return item;
}

void Playlist::AddPending( const MediaInfo& media, const bool startPendingThread )
{
	{
		std::lock_guard<std::mutex> lock( m_MutexPending );
		m_Pending.push_back( media );
	}
	if ( startPendingThread ) {
		StartPendingThread();
	}
}

void Playlist::OnPendingThreadHandler()
{
	m_RestartPendingThread = false;
	const DWORD timeout = 10 * 1000 /*msec*/;
	HANDLE eventHandles[ 2 ] = { m_PendingStopEvent, m_PendingWakeEvent };

	while ( WaitForMultipleObjects( 2, eventHandles, FALSE /*waitAll*/, timeout ) != WAIT_OBJECT_0 ) {
		MediaInfo mediaInfo;
		{
			std::lock_guard<std::mutex> lock( m_MutexPending );
			if ( m_Pending.empty() ) {
				if ( WAIT_OBJECT_0 == WaitForSingleObject( m_PendingWakeEvent, 0 ) ) {
					// Hang around for additional pending files.
					ResetEvent( m_PendingWakeEvent );
				} else {
					// We've timed out, so self-terminate and signal that the thread should be restarted.
					m_RestartPendingThread = true;
					break;
				}
			} else {
				mediaInfo = m_Pending.front();
				m_Pending.pop_front();
			}
		}

		if ( !mediaInfo.GetFilename().empty() ) {
			bool addItem = true;
			const Type type = GetType();
			if ( ( Type::All == type ) || ( Type::Favourites == type ) || ( Type::Folder == type ) || ( Type::Streams == type ) ) {
				addItem = !ContainsFile( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() );
			}
			if ( addItem ) {
				if ( m_Library.GetMediaInfo( mediaInfo ) ) {
					int position = 0;
					bool addedAsDuplicate = false;
					const Item item = AddItem( mediaInfo, position, addedAsDuplicate );
					VUPlayer* vuplayer = VUPlayer::Get();
					if ( nullptr != vuplayer ) {
						if ( addedAsDuplicate ) {
							vuplayer->OnPlaylistItemUpdated( this, item );
						} else {
							vuplayer->OnPlaylistItemAdded( this, item, position );
						}
					}
				}
			}
		}
	}
}

void Playlist::StartPendingThread()
{
	std::lock_guard<std::mutex> lock( m_MutexPending );
	if ( nullptr != m_PendingThread ) {
		if ( m_RestartPendingThread ) {
			WaitForSingleObject( m_PendingThread, INFINITE );
			CloseHandles();
			m_RestartPendingThread = false;
		}
	}

	if ( nullptr == m_PendingThread ) {
		m_PendingStopEvent = CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ );
		m_PendingWakeEvent = CreateEvent( NULL /*attributes*/, TRUE /*manualReset*/, FALSE /*initialState*/, L"" /*name*/ );
		m_PendingThread = CreateThread( NULL /*attributes*/, 0 /*stackSize*/, PendingThreadProc, reinterpret_cast<LPVOID>( this ), 0 /*flags*/, NULL /*threadId*/ );
	}

	SetEvent( m_PendingWakeEvent );
}

void Playlist::StopPendingThread()
{
	if ( nullptr != m_PendingThread ) {
		SetEvent( m_PendingStopEvent );
		WaitForSingleObject( m_PendingThread, INFINITE );
		CloseHandles();
	}
}

Library& Playlist::GetLibrary()
{
	return m_Library;
}

void Playlist::SavePlaylist( const std::wstring& filename )
{
	std::ofstream stream( filename );
	if ( stream.is_open() ) {
		for ( const auto& iter : m_Playlist ) {
			const MediaInfo& mediaInfo = iter.Info;
			const std::wstring& name = mediaInfo.GetFilename();
			if ( !name.empty() ) {
				stream << WideStringToUTF8( name ) << '\n';
			}
		}
		stream.close();
	}
}

bool Playlist::RemoveItem( const Item& item )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	bool removed = false;
	if ( const int position = GetPositionNoLock( item.ID ); position >= 0 ) {
		m_Playlist.erase( m_Playlist.begin() + position );
		m_ItemIDPositions.erase( item.ID );
		for ( size_t i = static_cast<size_t>( position ); i < m_Playlist.size(); i++ ) {
			m_ItemIDPositions[ m_Playlist[ i ].ID ] = i;
		}
		VUPlayer* vuplayer = VUPlayer::Get();
		if ( nullptr != vuplayer ) {
			vuplayer->OnPlaylistItemRemoved( this, item );
		}
		removed = true;
	}
	return removed;
}

bool Playlist::RemoveItem( const MediaInfo& mediaInfo )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );

	bool removed = false;
	for ( auto iter = m_Playlist.begin(); iter != m_Playlist.end(); iter++ ) {
		const auto itemToFind = std::tie( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() );
		if ( std::tie( iter->Info.GetFilename(), iter->Info.GetCueStart(), iter->Info.GetCueEnd() ) == itemToFind ) {
			if ( iter->Duplicates.empty() ) {
				const size_t position = std::distance( m_Playlist.begin(), iter );
				const Item item = *iter;
				m_Playlist.erase( iter );
				m_ItemIDPositions.erase( item.ID );
				for ( size_t i = position; i < m_Playlist.size(); i++ ) {
					m_ItemIDPositions[ m_Playlist[ i ].ID ] = i;
				}
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					vuplayer->OnPlaylistItemRemoved( this, item );
				}
				removed = true;
			} else {
				iter->Info.SetFilename( *iter->Duplicates.begin() );
				iter->Duplicates.erase( iter->Duplicates.begin() );
			}
			break;
		} else if ( !iter->Duplicates.empty() && !iter->Info.GetCueStart() ) {
			auto duplicate = std::find( iter->Duplicates.begin(), iter->Duplicates.end(), mediaInfo.GetFilename() );
			if ( iter->Duplicates.end() != duplicate ) {
				iter->Duplicates.erase( duplicate );
			}
		}
	}
	return removed;
}

bool Playlist::RemoveFiles( const MediaInfo::List& mediaList )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	std::list<Item> tempList( m_Playlist.begin(), m_Playlist.end() );
	bool anyItemsRemoved = false;
	for ( const auto& mediaInfo : mediaList ) {
		const auto& filename = mediaInfo.GetFilename();
		const auto& cueStart = mediaInfo.GetCueStart();
		const auto& cueEnd = mediaInfo.GetCueEnd();
		if ( const auto foundItem = std::find_if( tempList.begin(), tempList.end(), [ &filename, &cueStart, &cueEnd ] ( const Item& item ) {
			return std::tie( filename, cueStart, cueEnd ) == std::tie( item.Info.GetFilename(), item.Info.GetCueStart(), item.Info.GetCueEnd() );
			} ); tempList.end() != foundItem ) {
			tempList.erase( foundItem );
			anyItemsRemoved = true;
		}
	}
	if ( anyItemsRemoved ) {
		m_Playlist = { tempList.begin(), tempList.end() };
		m_ItemIDPositions.clear();
		size_t position = 0;
		for ( const auto& item : m_Playlist ) {
			m_ItemIDPositions[ item.ID ] = position++;
		}
	}
	return anyItemsRemoved;
}

long Playlist::GetCount()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	return static_cast<long>( m_Playlist.size() );
}

float Playlist::GetDuration()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	float duration = 0;
	for ( const auto& iter : m_Playlist ) {
		const MediaInfo& mediaInfo = iter.Info;
		duration += mediaInfo.GetDuration();
	}
	return duration;
}

long long Playlist::GetFilesize()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	long long filesize = 0;
	for ( const auto& iter : m_Playlist ) {
		const MediaInfo& mediaInfo = iter.Info;
		filesize += mediaInfo.GetFilesize( true /*applyCues*/ );
	}
	return filesize;
}

void Playlist::GetSort( Column& column, bool& ascending ) const
{
	column = m_SortColumn;
	ascending = m_SortAscending;
}

void Playlist::Sort( const Column column )
{
	if ( column != m_SortColumn ) {
		m_SortColumn = column;
		m_SortAscending = true;
	} else {
		m_SortAscending = !m_SortAscending;
	}
	if ( Column::_Undefined != m_SortColumn ) {
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		std::stable_sort( m_Playlist.begin(), m_Playlist.end(), [ = ] ( const Item& item1, const Item& item2 ) -> bool
			{
				return m_SortAscending ? LessThan( item1, item2, m_SortColumn ) : GreaterThan( item1, item2, m_SortColumn );
			} );
		m_ItemIDPositions.clear();
		size_t position = 0;
		for ( const auto& item : m_Playlist ) {
			m_ItemIDPositions[ item.ID ] = position++;
		}
	}
}

bool Playlist::LessThan( const Item& item1, const Item& item2, const Column column )
{
	bool lessThan = false;
	switch ( column ) {
		case Column::Album: {
			lessThan = _wcsicmp( item1.Info.GetAlbum().c_str(), item2.Info.GetAlbum().c_str() ) < 0;
			break;
		}
		case Column::Artist: {
			lessThan = _wcsicmp( item1.Info.GetArtist().c_str(), item2.Info.GetArtist().c_str() ) < 0;
			break;
		}
		case Column::Bitrate: {
			lessThan = item1.Info.GetBitrate() < item2.Info.GetBitrate();
			break;
		}
		case Column::BitsPerSample: {
			lessThan = item1.Info.GetBitsPerSample() < item2.Info.GetBitsPerSample();
			break;
		}
		case Column::Channels: {
			lessThan = item1.Info.GetChannels() < item2.Info.GetChannels();
			break;
		}
		case Column::Duration: {
			lessThan = item1.Info.GetDuration() < item2.Info.GetDuration();
			break;
		}
		case Column::Filepath: {
			const auto cmp = _wcsicmp( item1.Info.GetFilename().c_str(), item2.Info.GetFilename().c_str() );
			if ( ( 0 == cmp ) && ( item1.Info.GetCueStart() || item2.Info.GetCueStart() ) ) {
				const auto cue1 = item1.Info.GetCueStart().value_or( -1 );
				const auto cue2 = item2.Info.GetCueStart().value_or( -1 );
				lessThan = ( cue1 < cue2 );
			} else {
				lessThan = ( cmp < 0 );
			}
			break;
		}
		case Column::Filename: {
			const auto filename1 = std::filesystem::path( item1.Info.GetFilename() ).filename();
			const auto filename2 = std::filesystem::path( item2.Info.GetFilename() ).filename();
			const auto cmp = _wcsicmp( filename1.c_str(), filename2.c_str() );
			if ( ( 0 == cmp ) && ( item1.Info.GetCueStart() || item2.Info.GetCueStart() ) ) {
				const auto cue1 = item1.Info.GetCueStart().value_or( -1 );
				const auto cue2 = item2.Info.GetCueStart().value_or( -1 );
				lessThan = ( cue1 < cue2 );
			} else {
				lessThan = ( cmp < 0 );
			}
			break;
		}
		case Column::Filesize: {
			lessThan = item1.Info.GetFilesize() < item2.Info.GetFilesize();
			break;
		}
		case Column::Filetime: {
			lessThan = item1.Info.GetFiletime() < item2.Info.GetFiletime();
			break;
		}
		case Column::GainAlbum: {
			lessThan = item1.Info.GetGainAlbum() < item2.Info.GetGainAlbum();
			break;
		}
		case Column::GainTrack: {
			lessThan = item1.Info.GetGainTrack() < item2.Info.GetGainTrack();
			break;
		}
		case Column::Genre: {
			lessThan = _wcsicmp( item1.Info.GetGenre().c_str(), item2.Info.GetGenre().c_str() ) < 0;
			break;
		}
		case Column::SampleRate: {
			lessThan = item1.Info.GetSampleRate() < item2.Info.GetSampleRate();
			break;
		}
		case Column::Title: {
			lessThan = _wcsicmp( item1.Info.GetTitle( true ).c_str(), item2.Info.GetTitle( true ).c_str() ) < 0;
			break;
		}
		case Column::Track: {
			lessThan = item1.Info.GetTrack() < item2.Info.GetTrack();
			break;
		}
		case Column::Type: {
			lessThan = _wcsicmp( item1.Info.GetType().c_str(), item2.Info.GetType().c_str() ) < 0;
			break;
		}
		case Column::Version: {
			lessThan = _wcsicmp( item1.Info.GetVersion().c_str(), item2.Info.GetVersion().c_str() ) < 0;
			break;
		}
		case Column::Year: {
			lessThan = item1.Info.GetYear() < item2.Info.GetYear();
			break;
		}
		case Column::Composer: {
			lessThan = _wcsicmp( item1.Info.GetComposer().c_str(), item2.Info.GetComposer().c_str() ) < 0;
			break;
		}
		case Column::Conductor: {
			lessThan = _wcsicmp( item1.Info.GetConductor().c_str(), item2.Info.GetConductor().c_str() ) < 0;
			break;
		}
		case Column::Publisher: {
			lessThan = _wcsicmp( item1.Info.GetPublisher().c_str(), item2.Info.GetPublisher().c_str() ) < 0;
			break;
		}
	}
	return lessThan;
}

bool Playlist::GreaterThan( const Item& item1, const Item& item2, const Column column )
{
	return LessThan( item2, item1, column );
}

bool Playlist::OnUpdatedMedia( const MediaInfo& mediaInfo )
{
	bool updated = false;
	VUPlayer* vuplayer = VUPlayer::Get();
	Items itemsToRemove;
	std::set<MediaInfo> itemsToAdd;
	{
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		const auto itemToFind = std::tie( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() );
		for ( auto& item : m_Playlist ) {
			if ( std::tie( item.Info.GetFilename(), item.Info.GetCueStart(), item.Info.GetCueEnd() ) == itemToFind ) {
				if ( item.Info != mediaInfo ) {
					item.Info = mediaInfo;
					updated = true;

					if ( m_MergeDuplicates && !mediaInfo.GetCueStart() ) {
						// Split out any duplicates from the top level item, and add them back later as new items.
						for ( const auto& duplicate : item.Duplicates ) {
							MediaInfo itemToAdd( duplicate );
							m_Library.GetMediaInfo( itemToAdd, false /*scanMedia*/, false /*sendNotification*/ );
							itemsToAdd.insert( itemToAdd );
						}
						item.Duplicates.clear();

						// If the updated item now matches any other existing item, signal the item to be removed and added back later (as a duplicate).
						for ( auto& duplicateItem : m_Playlist ) {
							if ( ( item.Info.GetFilename() != duplicateItem.Info.GetFilename() ) && item.Info.IsDuplicate( duplicateItem.Info ) ) {
								itemsToRemove.push_back( item );
								itemsToAdd.insert( item.Info );
								break;
							}
						}
					}
				}
			} else if ( m_MergeDuplicates && !mediaInfo.GetCueStart() ) {
				// If a duplicate of a top level item has been updated, split it out and add it back later as a new item.
				for ( auto duplicate = item.Duplicates.begin(); item.Duplicates.end() != duplicate; duplicate++ ) {
					if ( *duplicate == mediaInfo.GetFilename() ) {
						if ( !item.Info.IsDuplicate( mediaInfo ) ) {
							MediaInfo itemToAdd( mediaInfo );
							itemToAdd.SetFilename( *duplicate );
							itemsToAdd.insert( itemToAdd );
							item.Duplicates.erase( duplicate );
							if ( nullptr != vuplayer ) {
								vuplayer->OnPlaylistItemUpdated( this, item );
							}
						}
						break;
					}
				}
			}
		}
	}

	if ( m_MergeDuplicates ) {
		for ( const auto& itemToRemove : itemsToRemove ) {
			RemoveItem( itemToRemove );
			if ( nullptr != vuplayer ) {
				vuplayer->OnPlaylistItemRemoved( this, itemToRemove );
			}
		}
		for ( const auto& itemToAdd : itemsToAdd ) {
			int position = 0;
			bool addedAsDuplicate = false;
			const Item item = AddItem( itemToAdd, position, addedAsDuplicate );
			if ( nullptr != vuplayer ) {
				if ( addedAsDuplicate ) {
					vuplayer->OnPlaylistItemUpdated( this, item );
				} else {
					vuplayer->OnPlaylistItemAdded( this, item, position );
				}
			}
		}
	}

	if ( updated && ( Type::CDDA == GetType() ) ) {
		for ( const auto& item : m_Playlist ) {
			if ( !item.Info.GetAlbum().empty() ) {
				SetName( item.Info.GetAlbum() );
				break;
			}
		}
	}
	return updated;
}

bool Playlist::MoveItems( const int position, const std::list<long>& itemIDs )
{
	bool changed = false;
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	std::list<Item> tempList( m_Playlist.begin(), m_Playlist.end() );
	if ( !itemIDs.empty() ) {
		auto insertPosition = tempList.begin();
		if ( position >= static_cast<int>( tempList.size() ) ) {
			insertPosition = tempList.end();
		} else if ( position > 0 ) {
			std::advance( insertPosition, position );
		}

		auto itemsToMove = itemIDs;
		for ( auto playlistIter = tempList.begin(); ( playlistIter != tempList.end() ) && !itemsToMove.empty(); ) {
			if ( playlistIter->ID == itemsToMove.front() ) {
				if ( playlistIter == insertPosition ) {
					++playlistIter;
					++insertPosition;
				} else {
					const Item item = *playlistIter;
					playlistIter = tempList.erase( playlistIter );
					changed = ( playlistIter != insertPosition );
					tempList.insert( insertPosition, item );
				}
				itemsToMove.pop_front();
			} else {
				++playlistIter;
			}
		}

	}
	if ( changed ) {
		m_SortColumn = Column::_Undefined;
		m_SortAscending = false;
		m_Playlist = { tempList.begin(), tempList.end() };
		m_ItemIDPositions.clear();
		size_t i = 0;
		for ( const auto& item : m_Playlist ) {
			m_ItemIDPositions[ item.ID ] = i++;
		}
	}
	return changed;
}

Playlist::Type Playlist::GetType() const
{
	return m_Type;
}

bool Playlist::ContainsFile( const std::wstring& filename, const std::optional<long>& cueStart, const std::optional<long>& cueEnd )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	bool containsFilename = false;
	auto iter = m_Playlist.begin();
	const auto itemToFind = std::tie( filename, cueStart, cueEnd );
	while ( !containsFilename && ( m_Playlist.end() != iter ) ) {
		containsFilename = ( itemToFind == std::tie( iter->Info.GetFilename(), iter->Info.GetCueStart(), iter->Info.GetCueEnd() ) );
		++iter;
	}
	return containsFilename;
}

void Playlist::SetMergeDuplicates( const bool merge )
{
	if ( merge != m_MergeDuplicates ) {
		switch ( GetType() ) {
			case Type::Album:
			case Type::Artist:
			case Type::Publisher:
			case Type::Composer:
			case Type::Conductor:
			case Type::Genre:
			case Type::Year: {
				m_MergeDuplicates = merge;
				if ( m_MergeDuplicates ) {
					MergeDuplicates();
				} else {
					SplitDuplicates();
				}
				break;
			}
		}
	}
}

void Playlist::MergeDuplicates()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	VUPlayer* vuplayer = VUPlayer::Get();
	Items itemsRemoved;
	auto firstItem = m_Playlist.begin();
	while ( m_Playlist.end() != firstItem ) {
		if ( !firstItem->Info.GetCueStart() ) {
			auto secondItem = firstItem;
			++secondItem;
			bool itemModified = false;
			while ( m_Playlist.end() != secondItem ) {
				if ( firstItem->Info.IsDuplicate( secondItem->Info ) ) {
					itemsRemoved.push_back( *secondItem );
					const auto foundDuplicate = std::find( firstItem->Duplicates.begin(), firstItem->Duplicates.end(), secondItem->Info.GetFilename() );
					if ( firstItem->Duplicates.end() == foundDuplicate ) {
						firstItem->Duplicates.insert( secondItem->Info.GetFilename() );
					}
					secondItem = m_Playlist.erase( secondItem );
					itemModified = true;
				} else {
					++secondItem;
				}
			}
			if ( itemModified && ( nullptr != vuplayer ) ) {
				vuplayer->OnPlaylistItemUpdated( this, *firstItem );
			}
		}
		++firstItem;
	}
	if ( nullptr != vuplayer ) {
		for ( const auto& item : itemsRemoved ) {
			vuplayer->OnPlaylistItemRemoved( this, item );
		}
	}
	m_ItemIDPositions.clear();
	size_t i = 0;
	for ( const auto& item : m_Playlist ) {
		m_ItemIDPositions[ item.ID ] = i++;
	}
}

void Playlist::SplitDuplicates()
{
	VUPlayer* vuplayer = VUPlayer::Get();
	std::set<MediaInfo> itemsToAdd;
	{
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		for ( auto& item : m_Playlist ) {
			bool itemModified = false;
			for ( const auto& duplicate : item.Duplicates ) {
				MediaInfo mediaInfo( item.Info );
				mediaInfo.SetFilename( duplicate );
				itemsToAdd.insert( mediaInfo );
				itemModified = true;
			}
			item.Duplicates.clear();
			if ( itemModified && ( nullptr != vuplayer ) ) {
				vuplayer->OnPlaylistItemUpdated( this, item );
			}
		}
	}
	for ( const auto& mediaInfo : itemsToAdd ) {
		int position = 0;
		bool addedAsDuplicate = false;
		const Item item = AddItem( mediaInfo, position, addedAsDuplicate );
		if ( nullptr != vuplayer ) {
			if ( addedAsDuplicate ) {
				vuplayer->OnPlaylistItemUpdated( this, item );
			} else {
				vuplayer->OnPlaylistItemAdded( this, item, position );
			}
		}
	}
}

void Playlist::CloseHandles()
{
	if ( nullptr != m_PendingStopEvent ) {
		CloseHandle( m_PendingStopEvent );
		m_PendingStopEvent = nullptr;
	}
	if ( nullptr != m_PendingWakeEvent ) {
		CloseHandle( m_PendingWakeEvent );
		m_PendingWakeEvent = nullptr;
	}
	if ( nullptr != m_PendingThread ) {
		CloseHandle( m_PendingThread );
		m_PendingThread = nullptr;
	}
}

void Playlist::UpdateItem( const Item& item )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	auto foundItem = std::find_if( m_Playlist.begin(), m_Playlist.end(), [ item ] ( const Item& entry )
		{
			return ( item.ID == entry.ID );
		} );
	if ( m_Playlist.end() != foundItem ) {
		*foundItem = item;
	}
}

bool Playlist::AddPlaylist( const std::wstring& filename, const bool startPendingThread )
{
	bool added = false;
	const std::wstring fileExt = GetFileExtension( filename );
	if ( L"vpl" == fileExt ) {
		added = AddVPL( filename );
	} else if ( ( L"m3u" == fileExt ) || ( L"m3u8" == fileExt ) ) {
		added = AddM3U( filename );
	} else if ( L"pls" == fileExt ) {
		added = AddPLS( filename );
	} else if ( L"cue" == fileExt ) {
		added = AddCUE( filename );
	}
	if ( added && startPendingThread ) {
		StartPendingThread();
	}
	return added;
}

bool Playlist::AddVPL( const std::wstring& filename )
{
	bool added = false;
	std::ifstream stream;
	stream.open( filename, std::ios::binary | std::ios::in );
	if ( stream.is_open() ) {
		do {
			std::string line;
			std::getline( stream, line );
			const size_t delimiter = line.find_first_of( 0x01 );
			if ( std::string::npos != delimiter ) {
				const std::string name = line.substr( 0 /*offset*/, delimiter /*count*/ );
				AddPending( AnsiCodePageToWideString( name ), false /*startPendingThread*/ );
				added = true;
			}
		} while ( !stream.eof() );
		stream.close();
	}
	return added;
}

bool Playlist::AddM3U( const std::wstring& filename )
{
	bool added = false;
	std::ifstream stream;
	stream.open( filename, std::ios::in );
	if ( stream.good() ) {
		std::filesystem::path playlistPath( filename );
		playlistPath = playlistPath.parent_path();
		do {
			std::string line;
			std::getline( stream, line );
			if ( !line.empty() ) {
				const std::wstring filenameEntry = UTF8ToWideString( line );
				if ( ( filenameEntry.size() > 0 ) && ( '#' != filenameEntry.front() ) ) {
					if ( IsURL( filenameEntry ) ) {
						AddPending( filenameEntry, false /*startPendingThread*/ );
						added = true;
					} else {
						std::filesystem::path filePath = std::filesystem::path( filenameEntry ).lexically_normal();
						if ( filePath.is_relative() ) {
							filePath = playlistPath / filePath;
						}
						if ( std::filesystem::exists( filePath ) ) {
							AddPending( MediaInfo( filePath ), false /*startPendingThread*/ );
							added = true;
						}
					}
				}
			}
		} while ( !stream.eof() );
	}
	return added;
}

bool Playlist::AddPLS( const std::wstring& filename )
{
	bool added = false;
	std::ifstream stream;
	stream.open( filename, std::ios::in );
	if ( stream.good() ) {
		std::filesystem::path playlistPath( filename );
		playlistPath = playlistPath.parent_path();
		do {
			std::string line;
			std::getline( stream, line );
			const size_t fileEntry = line.find( "File" );
			const size_t delimiter = line.find_first_of( '=' );
			if ( ( 0 == fileEntry ) && ( std::string::npos != delimiter ) ) {
				const std::string name = line.substr( delimiter + 1 );
				if ( !name.empty() ) {
					const std::wstring filenameEntry = UTF8ToWideString( name );
					if ( IsURL( filenameEntry ) ) {
						AddPending( filenameEntry, false /*startPendingThread*/ );
						added = true;
					} else {
						std::filesystem::path filePath = std::filesystem::path( filenameEntry ).lexically_normal();
						if ( filePath.is_relative() ) {
							filePath = playlistPath / filePath;
						}
						if ( std::filesystem::exists( filePath ) ) {
							AddPending( MediaInfo( filePath ), false /*startPendingThread*/ );
							added = true;
						}
					}
				}
			}
		} while ( !stream.eof() );
	}
	return added;
}

static std::vector<std::wstring> GetMatches( const std::string& line, const std::regex& regex )
{
	std::smatch match;
	if ( std::regex_match( line, match, regex ) ) {
		std::vector<std::wstring> matches;
		matches.reserve( match.size() );
		for ( size_t i = 1; i < match.size(); i++ ) {
			matches.emplace_back( UTF8ToWideString( StripQuotes( match.str( i ) ) ) );
		}
		return matches;
	}
	return {};
}

static bool SetCueStart( const std::string& line, MediaInfo& mediaInfo )
{
	const std::regex kIndex( R"(INDEX\s+(\d+)\s+(\d+):(\d{2}):(\d{2}))" );
	std::smatch match;
	if ( std::regex_match( line, match, kIndex ) && ( 5 == match.size() ) ) {
		try {
			if ( const long index = std::stol( match[ 1 ] ); index == 1 ) {
				const long minutes = std::stol( match[ 2 ] );
				const long seconds = std::stol( match[ 3 ] );
				const long frames = std::stol( match[ 4 ] );
				mediaInfo.SetCueStart( minutes * 60 * 75 + seconds * 75 + frames );
				return true;
			}
		} catch ( const std::logic_error& ) {}
	}
	return false;
}

static std::map<long /*trackNumber*/, std::pair<bool /*isAudio*/, MediaInfo>> GetCueEntries( const std::filesystem::path& filename )
{
	std::map<long /*trackNumber*/, std::pair<bool /*isAudio*/, MediaInfo>> tracks;
	std::ifstream stream( filename );
	if ( stream.good() ) {
		std::filesystem::path playlistPath( filename );
		playlistPath = playlistPath.parent_path();

		const std::regex kPerformer( R"(PERFORMER\s+(.+))" );
		const std::regex kTitle( R"(TITLE\s+(.+))" );
		const std::regex kGenre( R"(REM\s+GENRE\s+(.+))" );
		const std::regex kComment( R"(REM\s+COMMENT\s+(.+))" );
		const std::regex kDate( R"(REM\s+DATE\s+(.+))" );
		const std::regex kFile( R"-(FILE\s*"(.+)"\s*(.+))-" );
		const std::regex kTrack( R"(TRACK\s+(\d+)\s+(.+))" );

		std::wstring albumArtist;
		std::wstring albumTitle;
		std::wstring albumGenre;
		std::wstring albumComment;
		long albumYear = 0;

		std::filesystem::path currentFile;
		bool currentFileExists = false;

		auto currentTrack = tracks.end();

		std::vector<std::wstring> matches;
		do {
			std::string line;
			std::getline( stream, line );
			line = StripWhitespace( line );

			if ( tracks.empty() ) {
				if ( matches = GetMatches( line, kPerformer ); !matches.empty() ) {
					albumArtist = matches.front();
				} else if ( matches = GetMatches( line, kTitle ); !matches.empty() ) {
					albumTitle = matches.front();
				} else if ( matches = GetMatches( line, kGenre ); !matches.empty() ) {
					albumGenre = matches.front();
				} else if ( matches = GetMatches( line, kComment ); !matches.empty() ) {
					albumComment = matches.front();
				} else if ( matches = GetMatches( line, kDate ); !matches.empty() ) {
					try {
						albumYear = std::stol( matches.front() );
					} catch ( const std::logic_error& ) {}
				}
			}

			if ( matches = GetMatches( line, kFile ); matches.size() >= 2 ) {
				if ( matches[ 1 ] == L"MOTOROLA" ) {
					// Big-endian raw data is not supported.
					currentFileExists = false;
				} else {
					currentFile = playlistPath / matches.front();
					currentFileExists = std::filesystem::exists( currentFile );
				}
			}

			if ( currentFileExists ) {
				if ( matches = GetMatches( line, kTrack ); !matches.empty() ) {
					try {
						const long track = std::stol( matches.front() );
						const bool isAudio = ( matches.size() >= 2 ) && ( matches[ 1 ] == L"AUDIO" );
						currentTrack = tracks.insert( { track, std::make_pair( isAudio, MediaInfo( currentFile ) ) } ).first;
						if ( isAudio ) {
							MediaInfo& info = currentTrack->second.second;
							info.SetTrack( track );
							info.SetArtist( albumArtist );
							info.SetAlbum( albumTitle );
							info.SetGenre( albumGenre );
							info.SetComment( albumComment );
							info.SetYear( albumYear );
						}
					} catch ( const std::logic_error& ) {
						currentTrack = tracks.end();
					}
				}

				if ( tracks.end() != currentTrack ) {
					MediaInfo& info = currentTrack->second.second;
					if ( matches = GetMatches( line, kPerformer ); !matches.empty() ) {
						info.SetArtist( matches.front() );
					} else if ( matches = GetMatches( line, kTitle ); !matches.empty() ) {
						info.SetTitle( matches.front() );
					} else {
						SetCueStart( line, info );
					}
				}
			}
		} while ( !stream.eof() );

		// Strip out any tracks without cues.
		for ( auto track = tracks.begin(); track != tracks.end(); ) {
			const auto& info = track->second.second;
			if ( info.GetCueStart() ) {
				++track;
			} else {
				track = tracks.erase( track );
			}
		}
	}
	return tracks;
}

bool Playlist::AddCUE( const std::wstring& filename )
{
	auto tracks = GetCueEntries( filename );
	MediaInfo::List addedItems;
	bool queryMusicBrainz = false;
	for ( auto track = tracks.begin(); track != tracks.end(); track++ ) {
		const bool isAudio = track->second.first;
		MediaInfo& info = track->second.second;
		if ( isAudio ) {
			if ( auto nextTrack = track; ++nextTrack != tracks.end() ) {
				const MediaInfo& nextInfo = nextTrack->second.second;
				info.SetCueEnd( nextInfo.GetCueStart() );
			}
			queryMusicBrainz |= !m_Library.GetMediaInfo( info, false /*scanMedia*/, false /*sendNotifications*/ );
			if ( m_Library.GetMediaInfo( info ) ) {
				AddItem( info );
				addedItems.push_back( info );
			}
		}
	}
	if ( !addedItems.empty() ) {
		Playlist cueList( m_Library, GetID(), GetType() );
		for ( const auto& item : addedItems ) {
			cueList.AddItem( item );
		}
		if ( VUPlayer* vuplayer = VUPlayer::Get(); ( nullptr != vuplayer ) && vuplayer->IsMusicBrainzEnabled() && queryMusicBrainz ) {
			vuplayer->OnMusicBrainzQuery( &cueList );
		}
	}
	return !addedItems.empty();
}

std::set<std::wstring> Playlist::GetSupportedPlaylistExtensions()
{
	return { s_SupportedExtensions.begin(), s_SupportedExtensions.end() };
}

bool Playlist::IsSupportedPlaylist( const std::wstring& filename )
{
	return ( s_SupportedExtensions.end() != std::find( s_SupportedExtensions.begin(), s_SupportedExtensions.end(), GetFileExtension( filename ) ) );
}

std::optional<std::set<std::filesystem::path>> Playlist::ExtractCueFiles( std::set<std::filesystem::path>& originalFilenames )
{
	std::set<std::filesystem::path> cueFiles;
	for ( auto filename = originalFilenames.begin(); originalFilenames.end() != filename; ) {
		if ( const auto extension = WideStringToLower( filename->extension() ); L".cue" == extension ) {
			cueFiles.insert( *filename );
			filename = originalFilenames.erase( filename );
		} else {
			++filename;
		}
	}

	if ( !cueFiles.empty() ) {
		// Map a copy of the original file names, to allow for case insensitive comparisons.
		std::map<std::wstring, std::filesystem::path> filenames;
		for ( const auto& originalFilename : originalFilenames ) {
			filenames.insert( { WideStringToLower( originalFilename ), originalFilename } );
		}
		for ( const auto& cueFile : cueFiles ) {
			const auto cueEntries = GetCueEntries( cueFile );
			for ( const auto& [tracknumber, entry] : cueEntries ) {
				const auto cueFilename = WideStringToLower( entry.second.GetFilename() );
				if ( const auto match = filenames.find( cueFilename ); filenames.end() != match ) {
					const auto& originalFilename = match->second;
					originalFilenames.erase( originalFilename );
					filenames.erase( match );
				}
			}
		}
	}

	return cueFiles.empty() ? std::nullopt : std::make_optional( cueFiles );
}

bool Playlist::CanConvertAnyItems()
{
	bool canConvert = false;
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	for ( auto item = m_Playlist.begin(); !canConvert && ( m_Playlist.end() != item ); ++item ) {
		canConvert = !IsURL( item->Info.GetFilename() );
	}
	return canConvert;
}

bool Playlist::ContainsItem( const Item& item )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	const auto foundItem = std::find_if( m_Playlist.begin(), m_Playlist.end(), [ id = item.ID ] ( const Item& item ) { return id == item.ID; } );
	return m_Playlist.end() != foundItem;
}

int Playlist::FindItem( const int startIndex, const std::wstring& searchTitle, const bool partial, const bool wrap )
{
	if ( searchTitle.empty() ) {
		return -1;
	}

	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	const int start = ( static_cast<size_t>( startIndex ) < m_Playlist.size() ) ? startIndex : 0;
	auto foundItem = std::find_if( m_Playlist.begin() + start, m_Playlist.end(), [ &searchTitle, partial ] ( const Item& item ) {
		const auto itemTitle = item.Info.GetTitle( true );
		if ( partial ) {
			return 0 == _wcsnicmp( searchTitle.c_str(), itemTitle.c_str(), searchTitle.size() );
		} else {
			return ( 0 == _wcsicmp( searchTitle.c_str(), itemTitle.c_str() ) );
		}
		} );

	if ( ( m_Playlist.end() == foundItem ) && wrap ) {
		foundItem = std::find_if( m_Playlist.begin(), m_Playlist.begin() + start, [ &searchTitle, partial ] ( const Item& item ) {
			const auto itemTitle = item.Info.GetTitle( true );
			if ( partial ) {
				return 0 == _wcsnicmp( searchTitle.c_str(), itemTitle.c_str(), searchTitle.size() );
			} else {
				return ( 0 == _wcsicmp( searchTitle.c_str(), itemTitle.c_str() ) );
			}
			} );
	}

	if ( m_Playlist.end() != foundItem ) {
		return static_cast<int>( std::distance( m_Playlist.begin(), foundItem ) );
	}

	return -1;
}

bool Playlist::AllowMusicBrainzQueries()
{
	if ( Type::CDDA == m_Type )
		return true;

	return CDDAMedia::GetMusicBrainzPlaylistID( this ).has_value();
}

bool Playlist::UpdateOrAddItem( const MediaInfo& mediaInfo )
{
	bool itemFound = false;
	std::optional<Item> itemToUpdate;
	{
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		const auto itemToFind = std::tie( mediaInfo.GetFilename(), mediaInfo.GetCueStart(), mediaInfo.GetCueEnd() );
		for ( auto& item : m_Playlist ) {
			if ( itemToFind == std::tie( item.Info.GetFilename(), item.Info.GetCueStart(), item.Info.GetCueEnd() ) ) {
				itemFound = true;
				if ( item.Info != mediaInfo ) {
					itemToUpdate = item;
				}
				break;
			}
		}
	}
	if ( itemFound ) {
		if ( itemToUpdate ) {
			OnUpdatedMedia( mediaInfo );
		}
	} else {
		AddItem( mediaInfo );
	}
	return ( itemFound && itemToUpdate );
}
