#include "OutputDecoder.h"

// Pre-buffer read queue size.
constexpr size_t kQueueSize = 5;

// The (maximum) number of seconds stored in each pre-buffer slot.
constexpr float kSecondsPerSlot = 0.5f;

OutputDecoder::OutputDecoder( Decoder::Ptr decoder, const long id ) :
	m_Decoder( decoder ),
	m_ID( id ),
	m_Channels( GetOutputChannels( decoder ) )
{
  if ( m_Channels <= 0 ) {
		throw std::runtime_error( "Unable to create output decoder" );
	}
}

OutputDecoder::~OutputDecoder()
{
	StopPreBufferThread();
	if ( nullptr != m_SemaphoreInput ) {
		CloseHandle( m_SemaphoreInput );
	}
	if ( nullptr != m_SemaphoreOutput ) {
		CloseHandle( m_SemaphoreOutput );
	}
}

long OutputDecoder::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	if ( m_UsePreBuffer ) {
		while ( ( samplesRead < sampleCount ) && !m_DecoderFinished ) {
			const long readBufferSize = ( nullptr != m_ReadBuffer ) ? static_cast<long>( m_ReadBuffer->size() ) : 0;
			if ( m_ReadBufferOffset < readBufferSize ) {
				const long samplesToRead = std::min<long>( sampleCount - samplesRead, ( readBufferSize - m_ReadBufferOffset ) / m_Channels );
        const auto first = m_ReadBuffer->begin() + m_ReadBufferOffset;
        const auto last = first + samplesToRead * m_Channels;
        const auto dest = buffer + samplesRead * m_Channels;
				std::copy( first, last, dest );
				m_ReadBufferOffset += samplesToRead * m_Channels;
				samplesRead += samplesToRead;
			} else {
				WaitForSingleObject( m_SemaphoreOutput, INFINITE );
				std::lock_guard<std::mutex> lock( m_BufferMutex );
				if ( m_ReadQueue.empty() ) {
					m_DecoderFinished = true;
				} else {
					m_ReadBuffer = m_ReadQueue.front();
					m_ReadBufferOffset = 0;
					m_ReadQueue.pop();
				}
				ReleaseSemaphore( m_SemaphoreInput, 1, nullptr );
			}
		}
	} else {
		samplesRead = Decode( buffer, sampleCount );
	}
	return samplesRead;
}

double OutputDecoder::Seek( const double position )
{
	if ( m_UsePreBuffer ) {
		StopPreBufferThread();
	}
	const double result = m_Decoder->SetPosition( position );
	if ( m_UsePreBuffer ) {
		StartPreBufferThread();
	}
	return result;
}

long OutputDecoder::GetSampleRate() const
{
	return m_Decoder->GetSampleRate();
}

long OutputDecoder::GetChannels() const
{
	return m_Channels;
}

std::optional<long> OutputDecoder::GetBPS() const
{
	return m_Decoder->GetBPS();
}

std::optional<float> OutputDecoder::GetBitrate() const
{
	return m_Decoder->GetBitrate();
}

void OutputDecoder::SkipSilence()
{
	if ( m_UsePreBuffer ) {
		StopPreBufferThread();
		m_Decoder->SetPosition( 0 );
	}
	m_Decoder->SkipSilence();
	if ( m_UsePreBuffer ) {
		StartPreBufferThread();
	}
}

bool OutputDecoder::SupportsStreamTitles() const
{
	return m_Decoder->SupportsStreamTitles();
}

std::pair<float /*seconds*/, std::wstring /*title*/> OutputDecoder::GetStreamTitle()
{
	return m_Decoder->GetStreamTitle();
}

void OutputDecoder::PreBuffer( PreBufferFinishedCallback callback )
{
	if ( !m_UsePreBuffer ) {
		m_SemaphoreOutput = CreateSemaphore( nullptr /*attributes*/, 0 /*initial*/, kQueueSize /*maximum*/, L"" );
		m_SemaphoreInput = CreateSemaphore( nullptr /*attributes*/, kQueueSize /*initial*/, kQueueSize /*maximum*/, L"" );
		m_UsePreBuffer = ( nullptr != m_SemaphoreOutput ) && ( nullptr != m_SemaphoreInput );
		if ( m_UsePreBuffer ) {
			m_PreBufferFinishedCallback = callback;
			StartPreBufferThread();
		}
	}
}

