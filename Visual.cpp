#include "Visual.h"

Visual::Visual( WndVisual& wndVisual ) :
	m_WndVisual( wndVisual )
{
}

Visual::~Visual()
{
}

Output& Visual::GetOutput()
{
	return m_WndVisual.GetOutput();
}

Library& Visual::GetLibrary()
{
	return m_WndVisual.GetLibrary();
}

Settings& Visual::GetSettings()
{
	return m_WndVisual.GetSettings();
}

ID2D1DeviceContext* Visual::BeginDrawing()
{
	return m_WndVisual.BeginDrawing();
}

void Visual::EndDrawing()
{
	m_WndVisual.EndDrawing();
}

void Visual::DoRender()
{
	m_WndVisual.DoRender();
}

bool Visual::IsHardwareAccelerationEnabled() const
{
	return m_WndVisual.IsHardwareAccelerationEnabled();
}

float Visual::GetDPIScalingFactor() const
{
	return m_WndVisual.GetDPIScalingFactor();
}
