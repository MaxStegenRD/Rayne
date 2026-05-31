//
//  RNCamera.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNCamera.h"
#include "../Rendering/RNRenderer.h"
#include "../Rendering/RNWindow.h"
#include "RNLight.h"
#include "RNLightManager.h"

namespace RN
{
	RNDefineMeta(Camera, SceneNode)

	Camera::Camera(RenderPass *renderPass) :
		_cameraSceneEntry(this), _flags(Camera::Flags::Defaults)
	{
		if(!renderPass)
		{
			_renderPass = new RenderPass();
		}
		else
		{
			_renderPass = renderPass->Retain();
		}
		_rootFramePass = nullptr;

		Initialize();
	}

	Camera::Camera(const Vector2 &size) :
		_cameraSceneEntry(this),
		_flags(Camera::Flags::Defaults)
	{
		//TODO: This implementaiton is probably not what a user would expect when creating a camera with a size tbh... It just renders it to the screens top left corner with the given size. Instead it should probably create a render target of that size and render into it instead.
		_renderPass = new RenderPass();
		_renderPass->SetFrame(RN::Rect(0, 0, size.x, size.y));
		_rootFramePass = nullptr;

		Initialize();
	}

	Camera::~Camera()
	{
		SafeRelease(_renderPass);
		SafeRelease(_rootFramePass);

		SafeRelease(_multiviewCameras);
		SafeRelease(_renderNodes);

		SafeRelease(_lightManager);
	}


	void Camera::Initialize()
	{
		_fov = 70.0f;
		_aspect = 0.0f;

		_clipNear = 0.1f;
		_clipFar = 500.0f;

		_orthoLeft = -100.0f;
		_orthoRight = 100.0f;
		_orthoBottom = -100.0f;
		_orthoTop = 100.0f;

		_fogNear = 100.0f;
		_fogFar = 500.0f;
		_ambient = Color::White();
		_customData = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		_customNearClipPlane = Plane();

		_dirtyProjection = true;
		_dirtyPosition = true;
		_dirtyFrustum = true;
		_hasCustomNearClipPlane = false;

		_priority = 0;
		_lodCamera = nullptr;

		_multiviewCameras = nullptr;
		_isMultiviewCamera = false;

		_frustumPlaneOffsets[0] = 0.0f;
		_frustumPlaneOffsets[1] = 0.0f;
		_frustumPlaneOffsets[2] = 0.0f;
		_frustumPlaneOffsets[3] = 0.0f;

		_firstNodeMember = nullptr;

		_renderNodes = nullptr;

		_lightManager = nullptr;
	}

	// Setter
	void Camera::SetRenderPass(RenderPass *renderPass)
	{
		RN_ASSERT(renderPass, "Camera render pass mustn't be NULL");
		RN_ASSERT(!renderPass->GetIsSubpass(), "Subpass render passes have to be inside a root renderpass that is not a sub pass");

		SafeRelease(_renderPass);
		_renderPass = renderPass->Retain();

		if(_rootFramePass == _renderPass)
		{
			SafeRelease(_rootFramePass);
			_rootFramePass = nullptr;
		}
	}

	void Camera::SetRootFramePass(FramePass *framePass)
	{
		if(!framePass || framePass == _renderPass)
		{
			SafeRelease(_rootFramePass);
			_rootFramePass = nullptr;
			return;
		}

		RenderPass *renderPass = framePass->Downcast<RenderPass>();
		RN_ASSERT(!renderPass || !renderPass->GetIsSubpass(), "Subpass render passes have to be inside a root renderpass that is not a sub pass");

		SafeRelease(_rootFramePass);
		_rootFramePass = framePass->Retain();
	}

	void Camera::SetFlags(Flags flags)
	{
		_flags = flags;
	}

	void Camera::SetLODCamera(Camera *camera)
	{
		_lodCamera = camera;
	}

	void Camera::SetPriority(int32 priority)
	{
		_priority = priority;
	}

	void Camera::SetFOV(float fov)
	{
		_fov = fov;
		_dirtyProjection = true;
	}
	void Camera::SetAspectRatio(float ratio)
	{
		_aspect = ratio;
		_dirtyProjection = true;
	}

