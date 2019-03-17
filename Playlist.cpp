#include "Playlist.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <fstream>

// Next available playlist item ID.
long Playlist::s_NextItemID = 0;

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
	m_SortColumn( Column::_Undefined ),
	m_SortAscending( false ),
	m_Type( type ),
	m_MergeDuplicates( false )
{
}

Playlist::Playlist( Library& library, const Type& type ) :
	Playlist( library, GenerateGUIDString(), type )
{
}

Playlist::Playlist( Library& library, const Type& type, const bool mergeDuplicates ) :
	Playlist( library, type )
{
	m_MergeDuplicates = mergeDuplicates;
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

Playlist::ItemList Playlist::GetItems()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	return m_Playlist;
}

std::list<std::wstring> Playlist::GetPending()
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

	bool success = false;
	position = 0;
	for ( const auto& iter : m_Playlist ) {
		if ( iter.ID == item.ID ) {
			item = iter;
			success = true;
			break;
		} else {
			++position;
		}
	}
	return success;
}

bool Playlist::GetNextItem( const Item& currentItem, Item& nextItem, const bool wrap )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );

	bool success = false;
	const long currentID = currentItem.ID;
	auto iter = m_Playlist.begin();
	while ( iter != m_Playlist.end() ) {
		const long id = iter->ID;
		++iter;
		if ( id == currentID ) {
			if ( iter == m_Playlist.end() ) {
				if ( wrap ) {
					nextItem = m_Playlist.front();
					success = true;
				}
			} else {
				nextItem = *iter;
				success = true;
			}
			break;
		}
	}
	return success;
}

bool Playlist::GetPreviousItem( const Item& currentItem, Item& previousItem, const bool wrap )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );

	bool success = false;
	const long currentID = currentItem.ID;
	auto iter = m_Playlist.rbegin();
	while ( iter != m_Playlist.rend() ) {
		const long id = iter->ID;
		++iter;
		if ( id == currentID ) {
			if ( iter == m_Playlist.rend() ) {
				if ( wrap ) {
					previousItem = m_Playlist.back();
					success = true;
				}
			} else {
				previousItem = *iter;
				success = true;
			}
			break;
		}
	}
	return success;
}

Playlist::Item Playlist::GetRandomItem()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	Item item;
	if ( !m_Playlist.empty() ) {
		const long long itemPosition = GetRandomNumber( 0 /*minimum*/, m_Playlist.size() -1 /*maximum*/ );
		auto iter = m_Playlist.begin();
		std::advance( iter, itemPosition );
		item = *iter;
	}
	return item;
}

Playlist::Item Playlist::AddItem( const MediaInfo& mediaInfo )
{
	int position = 0;
	bool addedAsDuplicate = false;
	const Playlist::Item item = AddItem( mediaInfo, position, addedAsDuplicate );
	return item;
}

Playlist::Item Playlist::AddItem( const MediaInfo& mediaInfo, int& position, bool& addedAsDuplicate )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );

	Item item = {};
	position = 0;
	addedAsDuplicate = false;

	if ( m_MergeDuplicates ) {	
		for ( auto& itemIter : m_Playlist ) {
			if ( itemIter.Info.IsDuplicate( mediaInfo ) ) {
				if ( itemIter.Info.GetFilename() != mediaInfo.GetFilename() ) {
					const auto foundDuplicate = std::find( itemIter.Duplicates.begin(), itemIter.Duplicates.end(), mediaInfo.GetFilename() );
					if ( itemIter.Duplicates.end() == foundDuplicate ) {
						itemIter.Duplicates.push_back( mediaInfo.GetFilename() );
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
		}
	}
	return item;
}

void Playlist::AddPending( const std::wstring& filename, const bool startPendingThread )
{
	{
		std::lock_guard<std::mutex> lock( m_MutexPending );
		m_Pending.push_back( filename );
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
		std::wstring filename;
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
				filename = m_Pending.front();
				m_Pending.pop_front();
			}
		}

		if ( !filename.empty() ) {
			bool addItem = true;
			const Type type = GetType();
			if ( ( Type::All == type ) || ( Type::Favourites == type ) || ( Type::Folder == type ) ) {
				addItem = !ContainsFilename( filename );
			}
			if ( addItem ) {
				MediaInfo mediaInfo( filename );
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
	try {
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
	} catch ( ... ) {

	}
}

void Playlist::RemoveItem( const Item& item )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	
	for ( auto iter = m_Playlist.begin(); iter != m_Playlist.end(); iter++ ) {
		if ( iter->ID == item.ID ) {
			m_Playlist.erase( iter );
			VUPlayer* vuplayer = VUPlayer::Get();
			if ( nullptr != vuplayer ) {
				vuplayer->OnPlaylistItemRemoved( this, item );
			}
			break;
		}
	}
}

void Playlist::RemoveItem( const MediaInfo& mediaInfo )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	
	for ( auto iter = m_Playlist.begin(); iter != m_Playlist.end(); iter++ ) {
		if ( iter->Info.GetFilename() == mediaInfo.GetFilename() ) {
			if ( iter->Duplicates.empty() ) {
				const Item item = *iter;
				m_Playlist.erase( iter );
				VUPlayer* vuplayer = VUPlayer::Get();
				if ( nullptr != vuplayer ) {
					vuplayer->OnPlaylistItemRemoved( this, item );
				}
			} else {
				iter->Info.SetFilename( iter->Duplicates.front() );
				iter->Duplicates.pop_front();
			}
			break;
		} else if ( !iter->Duplicates.empty() ) {
			auto duplicate = std::find( iter->Duplicates.begin(), iter->Duplicates.end(), mediaInfo.GetFilename() );
			if ( iter->Duplicates.end() != duplicate ) {
				iter->Duplicates.erase( duplicate );
			}
		}
	}
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
		filesize += mediaInfo.GetFilesize();
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
		m_Playlist.sort( [ = ] ( const Item& item1, const Item& item2 ) -> bool
		{
			return m_SortAscending ? LessThan( item1, item2, m_SortColumn ) : GreaterThan( item1, item2, m_SortColumn );
		} );
	}
}

