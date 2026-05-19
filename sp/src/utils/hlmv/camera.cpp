//========= Copyright ? 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "camera.h"
#include "vmatrix.h"


CCamera::CCamera()
{
	
	m_cam   = { {0,   0, 0}, {0, 0, 0}, 65, 1.0f, 20000.0f };
	// 180 so we face the model
	m_orbit = { {0, 180, 0}, {0, 0, 0}, 60 };

}


void CCamera::GetViewMatrix(VMatrix& viewMatrix)
{
	ComputeViewMatrix(&viewMatrix, m_cam);
}


void CCamera::GetProjectionMatrix(VMatrix& projMatrix, float w, float h)
{
	ComputeProjectionMatrix(&projMatrix, m_cam, w, h);
}


void CCamera::UpdateView()
{
	if (m_viewMode == CameraViewMode::ORBIT)
	{

		Vector vecOrbitOrigin = m_orbit.origin;
		Vector vecOrbitAng;
		AngleVectors(m_orbit.angles, &vecOrbitAng);
		vecOrbitOrigin -= vecOrbitAng * m_orbit.zoom;

		m_cam.m_angles = m_orbit.angles;
		m_cam.m_origin = vecOrbitOrigin;
	}
	else
	{

	}

	m_cam.m_flFOV = m_fov;
}
