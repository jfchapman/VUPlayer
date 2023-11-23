#include "DecoderBin.h"

#include "Utility.h"

DecoderBin::DecoderBin( const std::wstring& filename, const Context context ) :
	Decoder( context ),
	m_Stream( filename, std::ios::binary )
{
  m_Stream.seekg( 0, std::ios::end );
  const auto filesize = m_Stream.tellg();
  m_Stream.seekg( 0, std::ios::beg );
  if ( filesize > 0 ) {
    SetBPS( 16 );
    SetChannels( 2 );
    SetSampleRate( 44100 );
    SetDuration( static_cast<float>( filesize ) / 44100 / 4 );
	} else {
		throw std::runtime_error( "DecoderBin could not load file" );
	}
}

long DecoderBin::Read( float* buffer, const long sampleCount )
{
  const long bytesToRead = sampleCount * GetChannels() * ( *GetBPS() / 8 );
  m_Buffer.resize( bytesToRead );
  m_Stream.read( m_Buffer.data(), bytesToRead );
  const auto bytesRead = m_Stream.gcount();
  const long samplesRead = static_cast<long>( bytesRead / GetChannels() / ( *GetBPS() / 8 ) );
  short* samples = reinterpret_cast<short*>( m_Buffer.data() );
  for ( long i = 0; i < samplesRead * GetChannels(); i++ ) {
    buffer[ i ] = Signed16ToFloat( samples[ i ] );
  }
	return samplesRead;
}

double DecoderBin::Seek( const double position )
{
  const size_t offset = static_cast<size_t>( position * GetSampleRate() * GetChannels() * ( *GetBPS() / 8 ) );
  m_Stream.seekg( offset - offset % ( GetChannels() * ( *GetBPS() / 8 ) ), std::ios::beg );
	return position;
}
