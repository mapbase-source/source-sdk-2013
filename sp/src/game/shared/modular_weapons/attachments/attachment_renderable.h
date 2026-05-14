#ifndef ATTACHMENT_RENDERABLE_H
#define ATTACHMENT_RENDERABLE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL

#include "c_baseanimating.h"
#include "studio.h"

//-----------------------------------------------------------------------------
// Client-only renderable for weapon attachment models. Bonemerges to a parent
// entity (viewmodel, worldmodel, or weapon entity in the world). Does not
// consume server edicts — created via InitializeAsClientEntity().
//-----------------------------------------------------------------------------
class C_AttachmentRenderable : public C_BaseAnimating
{
public:
    DECLARE_CLASS(C_AttachmentRenderable, C_BaseAnimating)

    // Required for bonemerge to actually access bones on the parent skeleton.
    // Without these, bonemerge silently fails and the model floats.
    void SetupBonemerge()
    {
        m_BoneAccessor.SetReadableBones(BONE_USED_BY_ANYTHING);
        m_BoneAccessor.SetWritableBones(BONE_USED_BY_ANYTHING);
    }

    virtual RenderGroup_t GetRenderGroup() { return RENDER_GROUP_VIEW_MODEL_TRANSLUCENT; };

    // Sample lighting at the parent's illumination point instead of our own
    // bonemerged origin. Without this, the attachment can look unlit in lit
    // areas (or vice versa) because our bone-derived position is somewhere
    // weird relative to the parent's actual mesh.
    virtual bool OnInternalDrawModel(ClientModelRenderInfo_t* pInfo) OVERRIDE
    {
        if (!BaseClass::OnInternalDrawModel(pInfo))
            return false;

        C_BaseEntity* pMoveParent = GetMoveParent();
        if (!pMoveParent)
            return true;

        C_BaseAnimating* pParent = pMoveParent->GetBaseAnimating();
        if (!pParent)
            return true;

        CStudioHdr* pParentHdr = pParent->GetModelPtr();
        if (!pParentHdr)
            return true;

        // Static is fine here — only one model draws at a time per thread,
        // and pInfo->pLightingOrigin is consumed before the next call.
        static Vector vecLightingOrigin = vec3_origin;

        int iIllumAttachIdx = pParentHdr->IllumPositionAttachmentIndex();
        if (iIllumAttachIdx <= 0)
        {
            // No dedicated illum-position attachment; use the parent's
            // local illum position transformed by its world matrix.
            VectorTransform(pParentHdr->illumposition(),
                pParent->RenderableToWorldTransform(),
                vecLightingOrigin);
        }
        else
        {
            matrix3x4_t matAttachment;
            pParent->GetAttachment(iIllumAttachIdx, matAttachment);
            VectorTransform(pParentHdr->illumposition(),
                matAttachment,
                vecLightingOrigin);
        }

        pInfo->pLightingOrigin = &vecLightingOrigin;
        return true;
    }
};

#endif // CLIENT_DLL

#endif // ATTACHMENT_RENDERABLE_H