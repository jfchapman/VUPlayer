#include "WndToolbarPlaylist.h"

#include "resource.h"

WndToolbarPlaylist::WndToolbarPlaylist( HINSTANCE instance, HWND parent, Settings& settings ) :
	WndToolbar( instance, parent, ID_TOOLBAR_PLAYLIST, settings, { IDI_ADD_STREAM, IDI_ADD_FOLDER, IDI_ADD_FILES, IDI_REMOVE_FILES, IDI_DELETE_FILES } ),
  m_AllowFileDeletion( settings.GetAllowFileDeletion() )
{
	CreateButtons();
}

void WndToolbarPlaylist::Update( Output& /*output*/, const Playlist::Ptr playlist, const Playlist::Item& selectedItem )
{
	const bool addFiles = true;
  const auto playlistType = playlist ? playlist->GetType() : Playlist::Type::_Undefined;
	const bool enableRemoveFiles = ( selectedItem.ID > 0 ) && ( ( Playlist::Type::Folder == playlistType ) ? m_AllowFileDeletion : ( ( Playlist::Type::CDDA != playlist->GetType() ) && ( Playlist::Type::_Undefined != playlist->GetType() ) ) );
	SetButtonEnabled( ID_FILE_PLAYLISTADDSTREAM, addFiles );
	SetButtonEnabled( ID_FILE_PLAYLISTADDFOLDER, addFiles );
	SetButtonEnabled( ID_FILE_PLAYLISTADDFILES, addFiles );
	SetButtonEnabled( ID_FILE_PLAYLISTREMOVEFILES, enableRemoveFiles );

  const bool isDeleteFiles = ( Playlist::Type::Folder == playlist->GetType() );
	if ( IsDeleteFileShown() != isDeleteFiles ) {
		const int imageIndex = isDeleteFiles ? 4 : 3;
		SendMessage( GetWindowHandle(), TB_CHANGEBITMAP, ID_FILE_PLAYLISTREMOVEFILES, imageIndex );
	}
}

void WndToolbarPlaylist::CreateButtons()
{
	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( GetImageList() ) );
	const int buttonCount = 4;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_PLAYLISTADDSTREAM;
	buttons[ 1 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 1 ].iBitmap = 1;
	buttons[ 1 ].idCommand = ID_FILE_PLAYLISTADDFOLDER;
	buttons[ 2 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 2 ].iBitmap = 2;
	buttons[ 2 ].idCommand = ID_FILE_PLAYLISTADDFILES;
	buttons[ 3 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 3 ].iBitmap = 3;
	buttons[ 3 ].idCommand = ID_FILE_PLAYLISTREMOVEFILES;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarPlaylist::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_FILE_PLAYLISTADDSTREAM : {
			tooltip = IDS_ADD_STREAM;
			break;
		}
		case ID_FILE_PLAYLISTADDFOLDER : {
			tooltip = IDS_ADDFOLDERTOPLAYLIST_TITLE;
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			tooltip = IDS_ADD_FILES;
			break;
		}
		case ID_FILE_PLAYLISTREMOVEFILES : {
			tooltip = IsDeleteFileShown() ? IDS_DELETE_FILES : IDS_REMOVE_FILES;
			break;
		}
		default : {
			break;
		}
	}
	return tooltip;
}

bool WndToolbarPlaylist::IsDeleteFileShown() const
{
	TBBUTTONINFO info = {};
	info.cbSize = sizeof( TBBUTTONINFO );
	info.dwMask = TBIF_IMAGE;
	SendMessage( GetWindowHandle(), TB_GETBUTTONINFO, ID_FILE_PLAYLISTREMOVEFILES, reinterpret_cast<LPARAM>( &info ) );
	const bool isDeleteFile = ( 4 == info.iImage );
	return isDeleteFile;
}

void WndToolbarPlaylist::OnSettingsChanged( Settings& settings )
{
  m_AllowFileDeletion = settings.GetAllowFileDeletion();
}
