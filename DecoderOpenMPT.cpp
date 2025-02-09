// Due to GitHub file size limits, exclude libopenmpt from debug builds.
#ifndef _DEBUG

#include "DecoderOpenMPT.h"

#include "Utility.h"
#include "VUPlayer.h"

#include <numeric>
#include <queue>

DecoderOpenMPT::DecoderOpenMPT( const std::wstring& filename, const Context context ) :
	Decoder( context ),
	m_filename( filename ),
	m_stream( filename, std::ios::binary ),
	m_module( m_stream )
{
	VUPlayer* vuplayer = VUPlayer::Get();
	const uint32_t samplerate = ( nullptr != vuplayer ) ? vuplayer->GetApplicationSettings().GetMODSamplerate() : 48000;
	if ( nullptr != vuplayer ) {
		bool fadeout = false;
		long separation = 100;
		long ramping = -1;
		long interpolation = 0;
		vuplayer->GetApplicationSettings().GetOpenMPTSettings( fadeout, separation, ramping, interpolation );
		m_fadeout = fadeout;
		m_module.set_render_param( openmpt::module::RENDER_STEREOSEPARATION_PERCENT, static_cast<int32_t>( separation ) );
		m_module.set_render_param( openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, static_cast<int32_t>( ramping ) );
		m_module.set_render_param( openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, static_cast<int32_t>( interpolation ) );
	}
	SetChannels( 2 );
	SetSampleRate( samplerate );
	SetDuration( static_cast<float>( m_module.get_duration_seconds() ) );

	if ( m_fadeout ) {
		if ( Decoder::Context::Output == context ) {
			StartLoopDetectionThread();
		} else if ( Decoder::Context::Input == context ) {
			CalculateIsLooped();
		}
	}
}

DecoderOpenMPT::~DecoderOpenMPT()
{
	StopLoopDetectionThread();
}

long DecoderOpenMPT::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	try {
		constexpr double kFadeSeconds = 15.0;

		const double previousPosition = m_module.get_position_seconds();
		const bool applyFade = m_looped && ( previousPosition > m_module.get_duration_seconds() );
		if ( applyFade && ( ( previousPosition - m_module.get_duration_seconds() ) >= kFadeSeconds ) ) {
			samplesRead = 0;
		} else {
			samplesRead = static_cast<long>( m_module.read_interleaved_stereo( static_cast<size_t>( GetSampleRate() ), static_cast<size_t>( sampleCount ), buffer ) );
		}
		if ( applyFade ) {
			for ( long sampleIndex = 0; sampleIndex < samplesRead; sampleIndex++ ) {
				const double position = previousPosition + static_cast<double>( sampleIndex ) / GetSampleRate();
				if ( position - m_module.get_duration_seconds() > kFadeSeconds ) {
					samplesRead = sampleIndex;
					break;
				} else {
					const float scale = static_cast<float>( 1.0 - ( position - m_module.get_duration_seconds() ) / kFadeSeconds );
					for ( long channel = 0; channel < GetChannels(); channel++ ) {
						buffer[ sampleIndex * GetChannels() + channel ] *= scale;
					}
				}
			}
		}
	} catch ( const openmpt::exception& ) {
	}
	return samplesRead;
}

double DecoderOpenMPT::Seek( const double position )
{
	double seekPosition = position;
	try {
		seekPosition = m_module.set_position_seconds( position );
	} catch ( const openmpt::exception& ) {
	}
	return seekPosition;
}

void DecoderOpenMPT::StartLoopDetectionThread()
{
	m_threadLoopDetection = std::thread( [ this ] () {
		CalculateIsLooped();
		} );
}

void DecoderOpenMPT::StopLoopDetectionThread()
{
	if ( m_threadLoopDetection.joinable() ) {
		m_stopLoopDetection = true;
		m_threadLoopDetection.join();
	}
}

void DecoderOpenMPT::CalculateIsLooped()
{
	// If the 'end' of the song is not 'silent', then assume it is looped.
	constexpr float kEndSeconds = 0.1f;
	constexpr float kSilenceThreshold = 0.01f;

	try {
		std::ifstream stream( m_filename, std::ios::binary );
		std::ostringstream log;
		openmpt::detail::initial_ctls_map ctls;
		openmpt::module mod( stream, log, ctls );

		constexpr size_t kSamplerate = 48000;
		const size_t kCheckSamples = static_cast<size_t>( kSamplerate * kEndSeconds );
		std::queue<std::vector<float>> buffers;
		size_t samplesRead = 0;
		{
			std::vector<float> buffer( kCheckSamples );
			samplesRead = mod.read( kSamplerate, kCheckSamples, buffer.data() );
			buffer.resize( samplesRead );
			if ( samplesRead > 0 ) {
				buffers.push( std::move( buffer ) );
			}
		}

		if ( mod.get_duration_seconds() > 0 ) {
			while ( samplesRead && !m_stopLoopDetection && ( mod.get_position_seconds() <= mod.get_duration_seconds() ) ) {
				std::vector<float> buffer( kCheckSamples );
				samplesRead = mod.read( kSamplerate, kCheckSamples, buffer.data() );
				buffer.resize( samplesRead );
				if ( samplesRead > 0 ) {
					buffers.push( std::move( buffer ) );
					if ( buffers.size() > 2 ) {
						buffers.pop();
					}
				}
			}
		}

		if ( !m_stopLoopDetection && ( buffers.size() == 2 ) ) {
			size_t sampleCount = 0;
			float total = 0;

			const auto& backSamples = buffers.back();
			for ( const auto& sample : backSamples ) {
				total += fabs( sample );
				++sampleCount;
			}

			const auto& frontSamples = buffers.front();
			for ( auto sample = frontSamples.rbegin(); ( frontSamples.rend() != sample ) && ( sampleCount < kCheckSamples ); sample++, sampleCount++ ) {
				total += fabs( *sample );
			}

			if ( sampleCount > 0 ) {
				const float average = total / sampleCount;
				m_looped = average > kSilenceThreshold;
				if ( m_looped ) {
					m_module.set_repeat_count( -1 );
				}
			}
		}
	} catch ( const std::exception& ) {
	}
}

#endif // _NDEBUG