	void Camera::SetClipNear(float near)
	{
		_clipNear = near;
		_dirtyProjection = true;
	}
	void Camera::SetClipFar(float far)
	{
		_clipFar = far;
		_dirtyProjection = true;
	}

	void Camera::SetFogColor0(Color color)
	{
		_fogColor0 = color;
	}
	void Camera::SetFogColor1(Color color)
	{
		_fogColor1 = color;
	}
	void Camera::SetFogNear(float near)
	{
		_fogNear = near;
	}
	void Camera::SetFogFar(float far)
	{
		_fogFar = far;
	}

	void Camera::SetAmbientColor(Color color)
	{
		_ambient = color;
	}

	void Camera::SetCustomData(const Vector4 &data)
	{
		_customData = data;
	}
	void Camera::SetCustomNearClipPlane(const Plane &clipPlane, bool enabled)
	{
		_customNearClipPlane = clipPlane;
		_hasCustomNearClipPlane = enabled;
	}

	void Camera::SetOrthogonalFrustum(float top, float bottom, float left, float right)
	{
		RN_ASSERT((_flags & Flags::Orthogonal), "SetOrthogonalFrustum() called, but the camera is not an orthogonal camera");

		_orthoLeft = left;
		_orthoRight = right;
		_orthoTop = top;
		_orthoBottom = bottom;

		_dirtyProjection = true;
	}

	void Camera::SetFrustumPlaneOffset(float topOffset, float bottomOffset, float leftOffset, float rightOffset)
	{
		_frustumPlaneOffsets[0] = topOffset;
		_frustumPlaneOffsets[1] = bottomOffset;
		_frustumPlaneOffsets[2] = leftOffset;
		_frustumPlaneOffsets[3] = rightOffset;
		_dirtyFrustum = true;
	}

	void Camera::SetProjectionMatrix(const Matrix &projectionMatrix)
	{
		_dirtyProjection = false;
		_dirtyFrustum = true;
		_projectionMatrix = projectionMatrix;
		_inverseProjectionMatrix = _projectionMatrix.GetInverse();
		UpdateFrustum();
	}

	void Camera::DidUpdate(ChangeSet changeSet)
	{
		SceneNode::DidUpdate(changeSet);

		if(changeSet & ChangeSet::Position)
		{
			_dirtyPosition = true;
		}
	}

	Matrix Camera::MakeShadowSplit(Camera *camera, Light *light, float cameraDistanceToCenter, float near, float far)
	{
		Rect frame = _renderPass->GetFrame();

		//Get camera frustum extends to be covered by the split
		//far plane is at z=0, near plane z=1 for reverse-z!
		Vector3 nearcenter = camera->ToWorld(Vector3(0.0f, 0.0f, near));
		Vector3 farcorner1 = camera->ToWorld(Vector3(1.0f, 1.0f, far));
		Vector3 farcorner2 = camera->ToWorld(Vector3(-1.0f, -1.0f, far));
		Vector3 farcenter = (farcorner1 + farcorner2) * 0.5f;
		Vector3 center = (nearcenter + farcenter) * 0.5f;

		//Calculate the size of a pixel in world units
		float dist = center.GetDistance(farcorner1);
		Vector3 pixelsize = Vector3(Vector2(dist * 2.0f), 1.0f) / Vector3(frame.width, frame.height, 1.0f);

		//Place the light camera above the splits center
		Vector3 pos = center - light->GetForward() * cameraDistanceToCenter;

		//Transform the position to light space
		Matrix rot = light->GetWorldRotation().GetRotationMatrix();
		pos = rot.GetInverse() * pos;

		//Snap to the pixel grid
		pos /= pixelsize;
		pos.x = roundf(pos.x);
		pos.y = roundf(pos.y);
		pos.z = roundf(pos.z);
		pos *= pixelsize;

		//Transform back and place the camera there
		pos = rot * pos;
		SetWorldPosition(pos);

		//Set the light camera frustum
		_orthoLeft = -dist;
		_orthoRight = dist;
		_orthoBottom = -dist;
		_orthoTop = dist;

		//Update the projection matrix
		_dirtyProjection = true;
		UpdateProjection(); //Because the target is always a valid framebuffer, we don't need to pass the renderer as parameter here

		//Return the resulting matrix
		Matrix projview = _projectionMatrix * GetWorldTransform().GetInverse();
		return projview;
	}