bool Playlist::LessThan( const Item& item1, const Item& item2, const Column column )
{
	bool lessThan = false;
	switch ( column ) {
		case Column::Album : {
			lessThan = _wcsicmp( item1.Info.GetAlbum().c_str(), item2.Info.GetAlbum().c_str() ) < 0;
			break;
		}
		case Column::Artist : {
			lessThan = _wcsicmp( item1.Info.GetArtist().c_str(), item2.Info.GetArtist().c_str() ) < 0;
			break;
		}
		case Column::Bitrate : {
			lessThan = item1.Info.GetBitrate() < item2.Info.GetBitrate();
			break;
		}
		case Column::BitsPerSample : {
			lessThan = item1.Info.GetBitsPerSample() < item2.Info.GetBitsPerSample();
			break;
		}
		case Column::Channels : {
			lessThan = item1.Info.GetChannels() < item2.Info.GetChannels();
			break;
		}
		case Column::Duration : {
			lessThan = item1.Info.GetDuration() < item2.Info.GetDuration();
			break;
		}
		case Column::Filename : {
			lessThan = _wcsicmp( item1.Info.GetFilename().c_str(), item2.Info.GetFilename().c_str() ) < 0;
			break;
		}
		case Column::Filesize : {
			lessThan = item1.Info.GetFilesize() < item2.Info.GetFilesize();
			break;
		}
		case Column::Filetime : {
			lessThan = item1.Info.GetFiletime() < item2.Info.GetFiletime();
			break;
		}
		case Column::GainAlbum : {
			lessThan = item1.Info.GetGainAlbum() < item2.Info.GetGainAlbum();
			break;
		}
		case Column::GainTrack : {
			lessThan = item1.Info.GetGainTrack() < item2.Info.GetGainTrack();
			break;
		}
		case Column::Genre : {
			lessThan = _wcsicmp( item1.Info.GetGenre().c_str(), item2.Info.GetGenre().c_str() ) < 0;
			break;
		}
		case Column::SampleRate : {
			lessThan = item1.Info.GetSampleRate() < item2.Info.GetSampleRate();
			break;
		}
		case Column::Title : {
			lessThan = _wcsicmp( item1.Info.GetTitle().c_str(), item2.Info.GetTitle().c_str() ) < 0;
			break;
		}
		case Column::Track : {
			lessThan = item1.Info.GetTrack() < item2.Info.GetTrack();
			break;
		}
		case Column::Type : {
			lessThan = _wcsicmp( item1.Info.GetType().c_str(), item2.Info.GetType().c_str() ) < 0;
			break;
		}
		case Column::Version : {
			lessThan = _wcsicmp( item1.Info.GetVersion().c_str(), item2.Info.GetVersion().c_str() ) < 0;
			break;
		}
		case Column::Year : {
			lessThan = item1.Info.GetYear() < item2.Info.GetYear();
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
	ItemList itemsToRemove;
	std::set<MediaInfo> itemsToAdd;
	{
		std::lock_guard<std::mutex> lock( m_MutexPlaylist );
		for ( auto& item : m_Playlist ) {
			if ( item.Info.GetFilename() == mediaInfo.GetFilename() ) {
				item.Info = mediaInfo;
				updated = true;

				if ( m_MergeDuplicates ) {
					// Split out any duplicates from the top level item, and add them back later as new items.
					for ( const auto& duplicate : item.Duplicates ) {
						MediaInfo itemToAdd( duplicate );
						m_Library.GetMediaInfo( itemToAdd, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ );
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
			} else if ( m_MergeDuplicates ) {
				// If a duplicate of a top level item has been updated, split it out and add it back later as a new item.
				for ( auto duplicate = item.Duplicates.begin(); item.Duplicates.end() != duplicate; duplicate++ ) {
					if ( *duplicate == mediaInfo.GetFilename() ) {
						MediaInfo itemToAdd( *duplicate );
						m_Library.GetMediaInfo( itemToAdd, false /*checkFileAttributes*/, false /*scanMedia*/, false /*sendNotification*/ );
						itemsToAdd.insert( itemToAdd );
						item.Duplicates.erase( duplicate );
						if ( nullptr != vuplayer ) {
							vuplayer->OnPlaylistItemUpdated( this, item );
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
			vuplayer->OnPlaylistItemRemoved( this, itemToRemove );
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

bool Playlist::MoveItems( const int position, const std::list<long>& items )
{
	bool changed = false;
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	if ( !items.empty() ) {
		auto insertPosition = m_Playlist.begin();
		if ( position >= static_cast<int>( m_Playlist.size() ) ) {
			insertPosition = m_Playlist.end();
		} else if ( position > 0 ) {
			std::advance( insertPosition, position );
		}

		auto itemsToMove = items;
		auto itemToMove = itemsToMove.begin();
		for ( auto playlistIter = m_Playlist.begin(); ( playlistIter != m_Playlist.end() ) && ( itemToMove != itemsToMove.end() ); ) {
			if ( playlistIter->ID == *itemToMove ) {
				if ( playlistIter == insertPosition ) {
					++playlistIter;
					++insertPosition;
				} else {
					const Item item = *playlistIter;
					playlistIter = m_Playlist.erase( playlistIter );
					changed = ( playlistIter != insertPosition );			
					m_Playlist.insert( insertPosition, item );
				}
				itemsToMove.pop_front();
				itemToMove = itemsToMove.begin();
			} else {
				++playlistIter;
			}
		}

	}
	if ( changed ) {
		m_SortColumn = Column::_Undefined;
		m_SortAscending = false;
	}
	return changed;
}

Playlist::Type Playlist::GetType() const
{
	return m_Type;
}

bool Playlist::ContainsFilename( const std::wstring& filename )
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	bool containsFilename = false;
	auto iter = m_Playlist.begin();
	while ( !containsFilename && ( m_Playlist.end() != iter ) ) {
		containsFilename = ( filename == iter->Info.GetFilename() );
		++iter;
	}
	return containsFilename;
}

void Playlist::SetMergeDuplicates( const bool merge )
{
	if ( merge != m_MergeDuplicates ) {
		m_MergeDuplicates = merge;
		if ( m_MergeDuplicates ) {
			MergeDuplicates();
		} else {
			SplitDuplicates();
		}
	}
}

void Playlist::MergeDuplicates()
{
	std::lock_guard<std::mutex> lock( m_MutexPlaylist );
	VUPlayer* vuplayer = VUPlayer::Get();
	ItemList itemsRemoved;
	auto firstItem = m_Playlist.begin();
	while ( m_Playlist.end() != firstItem ) {
		auto secondItem = firstItem;
		++secondItem;
		bool itemModified = false;
		while ( m_Playlist.end() != secondItem ) {
			if ( firstItem->Info.IsDuplicate( secondItem->Info ) ) {
				itemsRemoved.push_back( *secondItem );
				const auto foundDuplicate = std::find( firstItem->Duplicates.begin(), firstItem->Duplicates.end(), secondItem->Info.GetFilename() );
				if ( firstItem->Duplicates.end() == foundDuplicate ) {
					firstItem->Duplicates.push_back( secondItem->Info.GetFilename() );
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
		++firstItem;
	}
	if ( nullptr != vuplayer ) {
		for ( const auto& item : itemsRemoved ) {
			vuplayer->OnPlaylistItemRemoved( this, item );
		}
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
