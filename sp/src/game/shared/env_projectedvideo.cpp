#include "cbase.h"

#ifdef CLIENT_DLL
#define CEnvProjectedVideo	 C_EnvProjectedVideo
#define CEnvProjectedTexture C_EnvProjectedTexture

#include "C_Env_Projected_Texture.h"
#include "video/ivideoservices.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#else
#include "env_projectedtexture.h"
#endif

class CEnvProjectedVideo : public CEnvProjectedTexture
{
public:
	DECLARE_CLASS(CEnvProjectedVideo, CEnvProjectedTexture);

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS();
#else
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
#endif

	CEnvProjectedVideo();
	~CEnvProjectedVideo();

#ifdef CLIENT_DLL
	virtual void OnDataChanged(DataUpdateType_t type);
	virtual void ClientThink();
#endif

private:
#ifdef GAME_DLL

	CNetworkVar(string_t, m_szVideoFile);

#else

	void CreateVideo();
	void DestroyVideo();
	void RenderVideoToTexture();

	char m_szVideoFile[256];

	IVideoMaterial* m_pVideoMaterial;

	CTextureReference m_pVideoRT;
	CMaterialReference m_pVideoRTMaterial;

	float m_flLastVideoTime;
	float m_flaspect_ratio;
#endif
};

#ifdef GAME_DLL

LINK_ENTITY_TO_CLASS(env_projectedvideo, CEnvProjectedVideo);

IMPLEMENT_SERVERCLASS_ST(CEnvProjectedVideo, DT_EnvProjectedVideo)
	SendPropStringT(SENDINFO(m_szVideoFile)),
END_SEND_TABLE()

BEGIN_DATADESC(CEnvProjectedVideo)
	DEFINE_KEYFIELD(m_szVideoFile, FIELD_STRING, "VideoFile"),
END_DATADESC()

#else

#undef CEnvProjectedVideo

IMPLEMENT_CLIENTCLASS_DT(C_EnvProjectedVideo, DT_EnvProjectedVideo, CEnvProjectedVideo)
	RecvPropString(RECVINFO(m_szVideoFile)),
END_RECV_TABLE()

#define CEnvProjectedVideo C_EnvProjectedVideo
#endif

CEnvProjectedVideo::CEnvProjectedVideo()
{
#ifdef CLIENT_DLL
	m_pVideoMaterial = nullptr;
	m_flLastVideoTime = -1.0f;
#endif // CLIENT_DLL
}

CEnvProjectedVideo::~CEnvProjectedVideo()
{
#ifdef CLIENT_DLL
	DestroyVideo();
	SetNextClientThink(CLIENT_THINK_NEVER);
#endif // CLIENT_DLL
}

#ifdef CLIENT_DLL
void CEnvProjectedVideo::OnDataChanged(DataUpdateType_t type)
{
	if (type == DATA_UPDATE_CREATED)
	{
		CreateVideo();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}

	BaseClass::OnDataChanged(type);
}

void CEnvProjectedVideo::RenderVideoToTexture()
{
	if (!m_pVideoMaterial || !m_pVideoRT)
		return;

	// Get the video frame size
	int w = 0;
	int h = 0;
	m_pVideoMaterial->GetVideoImageSize(&w, &h);

	if (w <= 0 || h <= 0)
		return;

	IMaterial* videoMat = m_pVideoMaterial->GetMaterial();
	if (!videoMat)
		return;

	bool found = false;
	IMaterialVar *pVar = videoMat->FindVar("$ytexture", &found);

	int ytexW, ytexH;
	ytexW = ytexH = 0;
	if (pVar)
	{
		ytexW = pVar->GetTextureValue()->GetActualWidth();
		ytexH = pVar->GetTextureValue()->GetActualHeight();
	}

	if (ytexH <= 0 && ytexW <= 0)
		return;


	CMatRenderContextPtr ctx(materials);

	// Set the render target
	ctx->PushRenderTargetAndViewport((ITexture*)m_pVideoRT);

	// Clear RT completely
	ctx->ClearColor4ub(0, 0, 0, 255);
	ctx->ClearBuffers(true, true);

	// Draw the video material into the RT
	ctx->DrawScreenSpaceRectangle(
		videoMat,
		0, 0, 
		w, h,        // destination rectangle
		0, 0,        // source texel start
		w, h,        // source texel end
		ytexW, ytexH   // source texture dimensions
	);

	// Restore previous render target
	ctx->PopRenderTargetAndViewport();
}