	// Helper
	void Camera::Update(float delta)
	{
		SceneNode::Update(delta);
		_dirtyPosition = true;
		if(_hasCustomNearClipPlane) _dirtyProjection = true;
	}

	void Camera::PostUpdate()
	{
		if(_dirtyPosition)
		{
			_dirtyPosition = false;
			_dirtyFrustum = true;

			_inverseViewMatrix = Matrix::WithTranslation(GetWorldPosition());
			_inverseViewMatrix.Rotate(GetWorldRotation());
			_inverseViewMatrix.Scale(GetScale());

			_viewMatrix = _inverseViewMatrix.GetInverse();
		}

		UpdateProjection();
		UpdateFrustum();
	}

	void Camera::UpdateProjection()
	{
		if(!_dirtyProjection)
			return;

		if(_flags & Flags::Orthogonal)
		{
			_projectionMatrix = Matrix::WithProjectionOrthogonal(_orthoLeft, _orthoRight, _orthoBottom, _orthoTop, _clipNear, _clipFar);
			_inverseProjectionMatrix = _projectionMatrix.GetInverse();
			return;
		}

		float tempAspect = _aspect;
		if(std::abs(tempAspect) <= 0.0001)
		{
			Vector2 size = _renderPass->GetFrame().GetSize();
			tempAspect = size.x / size.y;
		}

		_projectionMatrix = Matrix::WithProjectionPerspective(_fov, tempAspect, _clipNear, _clipFar);
		if(_hasCustomNearClipPlane)
		{
			Vector3 planePosition = _viewMatrix * _customNearClipPlane.GetPosition();
			Vector4 planeNormal = _viewMatrix * RN::Vector4(_customNearClipPlane.GetNormal(), 0.0f);
			_projectionMatrix.MakeObliqueNearPlane(Plane::WithPositionNormal(planePosition, RN::Vector3(planeNormal)));
		}

		_inverseProjectionMatrix = _projectionMatrix.GetInverse();

		_dirtyProjection = false;

		_dirtyFrustum = true;
		UpdateFrustum();
	}

	void Camera::UpdateFrustum()
	{
		if(!_dirtyFrustum) return;
		_dirtyFrustum = false;

		bool useSimpleCulling = _flags & Flags::UseSimpleCulling;
		if(useSimpleCulling)
		{
			_frustumCenter = Vector3(0.0f, 0.0f, _clipFar * 0.5f) + GetWorldPosition();
			_frustumRadius = _clipFar * 1.5;
		}

		//far plane is at z=0, near plane z=1 for reverse-z!
		Vector3 pos1 = __ToWorld(Vector3(-1.0f, 1.0f, 1.0f));
		Vector3 pos2 = __ToWorld(Vector3(-1.0f, 1.0f, 0.0));
		Vector3 pos3 = __ToWorld(Vector3(-1.0f, -1.0f, 0.0));
		Vector3 pos4 = __ToWorld(Vector3(1.0f, -1.0f, 1.0));
		Vector3 pos5 = __ToWorld(Vector3(1.0f, 1.0f, 0.0));
		Vector3 pos6 = __ToWorld(Vector3(1.0f, -1.0f, 0.0));
		Vector3 pos7 = __ToWorld(Vector3(1.0f, 1.0f, 1.0f));
		Vector3 pos8 = __ToWorld(Vector3(-1.0f, -1.0f, 1.0f));

		const Vector3 &position = GetWorldPosition();
		Vector3 direction = GetWorldRotation().GetRotatedVector(Vector3(0.0, 0.0, -1.0));

		if(!useSimpleCulling)
		{
			// Tighter sphere via farthest corner pair among 8 corners
			Vector3 corners[8] = { pos1, pos2, pos3, pos4, pos5, pos6, pos7, pos8 };
			float maxDist2 = 0.0f;
			Vector3 bestA = corners[0], bestB = corners[0];
			for(int i = 0; i < 8; ++i)
			{
				for(int j = i + 1; j < 8; ++j)
				{
					float d2 = corners[i].GetSquaredDistance(corners[j]);
					if(d2 > maxDist2)
					{
						maxDist2 = d2;
						bestA = corners[i];
						bestB = corners[j];
					}
				}
			}
			_frustumCenter = (bestA + bestB) * 0.5f;
			float radius = 0.0f;
			for(int i = 0; i < 8; ++i)
			{
				float d = _frustumCenter.GetDistance(corners[i]);
				if(d > radius) radius = d;
			}
			_frustumRadius = radius;
		}

		frustums._frustumLeft = Plane::WithTriangle(pos1, pos2, pos3, -1.0f, _frustumPlaneOffsets[2]);
		frustums._frustumRight = Plane::WithTriangle(pos4, pos5, pos6, 1.0f, -_frustumPlaneOffsets[3]);
		frustums._frustumTop = Plane::WithTriangle(pos1, pos2, pos5, 1.0f, _frustumPlaneOffsets[0]);
		frustums._frustumBottom = Plane::WithTriangle(pos4, pos3, pos6, -1.0f, -_frustumPlaneOffsets[1]);
		frustums._frustumNear = Plane::WithPositionNormal(position + direction * std::min(_clipNear, _clipFar), direction);
		frustums._frustumFar = Plane::WithPositionNormal(position + direction * std::max(_clipNear, _clipFar), -direction);
	}

