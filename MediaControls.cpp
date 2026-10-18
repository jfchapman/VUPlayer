#include "MediaControls.h"

#include "Library.h"
#include "Output.h"
#include "WndToolbarPlayback.h"

#include "resource.h"

#include <systemmediatransportcontrolsinterop.h>
#include <windows.storage.streams.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>

using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage::Streams;
using Microsoft::WRL::ComPtr;

namespace
{
	// Button pressed event handler interface.
	using ButtonPressedEventHandler = __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CSystemMediaTransportControlsButtonPressedEventArgs;

	// Converts 'seconds' to a WinRT time span.
	ABI::Windows::Foundation::TimeSpan ToTimeSpan( const double seconds )
	{
		return { static_cast<INT64>( seconds * 10000000 ) };
	}

	// Returns the contents of the image file 'filename'.
	std::vector<BYTE> ReadImageFile( const std::wstring& filename )
	{
		std::vector<BYTE> image;
		if ( std::ifstream stream( filename, std::ios::binary | std::ios::ate ); stream.is_open() ) {
			if ( const std::streamoff size = stream.tellg(); ( size > 0 ) && ( size < 0x10000000 ) ) {
				image.resize( static_cast<size_t>( size ) );
				stream.seekg( 0 );
				if ( !stream.read( reinterpret_cast<char*>( image.data() ), size ) ) {
					image.clear();
				}
			}
		}
		return image;
	}
}

// Media control button pressed event handler, which forwards button presses to the main application window.
// Button presses can arrive on any thread, so the handler is agile and does nothing other than post a message.
class MediaControls::ButtonPressedHandler : public ButtonPressedEventHandler, public IAgileObject
{
public:
	// 'hwnd' - main application window handle.
	ButtonPressedHandler( const HWND hwnd ) :
		m_hWnd( hwnd )
	{
	}

	virtual ~ButtonPressedHandler() = default;

	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject ) override
	{
		if ( nullptr == ppvObject ) {
			return E_POINTER;
		}
		if ( ( __uuidof( IUnknown ) == riid ) || ( __uuidof( ButtonPressedEventHandler ) == riid ) ) {
			*ppvObject = static_cast<ButtonPressedEventHandler*>( this );
		} else if ( __uuidof( IAgileObject ) == riid ) {
			*ppvObject = static_cast<IAgileObject*>( this );
		} else {
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement( &m_RefCount );
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG refCount = InterlockedDecrement( &m_RefCount );
		if ( 0 == refCount ) {
			delete this;
		}
		return refCount;
	}

	HRESULT STDMETHODCALLTYPE Invoke( ISystemMediaTransportControls* /*sender*/, ISystemMediaTransportControlsButtonPressedEventArgs* args ) override
	{
		SystemMediaTransportControlsButton pressed = SystemMediaTransportControlsButton_Play;
		if ( ( nullptr != args ) && SUCCEEDED( args->get_Button( &pressed ) ) ) {
			std::optional<Button> button;
			switch ( pressed ) {
				case SystemMediaTransportControlsButton_Play: {
					button = Button::Play;
					break;
				}
				case SystemMediaTransportControlsButton_Pause: {
					button = Button::Pause;
					break;
				}
				case SystemMediaTransportControlsButton_Stop: {
					button = Button::Stop;
					break;
				}
				case SystemMediaTransportControlsButton_Previous: {
					button = Button::Previous;
					break;
				}
				case SystemMediaTransportControlsButton_Next: {
					button = Button::Next;
					break;
				}
				default: {
					break;
				}
			}
			if ( button.has_value() ) {
				PostMessage( m_hWnd, MSG_MEDIACONTROLBUTTON, static_cast<WPARAM>( *button ), 0 );
			}
		}
		return S_OK;
	}

private:
	// Main application window handle.
	const HWND m_hWnd;

	// Reference count.
	ULONG m_RefCount = 1;
};

