//
//  RNDebugRenderer.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_DEBUGRENDERER_H__
#define __RAYNE_DEBUGRENDERER_H__

#include "../Math/RNAABB.h"
#include "../Math/RNColor.h"
#include "../Math/RNSphere.h"
#include "../Objects/RNString.h"
#include "../Scene/RNSceneAttachment.h"

namespace RN
{
	class DebugDraw;
	class DebugRenderer;

	struct DebugDrawOptions
	{
		DebugDrawOptions() = default;
		DebugDrawOptions(const Color &color) :
			color(color)
		{}

		Color color = Color::White();
		float duration = 0.0f;
		float thickness = 0.005f;
		bool depthTest = true;
	};

	class DebugTextBackend : public Object
	{
	public:
		RNAPI DebugTextBackend();
		RNAPI ~DebugTextBackend() override;

	protected:
		RNAPI virtual void Clear();
		RNAPI virtual void Update(float delta);
		RNAPI virtual void WillRender(Renderer *renderer, Camera *camera);
		RNAPI virtual void DidRender(Renderer *renderer);
		RNAPI virtual void DrawText(Scene *scene, const Vector3 &position, const String *text, const DebugDrawOptions &options) = 0;

	private:
		friend class DebugRenderer;

		RNDeclareMetaAPI(DebugTextBackend, RNAPI)
	};

	class DebugRenderer : public SceneAttachment
	{
	public:
		RNAPI ~DebugRenderer() override;

	protected:
		RNAPI void Update(float delta) override;
		RNAPI void WillRender(Renderer *renderer) override;
		RNAPI void DidRender(Renderer *renderer) override;
		RNAPI void SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context) override;

	private:
		friend class DebugDraw;

		RNAPI DebugRenderer();

		RNAPI void Clear();
		RNAPI void DrawPoint(const Vector3 &position, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawLine(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawRay(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawAABB(const RN::AABB &aabb, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawSphere(const Vector3 &center, float radius, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawSphere(const RN::Sphere &sphere, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI void DrawText(const Vector3 &position, const String *text, const DebugDrawOptions &options = DebugDrawOptions());

		struct Internals;
		Internals *_internals;

		void InstallInScene(Scene *scene);
		void UninstallFromScene();
		void SetTextBackend(DebugTextBackend *backend);
		void AddCommand(uint8 type, const PositionType &origin, const Vector3 &from, const Vector3 &to, const RN::AABB &aabb, float radius, const DebugDrawOptions &options);
		void Rebuild(Renderer *renderer);

		RNDeclareMetaAPI(DebugRenderer, RNAPI)
	};

	class DebugDraw
	{
	public:
		RNAPI static void AttachToScene(Scene *scene, DebugTextBackend *textBackend = nullptr);

		RNAPI static void Clear();

		RNAPI static void Point(const Vector3 &position, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Line(const Vector3 &from, const Vector3 &to, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Ray(const Vector3 &origin, const Vector3 &direction, float length, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Ray(const Vector3 &origin, const Vector3 &direction, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void AABB(const RN::AABB &aabb, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Sphere(const Vector3 &center, float radius, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Sphere(const RN::Sphere &sphere, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Text(const Vector3 &position, const String *text, const DebugDrawOptions &options = DebugDrawOptions());
		RNAPI static void Text(const Vector3 &position, const char *text, const DebugDrawOptions &options = DebugDrawOptions());
	};
} // namespace RN

#endif /* __RAYNE_DEBUGRENDERER_H__ */
