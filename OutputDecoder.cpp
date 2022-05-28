#include "OutputDecoder.h"

// Pre-buffer read queue size.
constexpr size_t kQueueSize = 5;

// The (maximum) number of seconds stored in each pre-buffer slot.
constexpr float kSecondsPerSlot = 0.5f;

OutputDecoder::OutputDecoder( Decoder::Ptr decoder, const long id ) :
	m_Decoder( decoder ),
	m_ID( id ),
	m_Channels( decoder ? decoder->GetChannels() : 0 )
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
				std::copy( m_ReadBuffer->data() + m_ReadBufferOffset, m_ReadBuffer->data() + m_ReadBufferOffset + samplesToRead * m_Channels, buffer + samplesRead * m_Channels );
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
		samplesRead = m_Decoder->Read( buffer, sampleCount );
	}
	return samplesRead;
}

float OutputDecoder::Seek( const float position )
{
	if ( m_UsePreBuffer ) {
		StopPreBufferThread();
	}
	const float result = m_Decoder->Seek( position );
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
	return m_Decoder->GetChannels();
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
		m_Decoder->Seek( 0 );
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
				const long samplesRead = m_Decoder->Read( buffer.data(), bufferSamples );
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
