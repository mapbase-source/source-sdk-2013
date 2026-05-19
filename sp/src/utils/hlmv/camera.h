//=============================================================================//
//
//  Resonance Team
//   - Camera helper class for HLMV
//   - Handles view models and math  
//
//=============================================================================//

#ifndef INC_CAMERA_H
#define INC_CAMERA_H

#include "tier2/camerautils.h"

enum class CameraViewMode
{
	ORBIT,
	FIRST_PERSON,
	VIEW_MODEL,

};

class CCamera
{
public:

	CCamera();


	void GetViewMatrix(VMatrix& viewMatrix);
	void GetProjectionMatrix(VMatrix& projMatrix, float w, float h);

	void UpdateView();

	struct
	{
		QAngle angles; // View angles of our orbit
		Vector origin; // What we're orbiting around
		float  zoom; // How far away we are from our origin
	} m_orbit;

	float m_fov;
private:

	Camera_t m_cam;

	CameraViewMode m_viewMode;


};


#endif