MediaControls::MediaControls( HWND parent ) :
	m_hWnd( parent )
{
}

MediaControls::~MediaControls()
{
	Release();
}

void MediaControls::Enable( const bool enable )
{
	if ( enable && !m_Controls ) {
		if ( !Init() ) {
			Release();
		}
	} else if ( !enable && m_Controls ) {
		Release();
	}
}

bool MediaControls::IsActive() const
{
	return static_cast<bool>( m_Controls );
}

bool MediaControls::Init()
{
	// The WinRT functions are loaded dynamically, so that the application can still run on Windows 7.
	m_CombaseModule = LoadLibraryEx( L"combase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
	if ( nullptr == m_CombaseModule ) {
		return false;
	}
	m_RoGetActivationFactory = reinterpret_cast<RoGetActivationFactoryFunc>( GetProcAddress( m_CombaseModule, "RoGetActivationFactory" ) );
	m_RoActivateInstance = reinterpret_cast<RoActivateInstanceFunc>( GetProcAddress( m_CombaseModule, "RoActivateInstance" ) );
	m_WindowsCreateString = reinterpret_cast<WindowsCreateStringFunc>( GetProcAddress( m_CombaseModule, "WindowsCreateString" ) );
	m_WindowsDeleteString = reinterpret_cast<WindowsDeleteStringFunc>( GetProcAddress( m_CombaseModule, "WindowsDeleteString" ) );
	if ( ( nullptr == m_RoGetActivationFactory ) || ( nullptr == m_RoActivateInstance ) || ( nullptr == m_WindowsCreateString ) || ( nullptr == m_WindowsDeleteString ) ) {
		return false;
	}

	// Artwork support is optional.
	m_ShcoreModule = LoadLibraryEx( L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
	if ( nullptr != m_ShcoreModule ) {
		m_CreateRandomAccessStreamOverStream = reinterpret_cast<CreateRandomAccessStreamOverStreamFunc>( GetProcAddress( m_ShcoreModule, "CreateRandomAccessStreamOverStream" ) );
	}

	// Get the media controls for the main application window.
	if ( HSTRING className = CreateHString( RuntimeClass_Windows_Media_SystemMediaTransportControls ); nullptr != className ) {
		ComPtr<ISystemMediaTransportControlsInterop> interop;
		if ( SUCCEEDED( m_RoGetActivationFactory( className, IID_PPV_ARGS( &interop ) ) ) ) {
			interop->GetForWindow( m_hWnd, IID_PPV_ARGS( &m_Controls ) );
		}
		m_WindowsDeleteString( className );
	}
	if ( !m_Controls ) {
		return false;
	}

	ComPtr<ButtonPressedEventHandler> handler;
	handler.Attach( new ButtonPressedHandler( m_hWnd ) );
	if ( FAILED( m_Controls->add_ButtonPressed( handler.Get(), &m_ButtonPressedToken ) ) ) {
		m_ButtonPressedToken = {};
		return false;
	}

	// Timeline support is optional.
	if ( HSTRING className = CreateHString( RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties ); nullptr != className ) {
		ComPtr<IInspectable> inspectable;
		if ( SUCCEEDED( m_RoActivateInstance( className, &inspectable ) ) ) {
			inspectable.As( &m_TimelineProperties );
		}
		m_WindowsDeleteString( className );
	}

	m_Controls->put_IsPlayEnabled( false );
	m_Controls->put_IsPauseEnabled( false );
	m_Controls->put_IsStopEnabled( false );
	m_Controls->put_IsPreviousEnabled( false );
	m_Controls->put_IsNextEnabled( false );
	m_Controls->put_PlaybackStatus( MediaPlaybackStatus_Closed );
	m_Controls->put_IsEnabled( true );
	return true;
}

void MediaControls::Release()
{
	if ( m_Controls ) {
		if ( 0 != m_ButtonPressedToken.value ) {
			m_Controls->remove_ButtonPressed( m_ButtonPressedToken );
		}
		ComPtr<ISystemMediaTransportControlsDisplayUpdater> displayUpdater;
		if ( SUCCEEDED( m_Controls->get_DisplayUpdater( &displayUpdater ) ) ) {
			displayUpdater->ClearAll();
			displayUpdater->Update();
		}
		m_Controls->put_PlaybackStatus( MediaPlaybackStatus_Closed );
		m_Controls->put_IsEnabled( false );
	}
	m_ButtonPressedToken = {};
	m_TimelineProperties.Reset();
	m_Controls.Reset();

	m_RoGetActivationFactory = nullptr;
	m_RoActivateInstance = nullptr;
	m_WindowsCreateString = nullptr;
	m_WindowsDeleteString = nullptr;
	m_CreateRandomAccessStreamOverStream = nullptr;
	if ( nullptr != m_ShcoreModule ) {
		FreeLibrary( m_ShcoreModule );
		m_ShcoreModule = nullptr;
	}
	if ( nullptr != m_CombaseModule ) {
		FreeLibrary( m_CombaseModule );
		m_CombaseModule = nullptr;
	}

	m_PlaybackStatus = MediaPlaybackStatus_Closed;
	m_PlayEnabled = false;
	m_StopEnabled = false;
	m_PreviousEnabled = false;
	m_NextEnabled = false;
	m_DisplayItemID = -1;
	m_DisplayTitle.clear();
	m_DisplayArtist.clear();
	m_DisplayAlbum.clear();
	m_DisplayArtworkID.clear();
	m_TimelineDuration = -1;
	m_TimelinePosition = -1;
}

void MediaControls::Update( WndToolbarPlayback& toolbarPlayback, Output& output, Library& library, const Playlist::Ptr& playlist )
{
	if ( m_Controls ) {
		// When stopped, display the selected item (which would be played by the play button).
		// If there is no selected item, display the playlist name (the play button would play the first, or a random, playlist item).
		const Output::State state = output.GetState();
		const Output::Item playingItem = ( Output::State::Stopped != state ) ? output.GetCurrentPlaying() : Output::Item();
		Output::Item displayItem = playingItem;
		std::wstring playlistName;
		if ( 0 == displayItem.PlaylistItem.ID ) {
			displayItem.PlaylistItem = output.GetCurrentSelectedPlaylistItem();
			if ( ( 0 == displayItem.PlaylistItem.ID ) && playlist && ( playlist->GetCount() > 0 ) ) {
				playlistName = playlist->GetName();
			}
		}

		UpdateStatus( toolbarPlayback, state, ( displayItem.PlaylistItem.ID > 0 ) || !playlistName.empty() );
		UpdateDisplay( displayItem, playlistName, library );
		UpdateTimeline( playingItem );
	}
}

void MediaControls::UpdateStatus( WndToolbarPlayback& toolbarPlayback, const Output::State state, const bool hasDisplayItem )
{
	// When there is nothing to display, the media controls are closed (otherwise Windows would display an empty media session).
	MediaPlaybackStatus status = hasDisplayItem ? MediaPlaybackStatus_Stopped : MediaPlaybackStatus_Closed;
	switch ( state ) {
		case Output::State::Playing: {
			status = MediaPlaybackStatus_Playing;
			break;
		}
		case Output::State::Paused: {
			status = MediaPlaybackStatus_Paused;
			break;
		}
		default: {
			break;
		}
	}
	if ( status != m_PlaybackStatus ) {
		m_PlaybackStatus = status;
		m_Controls->put_PlaybackStatus( m_PlaybackStatus );
	}

	if ( const bool playEnabled = toolbarPlayback.IsButtonEnabled( ID_CONTROL_PLAY ); playEnabled != m_PlayEnabled ) {
		m_PlayEnabled = playEnabled;
		m_Controls->put_IsPlayEnabled( m_PlayEnabled );
		m_Controls->put_IsPauseEnabled( m_PlayEnabled );
	}
	if ( const bool stopEnabled = ( Output::State::Stopped != state ); stopEnabled != m_StopEnabled ) {
		m_StopEnabled = stopEnabled;
		m_Controls->put_IsStopEnabled( m_StopEnabled );
	}
	if ( const bool previousEnabled = toolbarPlayback.IsButtonEnabled( ID_CONTROL_PREVIOUSTRACK ); previousEnabled != m_PreviousEnabled ) {
		m_PreviousEnabled = previousEnabled;
		m_Controls->put_IsPreviousEnabled( m_PreviousEnabled );
	}
	if ( const bool nextEnabled = toolbarPlayback.IsButtonEnabled( ID_CONTROL_NEXTTRACK ); nextEnabled != m_NextEnabled ) {
		m_NextEnabled = nextEnabled;
		m_Controls->put_IsNextEnabled( m_NextEnabled );
	}
}

void MediaControls::UpdateDisplay( const Output::Item& item, const std::wstring& playlistName, Library& library )
{
	const MediaInfo& mediaInfo = item.PlaylistItem.Info;
	const long itemID = item.PlaylistItem.ID;

	std::wstring title;
	std::wstring artist;
	std::wstring album;
	std::wstring artworkID;
	if ( itemID > 0 ) {
		if ( !item.StreamTitle.empty() ) {
			title = item.StreamTitle;
			artist = mediaInfo.GetTitle( true /*filenameAsTitle*/ );
		} else {
			title = mediaInfo.GetTitle( true /*filenameAsTitle*/ );
			artist = mediaInfo.GetArtist();
			album = mediaInfo.GetAlbum();
		}
		artworkID = mediaInfo.GetArtworkID( true /*checkFolder*/ );
	} else {
		title = playlistName;
	}

	const bool itemChanged = ( itemID != m_DisplayItemID );
	const bool textChanged = ( title != m_DisplayTitle ) || ( artist != m_DisplayArtist ) || ( album != m_DisplayAlbum );
	const bool artworkChanged = ( artworkID != m_DisplayArtworkID );
	if ( !itemChanged && !textChanged && !artworkChanged ) {
		return;
	}

	ComPtr<ISystemMediaTransportControlsDisplayUpdater> displayUpdater;
	if ( FAILED( m_Controls->get_DisplayUpdater( &displayUpdater ) ) ) {
		return;
	}

	m_DisplayItemID = itemID;
	m_DisplayTitle = title;
	m_DisplayArtist = artist;
	m_DisplayAlbum = album;
	m_DisplayArtworkID = artworkID;

	if ( ( itemID <= 0 ) && title.empty() ) {
		displayUpdater->ClearAll();
		displayUpdater->Update();
		return;
	}

	displayUpdater->put_Type( MediaPlaybackType_Music );
	ComPtr<IMusicDisplayProperties> musicProperties;
	if ( SUCCEEDED( displayUpdater->get_MusicProperties( &musicProperties ) ) ) {
		// Note that a null HSTRING represents an empty string.
		HSTRING str = CreateHString( title );
		musicProperties->put_Title( str );
		m_WindowsDeleteString( str );

		str = CreateHString( artist );
		musicProperties->put_Artist( str );
		m_WindowsDeleteString( str );

		ComPtr<IMusicDisplayProperties2> musicProperties2;
		if ( SUCCEEDED( musicProperties.As( &musicProperties2 ) ) ) {
			str = CreateHString( album );
			musicProperties2->put_AlbumTitle( str );
			m_WindowsDeleteString( str );
		}
	}

	if ( artworkChanged ) {
		std::vector<BYTE> artwork = library.GetMediaArtwork( mediaInfo );
		if ( artwork.empty() && !artworkID.empty() && ( artworkID != mediaInfo.GetArtworkID( false /*checkFolder*/ ) ) ) {
			// Folder artwork.
			artwork = ReadImageFile( artworkID );
		}
		SetThumbnail( displayUpdater.Get(), artwork );
	}

	displayUpdater->Update();
}

void MediaControls::UpdateTimeline( const Output::Item& item )
{
	if ( !m_TimelineProperties ) {
		return;
	}
	ComPtr<ISystemMediaTransportControls2> controls2;
	if ( FAILED( m_Controls.As( &controls2 ) ) ) {
		return;
	}

	const double duration = ( item.PlaylistItem.ID > 0 ) ? item.PlaylistItem.Info.GetDuration() : 0;
	const long long durationSeconds = ( duration > 0 ) ? std::llround( duration ) : -1;
	const long long positionSeconds = ( durationSeconds > 0 ) ? std::clamp( std::llround( item.Position ), 0ll, durationSeconds ) : -1;
	if ( ( durationSeconds == m_TimelineDuration ) && ( positionSeconds == m_TimelinePosition ) ) {
		return;
	}
	m_TimelineDuration = durationSeconds;
	m_TimelinePosition = positionSeconds;

	// Seeking is not supported, so the seek range is empty.
	const double end = ( durationSeconds > 0 ) ? duration : 0;
	const double position = ( durationSeconds > 0 ) ? std::clamp( item.Position, 0.0, duration ) : 0;
	m_TimelineProperties->put_StartTime( ToTimeSpan( 0 ) );
	m_TimelineProperties->put_EndTime( ToTimeSpan( end ) );
	m_TimelineProperties->put_MinSeekTime( ToTimeSpan( 0 ) );
	m_TimelineProperties->put_MaxSeekTime( ToTimeSpan( 0 ) );
	m_TimelineProperties->put_Position( ToTimeSpan( position ) );
	controls2->UpdateTimelineProperties( m_TimelineProperties.Get() );
}

void MediaControls::SetThumbnail( ISystemMediaTransportControlsDisplayUpdater* displayUpdater, const std::vector<BYTE>& artwork )
{
	ComPtr<IRandomAccessStreamReference> reference;
	if ( !artwork.empty() && ( nullptr != m_CreateRandomAccessStreamOverStream ) ) {
		ComPtr<IStream> stream;
		if ( SUCCEEDED( CreateStreamOnHGlobal( nullptr, TRUE /*deleteOnRelease*/, &stream ) ) &&
				SUCCEEDED( stream->Write( artwork.data(), static_cast<ULONG>( artwork.size() ), nullptr ) ) &&
				SUCCEEDED( stream->Seek( {}, STREAM_SEEK_SET, nullptr ) ) ) {
			ComPtr<IRandomAccessStream> randomAccessStream;
			constexpr UINT BSOS_DEFAULT = 0;
			if ( SUCCEEDED( m_CreateRandomAccessStreamOverStream( stream.Get(), BSOS_DEFAULT, IID_PPV_ARGS( &randomAccessStream ) ) ) ) {
				if ( HSTRING className = CreateHString( RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference ); nullptr != className ) {
					ComPtr<IRandomAccessStreamReferenceStatics> statics;
					if ( SUCCEEDED( m_RoGetActivationFactory( className, IID_PPV_ARGS( &statics ) ) ) ) {
						statics->CreateFromStream( randomAccessStream.Get(), &reference );
					}
					m_WindowsDeleteString( className );
				}
			}
		}
	}
	displayUpdater->put_Thumbnail( reference.Get() );
}

HSTRING MediaControls::CreateHString( const std::wstring& str ) const
{
	HSTRING hstring = nullptr;
	if ( ( nullptr == m_WindowsCreateString ) || FAILED( m_WindowsCreateString( str.c_str(), static_cast<UINT32>( str.size() ), &hstring ) ) ) {
		hstring = nullptr;
	}
	return hstring;
}