	Vector3 Camera::__ToWorld(const Vector3 &dir)
	{
		PostUpdate();

		Vector4 ndcPos(dir.x, dir.y, dir.z, 1.0f);

		Vector4 vec = _inverseViewMatrix * (_inverseProjectionMatrix * ndcPos);
		vec /= vec.w;

		return Vector3(vec);
	}

	// There should be a much better solution, but at least this works for now
	Vector3 Camera::ToWorld(const Vector3 &dir)
	{
		PostUpdate();
		Vector4 ndcPos(dir.x, dir.y, 0.0f, 1.0f);

		Vector4 vec = _inverseProjectionMatrix * ndcPos;
		vec /= vec.w;

		float sign = vec.z < 0.0f ? -1.0f : 1.0f; //Otherwise the division would lose the sign
		vec /= vec.z * sign;
		vec *= dir.z;
		vec.w = 1.0;

		vec = _inverseViewMatrix * vec;

		return Vector3(vec);
	}

	void Camera::CreateLightManager(uint16_t maxPackedPointLights, uint16_t maxPackedSpotLights)
	{
		if(_lightManager) return;

		Rect frame = _renderPass->GetFrame();
		uint32 clustersX = std::max<uint32>(1u, static_cast<uint32>(frame.width) / 64u);
		uint32 clustersY = std::max<uint32>(1u, static_cast<uint32>(frame.height) / 64u);
		uint32 clusterXY = std::max<uint32>(1u, clustersX * clustersY);
		uint32 clustersZ = std::max<uint32>(1u, std::min((4000u * 6u) / clusterXY, 32u));
		float zLogFactor = 0.7f;
		_lightManager = new LightManager(clustersX, clustersY, clustersZ, zLogFactor, maxPackedPointLights, maxPackedSpotLights);
	}

	const Vector3 &Camera::GetFrustumCenter()
	{
		UpdateFrustum();
		return _frustumCenter;
	}

	float Camera::GetFrustumRadius()
	{
		UpdateFrustum();
		return _frustumRadius;
	}

	void Camera::GetFrustumPlanes(Vector4 *planes) const
	{
		if(!planes) return;

		const_cast<Camera *>(this)->UpdateFrustum();
		planes[0] = frustums._frustumLeft.GetPlaneVector();
		planes[1] = frustums._frustumRight.GetPlaneVector();
		planes[2] = frustums._frustumTop.GetPlaneVector();
		planes[3] = frustums._frustumBottom.GetPlaneVector();
		planes[4] = frustums._frustumNear.GetPlaneVector();
		planes[5] = frustums._frustumFar.GetPlaneVector();
	}