void CEnvProjectedVideo::CreateVideo()
{
	if (!g_pVideo)
		return;

	if (!m_szVideoFile[0])
		return;

	// If we already have a material, destroy & recreate it
	if (m_pVideoMaterial)
	{
		ConColorMsg(Color(200, 200, 0, 255), "[ProjectedVideo] Recreating existing video material.\n");
		DestroyVideo();
	}

	//Need to do this when it gets recreated so the texture still gets update
	char name[60];
	V_snprintf(name, sizeof(name), "ProjectedVideoMaterial_%d_%d", entindex(), random->RandomInt(0, 20));

	m_pVideoMaterial = g_pVideo->CreateVideoMaterial(
		name,
		m_szVideoFile,
		"GAME",
		VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS | VideoPlaybackFlags::NO_AUDIO | VideoPlaybackFlags::LOOP_VIDEO | VideoPlaybackFlags::DONT_AUTO_START_VIDEO,
		VideoSystem::DETERMINE_FROM_FILE_EXTENSION);

	if (!m_pVideoMaterial)
	{
		ConColorMsg(Color(255, 100, 100, 255), "[ProjectedVideo] CreateVideo: failed to create video material for '%s'.\n", STRING(m_szVideoFile));
		return;
	}

	ConColorMsg(Color(255, 100, 100, 255), "[ProjectedVideo] CreateVideo: material shader = %s.\n", m_pVideoMaterial->GetMaterial()->GetShaderName());

	int w, h;
	m_pVideoMaterial->GetVideoImageSize(&w, &h);

	g_pMaterialSystem->BeginRenderTargetAllocation();

	m_pVideoRT.Init( g_pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		VarArgs("_rt_projectedvideo_%d", entindex()),
		w,
		h,
		RT_SIZE_OFFSCREEN,
		g_pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_NONE,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR
	));

	g_pMaterialSystem->EndRenderTargetAllocation();

	KeyValues* kv = new KeyValues("UnlitGeneric");
	ITexture *pTex = (ITexture*)m_pVideoRT;
	kv->SetString("$basetexture", pTex->GetName());
	kv->SetInt("$translucent", 1);

	m_pVideoRTMaterial.Init( materials->CreateMaterial(
		VarArgs("ProjectedVideoRTMat_%d", entindex()),
		kv
	));

	m_pVideoMaterial->StartVideo();

	ConColorMsg(Color(255, 255, 20, 255), "[ProjectedVideo] Created video material. Looping=%s\n", m_pVideoMaterial->IsLooping() ? "true" : "false");

	BaseClass::SetVideoMaterial(m_pVideoRTMaterial);
}

void CEnvProjectedVideo::DestroyVideo()
{
	if (m_pVideoMaterial && g_pVideo)
	{

		ConColorMsg(Color(255, 120, 120, 255), "[ProjectedVideo] Destroying video material.\n");
		g_pVideo->DestroyVideoMaterial(m_pVideoMaterial);
		m_pVideoMaterial = nullptr;
		m_pVideoRT.Shutdown();
		m_pVideoRTMaterial.Shutdown();
		BaseClass::SetVideoMaterial(NULL);
		ShutDownLightHandle();
	}

}

void CEnvProjectedVideo::ClientThink()
{

	if (!m_bState)
	{
		if (m_pVideoMaterial)
		{
			DestroyVideo();
		}
		SetNextClientThink(CLIENT_THINK_ALWAYS);
		return;
	}

	if (!m_pVideoMaterial)
	{
		CreateVideo();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
		return;
	}

	// Pump decoder first
	m_pVideoMaterial->Update();

	RenderVideoToTexture();

	// Think at ~30fps instead of every engine tick to reduce load
	SetNextClientThink(gpGlobals->curtime + 0.03f);
}

#endif