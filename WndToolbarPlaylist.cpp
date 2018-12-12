#include "WndToolbarPlaylist.h"

#include "resource.h"
#include "Utility.h"

// Toolbar button size.
static const int s_ButtonSize = 24;

WndToolbarPlaylist::WndToolbarPlaylist( HINSTANCE instance, HWND parent ) :
	WndToolbar( instance, parent, ID_TOOLBAR_PLAYLIST ),
	m_ImageList( nullptr ),
	m_ImageMap( {
		{ 0, IDI_ADD_FOLDER },
		{ 1, IDI_ADD_FILES },
		{ 2, IDI_REMOVE_FILES }
	} )
{
	CreateButtons();

	RECT rect = {};
	SendMessage( GetWindowHandle(), TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>( &rect ) );
	const int buttonCount = static_cast<int>( SendMessage( GetWindowHandle(), TB_BUTTONCOUNT, 0, 0 ) );
	MoveWindow( GetWindowHandle(), 0 /*x*/, 0 /*y*/, ( rect.right - rect.left ) * buttonCount, rect.bottom - rect.top, TRUE /*repaint*/ );
}

WndToolbarPlaylist::~WndToolbarPlaylist()
{
	ImageList_Destroy( m_ImageList );
}

void WndToolbarPlaylist::Update( Output& /*output*/, const Playlist::Ptr playlist, const Playlist::Item& selectedItem )
{
	const bool addFiles = true;
	const bool removeFiles = ( playlist && ( selectedItem.ID > 0 ) && ( Playlist::Type::CDDA != playlist->GetType() ) && ( Playlist::Type::Folder != playlist->GetType() ) );
	SetButtonEnabled( ID_FILE_PLAYLISTADDFOLDER, addFiles );
	SetButtonEnabled( ID_FILE_PLAYLISTADDFILES, addFiles );
	SetButtonEnabled( ID_FILE_PLAYLISTREMOVEFILES, removeFiles );
}

void WndToolbarPlaylist::CreateImageList()
{
	const float dpiScale = GetDPIScaling();
	const int cx = static_cast<int>( s_ButtonSize * dpiScale );
	const int cy = static_cast<int>( s_ButtonSize * dpiScale );
	const int imageCount = static_cast<int>( m_ImageMap.size() );
	m_ImageList = ImageList_Create( cx, cy, ILC_COLOR32, 0 /*initial*/, imageCount /*grow*/ );
	for ( const auto& iter : m_ImageMap ) {
		HICON hIcon = static_cast<HICON>( LoadImage( GetInstanceHandle(), MAKEINTRESOURCE( iter.second ), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR | LR_SHARED ) );
		if ( NULL != hIcon ) {
			ImageList_ReplaceIcon( m_ImageList, -1, hIcon );
		}
	}
}

void WndToolbarPlaylist::CreateButtons()
{
	CreateImageList();

	SendMessage( GetWindowHandle(), TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
	SendMessage( GetWindowHandle(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>( m_ImageList ) );
	const int buttonCount = 3;
	TBBUTTON buttons[ buttonCount ] = {};
	buttons[ 0 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 0 ].iBitmap = 0;
	buttons[ 0 ].idCommand = ID_FILE_PLAYLISTADDFOLDER;
	buttons[ 1 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 1 ].iBitmap = 1;
	buttons[ 1 ].idCommand = ID_FILE_PLAYLISTADDFILES;
	buttons[ 2 ].fsStyle = TBSTYLE_BUTTON;
	buttons[ 2 ].iBitmap = 2;
	buttons[ 2 ].idCommand = ID_FILE_PLAYLISTREMOVEFILES;
	SendMessage( GetWindowHandle(), TB_ADDBUTTONS, buttonCount, reinterpret_cast<LPARAM>( buttons ) );
}

UINT WndToolbarPlaylist::GetTooltip( const UINT commandID ) const
{
	UINT tooltip = 0;
	switch ( commandID ) {
		case ID_FILE_PLAYLISTADDFOLDER : {
			tooltip = IDS_ADDFOLDERTOPLAYLIST_TITLE;
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			tooltip = IDS_ADD_FILES;
			break;
		}
		case ID_FILE_PLAYLISTREMOVEFILES : {
			tooltip = IDS_REMOVE_FILES;
			break;
		}
		default : {
			break;
		}
	}
	return tooltip;
}
