#include "NullVisual.h"

NullVisual::NullVisual( WndVisual& wndVisual ) :
	Visual( wndVisual )
{
}

NullVisual::~NullVisual()
{
}

int NullVisual::GetHeight( const int /*width*/ )
{
	return 0;
}

void NullVisual::Show()
{
}

void NullVisual::Hide()
{
}

void NullVisual::OnPaint()
{
}

void NullVisual::OnSettingsChange()
{
}

void NullVisual::OnSysColorChange()
{
}