void OutputDecoder::StartPreBufferThread()
{
	m_StopPreBuffering = false;
	m_DecoderFinished = false;
	m_ReadQueue = {};
	m_ReadBuffer = nullptr;
	m_ReadBufferOffset = 0;

	m_BufferThread = std::thread( [ this ] ()
		{
			// Create a 'circular' buffer containing a fixed number of write slots, which the read queue will point to.
			// This buffer needs to be two slots wider than the read queue because:
			// 1. When the writer wraps around, it will (re)acquire the initial read slot, before being held.
			// 2. When the writer is then released, it will immediately acquire the next read slot, before being held.
			constexpr size_t kSlotCount = 2 + kQueueSize;
			m_BufferSlots.resize( kSlotCount );
			size_t slotIndex = 0;

			const long bufferSamples = static_cast<long>( m_Decoder->GetSampleRate() * kSecondsPerSlot );
			bool finished = false;
			while ( !finished ) {
				auto& buffer = m_BufferSlots[ slotIndex ];
				slotIndex = ( 1 + slotIndex ) % kSlotCount;

				buffer.resize( bufferSamples * m_Channels );
				const long samplesRead = Decode( buffer.data(), bufferSamples );
				buffer.resize( samplesRead * m_Channels );

				WaitForSingleObject( m_SemaphoreInput, INFINITE );
				std::lock_guard<std::mutex> lock( m_BufferMutex );
				if ( m_StopPreBuffering ) {
					finished = true;
					m_ReadQueue = {};
				} else if ( buffer.empty() ) {
					finished = true;
					if ( m_PreBufferFinishedCallback ) {
						m_PreBufferFinishedCallback( m_ID );
					}
				} else {
					m_ReadQueue.push( &buffer );
				}
				ReleaseSemaphore( m_SemaphoreOutput, 1, nullptr );
			}
		}
	);
}

void OutputDecoder::StopPreBufferThread()
{
	if ( m_BufferThread.joinable() ) {
		m_StopPreBuffering = true;
		ReleaseSemaphore( m_SemaphoreInput, 1, nullptr );
		m_BufferThread.join();

		while ( WAIT_OBJECT_0 == WaitForSingleObject( m_SemaphoreOutput, 0 ) ) {}
		while ( WAIT_OBJECT_0 == WaitForSingleObject( m_SemaphoreInput, 0 ) ) {}
		ReleaseSemaphore( m_SemaphoreInput, kQueueSize, nullptr );
	}
}

long OutputDecoder::GetOutputChannels( const Decoder::Ptr& decoder )
{
  MediaInfo info;
  info.SetChannels( decoder ? decoder->GetChannels() : 0 );
  return GetOutputChannels( info );
}

long OutputDecoder::GetOutputChannels( const MediaInfo& mediaInfo )
{
  // Convert 6.1 files to 7.1, so that they can be downmixed correctly.
  const long channels = mediaInfo.GetChannels();
  return ( 7 == channels ) ? 8 : channels;
}

long OutputDecoder::Decode( float* buffer, const long sampleCount )
{
  const long samplesRead = m_Decoder->ReadSamples( buffer, sampleCount );
  if ( ( 7 == m_Decoder->GetChannels() ) && ( 8 == m_Channels ) ) {
    // Copy the back centre channel to the back left & back right channels.
    for ( long sample = sampleCount - 1; sample >= 0; sample-- ) {
      const long srcOffset = sample * 7;
      const long destOffset = sample * 8;
      buffer[ destOffset + 7 ] = buffer[ srcOffset + 6 ];
      buffer[ destOffset + 6 ] = buffer[ srcOffset + 5 ];
      buffer[ destOffset + 5 ] = buffer[ srcOffset + 4 ];
      buffer[ destOffset + 4 ] = buffer[ srcOffset + 4 ];
      buffer[ destOffset + 3 ] = buffer[ srcOffset + 3 ];
      buffer[ destOffset + 2 ] = buffer[ srcOffset + 2 ];
      buffer[ destOffset + 1 ] = buffer[ srcOffset + 1 ];
      buffer[ destOffset + 0 ] = buffer[ srcOffset + 0 ];
    }
  }
  return samplesRead;
}
