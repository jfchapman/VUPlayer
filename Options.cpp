#include "Options.h"

Options::Options( HINSTANCE instance, Settings& settings, Output& output ) :
	m_hInst( instance ),
	m_Settings( settings ),
	m_Output( output )
{
}

Options::~Options()
{
}

HINSTANCE Options::GetInstanceHandle() const
{
	return m_hInst;
}

Settings& Options::GetSettings()
{
	return m_Settings;
}

Output& Options::GetOutput()
{
	return m_Output;
}

void Options::OnNotify( const HWND /*hwnd*/, const WPARAM /*wParam*/, const LPARAM /*lParam*/ )
{
}
