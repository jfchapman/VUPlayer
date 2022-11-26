#include "DecoderOpenMPT.h"

#include "Utility.h"

DecoderOpenMPT::DecoderOpenMPT( const std::wstring& filename ) :
	Decoder(),
  m_stream( filename, std::ios::binary ),
  m_module( m_stream )
{
  SetChannels( 2 );
  SetSampleRate( 48000 );
  SetDuration( static_cast<float>( m_module.get_duration_seconds() ) );
}

DecoderOpenMPT::~DecoderOpenMPT()
{
}

long DecoderOpenMPT::Read( float* buffer, const long sampleCount )
{
  long samplesRead = 0;
  try {
    samplesRead = static_cast<long>( m_module.read_interleaved_stereo( static_cast<size_t>( GetSampleRate() ), static_cast<size_t>( sampleCount ), buffer ) );
  } catch ( const openmpt::exception& ) {

  }
	return samplesRead;
}

float DecoderOpenMPT::Seek( const float position )
{
  float seekPosition = position;
  try {
    seekPosition = static_cast<float>( m_module.set_position_seconds( position ) );
  } catch ( const openmpt::exception& ) {
  }
  return seekPosition;
}
