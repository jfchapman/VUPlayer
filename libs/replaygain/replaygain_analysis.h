/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001 David Robinson and Glen Sawyer
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  concept and filter values by David Robinson (David@Robinson.org)
 *    -- blame him if you think the idea is flawed
 *  coding by Glen Sawyer (glensawyer@hotmail.com) 442 N 700 E, Provo, UT 84606 USA
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *  minor cosmetic tweaks to integrate with FLAC by Josh Coalson
 *
 *	thread safe C++ version by James Chapman
 *	
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

#ifndef GAIN_ANALYSIS_H
#define GAIN_ANALYSIS_H

#include <stddef.h>

#include <vector>

#define GAIN_NOT_ENOUGH_SAMPLES				-24601
#define GAIN_ANALYSIS_ERROR						0
#define GAIN_ANALYSIS_OK							1

#define INIT_GAIN_ANALYSIS_ERROR			0
#define INIT_GAIN_ANALYSIS_OK					1

#define YULE_ORDER									  10
#define BUTTER_ORDER									2

extern float ReplayGainReferenceLoudness; /* in dB SPL, currently == 89.0 */

struct ReplayGainFilter {
	long rate;
	unsigned int downsample;
	float BYule[YULE_ORDER+1];
	float AYule[YULE_ORDER+1];
	float BButter[BUTTER_ORDER+1];
	float AButter[BUTTER_ORDER+1];
};

class replaygain_analysis
{
public:
	replaygain_analysis();

	virtual ~replaygain_analysis();

	bool InitGainAnalysis( const long samplefreq );

	bool ValidGainFrequency( const long samplefreq ) const;

	bool AnalyzeSamples( const float* left_samples, const float* right_samples, size_t num_samples, int num_channels );

	float GetTitleGain();

	float GetAlbumGain();

private:
	void filter( const float* input, float* output, size_t nSamples, const float* a, const float* b, const size_t order, const unsigned int downsample ) const;

	bool CreateGainFilter( long samplefreq, ReplayGainFilter& gainfilter ) const;

	bool ResetSampleFrequency( const long samplefreq );

	float analyzeResult( const std::vector<unsigned int>& Array ) const;

	ReplayGainFilter m_gainfilter;

	std::vector<float> m_linprebuf;
	std::vector<float> m_rinprebuf;

	std::vector<float> m_lstepbuf;
	std::vector<float> m_rstepbuf;

	std::vector<float> m_loutbuf;
	std::vector<float> m_routbuf;

	std::vector<unsigned int> m_A;
	std::vector<unsigned int> m_B;

	float*	m_linpre;                                          /* left input samples, with pre-buffer */
	float*	m_lstep;                                           /* left "first step" (i.e. post first filter) samples */
	float*	m_lout;                                            /* left "out" (i.e. post second filter) samples */
	float*	m_rinpre;                                          /* right input samples ... */
	float*	m_rstep;
	float*	m_rout;

	unsigned int		m_sampleWindow;                           /* number of samples required to reach number of milliseconds required for RMS window */
	unsigned long		m_totsamp;
	double					m_lsum;
	double					m_rsum;
};
#endif /* GAIN_ANALYSIS_H */