	bool Camera::InFrustum(const Vector3 &position, float radius)
	{
		//if(_hasCustomNearClipPlane) return true;

		if(_frustumCenter.GetDistance(position) > _frustumRadius + radius)
			return false;

		if(_flags & Flags::UseSimpleCulling)
			return true;

		if(frustums._frustumLeft.GetDistance(position) < -radius)
			return false;

		if(frustums._frustumRight.GetDistance(position) < -radius)
			return false;

		if(frustums._frustumTop.GetDistance(position) < -radius)
			return false;

		if(frustums._frustumBottom.GetDistance(position) < -radius)
			return false;

		if(frustums._frustumNear.GetDistance(position) < -radius)
			return false;

		if(frustums._frustumFar.GetDistance(position) < -radius)
			return false;

		return true;
	}

	bool Camera::InFrustum(const Sphere &sphere)
	{
		UpdateFrustum();
		return InFrustum(sphere.position + sphere.offset, sphere.radius);
	}

	bool Camera::InFrustum(const AABB &aabb)
	{
		UpdateFrustum();

		// Early-out: camera-centered sphere intersection
		const Vector3 mn = aabb.position + aabb.minExtend;
		const Vector3 mx = aabb.position + aabb.maxExtend;
		const float nx = std::max(mn.x, std::min(_frustumCenter.x, mx.x));
		const float ny = std::max(mn.y, std::min(_frustumCenter.y, mx.y));
		const float nz = std::max(mn.z, std::min(_frustumCenter.z, mx.z));
		const float dx = _frustumCenter.x - nx;
		const float dy = _frustumCenter.y - ny;
		const float dz = _frustumCenter.z - nz;
		const float dist2 = dx*dx + dy*dy + dz*dz;
		if(dist2 > _frustumRadius*_frustumRadius) return false;

		for(int i = 0; i < 6; ++i)
		{
			const Plane &plane = (&frustums._frustumLeft)[i];

			//Pick the corner most in direction of the plane normal
			Vector3 positive;
			positive.x = (plane.GetNormal().x >= 0) ? (aabb.position.x + aabb.maxExtend.x) : (aabb.position.x + aabb.minExtend.x);
			positive.y = (plane.GetNormal().y >= 0) ? (aabb.position.y + aabb.maxExtend.y) : (aabb.position.y + aabb.minExtend.y);
			positive.z = (plane.GetNormal().z >= 0) ? (aabb.position.z + aabb.maxExtend.z) : (aabb.position.z + aabb.minExtend.z);

			if(plane.GetDistance(positive) <= 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	void Camera::AddMultiviewCamera(RN::Camera *camera)
	{
		RN_ASSERT(camera, "Camera cannot be empty");

		if(!_multiviewCameras)
		{
			_multiviewCameras = new RN::Array();
		}

		_multiviewCameras->AddObject(camera);
		camera->_isMultiviewCamera = true;
	}

	void Camera::RemoveMultiviewCamera(RN::Camera *camera)
	{
		_multiviewCameras->RemoveObject(camera);
		camera->_isMultiviewCamera = false;
	}

	void Camera::SetFirstSceneNodeMember(IntrusiveList<SceneNode>::Member *member)
	{
		_firstNodeMember = member;
	}

	void Camera::AddRenderNode(SceneNode *node)
	{
		if(!_renderNodes) _renderNodes = new Array();

		_renderNodes->AddObject(node);

		/*size_t insertIndex = 0;
		_renderNodes->Enumerate<SceneNode>([&](SceneNode *oldNode, size_t index, bool &stop) {
			if(oldNode->GetRenderPriority() > node->GetRenderPriority())
			{
				insertIndex = index;
				stop = true;
			}
		});
		_renderNodes->InsertObjectAtIndex(node, insertIndex);*/
	}

	void Camera::RemoveRenderNode(SceneNode *node)
	{
		if(!_renderNodes) return;
		_renderNodes->RemoveObject(node);
	}

	void Camera::TruncateRenderNodes(size_t length)
	{
		if(!_renderNodes) return;
		while(_renderNodes->GetCount() > length)
		{
			_renderNodes->RemoveObjectAtIndex(length);
		}
	}

	void Camera::ClearRenderNodes()
	{
		SafeRelease(_renderNodes);
	}
} // namespace RN
