//
//  RNRenderFrame.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_RENDERFRAME_H__
#define __RAYNE_RENDERFRAME_H__

#include "RNDrawable.h"
#include "RNRenderingConfig.h"
#include "RNRenderPass.h"
#include "../Scene/RNCamera.h"
#include "../Scene/RNLight.h"
#include "../Scene/RNLightManager.h"

namespace RN
{
	class Texture;

	class RenderFrame
	{
	public:
		struct CameraStatistics
		{
			uint64 numberOfDrawables = 0;
			uint64 numberOfDrawCalls = 0;
			uint64 numberOfVertices = 0;
			uint64 numberOfIndices = 0;
		};

		class CameraSnapshot
		{
		public:
			CameraSnapshot() :
				_tag(0)
			{}

			CameraSnapshot(const Vector3 &viewPosition, const Matrix &viewMatrix, const Matrix &inverseViewMatrix, const Matrix &projectionMatrix, const Matrix &inverseProjectionMatrix, const Matrix &projectionViewMatrix, const Matrix &inverseProjectionViewMatrix, const Color &ambientColor, const Vector4 &customData, const Color &fogColor0, const Color &fogColor1, const Vector2 &clipDistance, const Vector2 &fogDistance, int32 tag, const Rect &frame) :
				_viewPosition(viewPosition),
				_viewMatrix(viewMatrix),
				_inverseViewMatrix(inverseViewMatrix),
				_projectionMatrix(projectionMatrix),
				_inverseProjectionMatrix(inverseProjectionMatrix),
				_projectionViewMatrix(projectionViewMatrix),
				_inverseProjectionViewMatrix(inverseProjectionViewMatrix),
				_ambientColor(ambientColor),
				_customData(customData),
				_fogColor0(fogColor0),
				_fogColor1(fogColor1),
				_clipDistance(clipDistance),
				_fogDistance(fogDistance),
				_tag(tag),
				_frame(frame)
			{}

			static CameraSnapshot WithCamera(const Camera *camera, const Rect &frame)
			{
				return WithCameraProjection(camera, frame, camera->GetProjectionMatrix());
			}

			static CameraSnapshot WithCamera(const Camera *camera, const Rect &frame, const Matrix &projectionCorrection)
			{
				return WithCameraProjection(camera, frame, projectionCorrection * camera->GetProjectionMatrix());
			}

			const Vector3 &GetViewPosition() const { return _viewPosition; }
			const Matrix &GetViewMatrix() const { return _viewMatrix; }
			const Matrix &GetInverseViewMatrix() const { return _inverseViewMatrix; }
			const Matrix &GetProjectionMatrix() const { return _projectionMatrix; }
			const Matrix &GetInverseProjectionMatrix() const { return _inverseProjectionMatrix; }
			const Matrix &GetProjectionViewMatrix() const { return _projectionViewMatrix; }
			const Matrix &GetInverseProjectionViewMatrix() const { return _inverseProjectionViewMatrix; }
			const Color &GetAmbientColor() const { return _ambientColor; }
			const Vector4 &GetCustomData() const { return _customData; }
			const Color &GetFogColor0() const { return _fogColor0; }
			const Color &GetFogColor1() const { return _fogColor1; }
			const Vector2 &GetClipDistance() const { return _clipDistance; }
			const Vector2 &GetFogDistance() const { return _fogDistance; }
			const int32 &GetTag() const { return _tag; }
			const Rect &GetFrame() const { return _frame; }

		private:
			static CameraSnapshot WithCameraProjection(const Camera *camera, const Rect &frame, const Matrix &projectionMatrix)
			{
				Matrix viewMatrix = camera->GetViewMatrix();
				Matrix inverseViewMatrix = camera->GetInverseViewMatrix();
				Matrix inverseProjectionMatrix = camera->GetInverseProjectionMatrix();
				return CameraSnapshot(camera->GetWorldPosition(), viewMatrix, inverseViewMatrix, projectionMatrix, inverseProjectionMatrix, projectionMatrix * viewMatrix, inverseViewMatrix * inverseProjectionMatrix, camera->GetAmbientColor(), camera->GetCustomData(), camera->GetFogColor0(), camera->GetFogColor1(), Vector2(camera->GetClipNear(), camera->GetClipFar()), Vector2(camera->GetFogNear(), camera->GetFogFar()), camera->GetTag(), frame);
			}

			Vector3 _viewPosition;
			Matrix _viewMatrix;
			Matrix _inverseViewMatrix;
			Matrix _projectionMatrix;
			Matrix _inverseProjectionMatrix;
			Matrix _projectionViewMatrix;
			Matrix _inverseProjectionViewMatrix;
			Color _ambientColor;
			Vector4 _customData;
			Color _fogColor0;
			Color _fogColor1;
			Vector2 _clipDistance;
			Vector2 _fogDistance;
			int32 _tag;
			Rect _frame;
		};

		class DirectionalLight
		{
		public:
			DirectionalLight(const Vector3 &direction, const Vector4 &color) :
				_direction(direction),
				_padding(0.0f),
				_color(color)
			{}

			static DirectionalLight WithLight(const Light *light)
			{
				return DirectionalLight(light->GetForward(), light->GetFinalColor());
			}

		private:
			Vector3 _direction;
			float _padding;
			Vector4 _color;
		};

		class PointLight
		{
		public:
			PointLight(const Vector3 &position, float range, const Vector4 &color) :
				_position(position),
				_range(range),
				_color(color)
			{}

			static PointLight WithLight(const Light *light)
			{
				return PointLight(light->GetWorldPosition(), light->GetRange(), light->GetFinalColor());
			}

		private:
			Vector3 _position;
			float _range;
			Vector4 _color;
		};

		class SpotLight
		{
		public:
			SpotLight(const Vector3 &position, float range, const Vector3 &direction, float angle, const Vector4 &color) :
				_position(position),
				_range(range),
				_direction(direction),
				_angle(angle),
				_color(color)
			{}

			static SpotLight WithLight(const Light *light)
			{
				return SpotLight(light->GetWorldPosition(), light->GetRange(), light->GetForward(), light->GetAngleCos(), light->GetFinalColor());
			}

		private:
			Vector3 _position;
			float _range;
			Vector3 _direction;
			float _angle;
			Vector4 _color;
		};

		class DrawItem
		{
		public:
			DrawItem(Drawable *sourceDrawable, const Drawable::DrawSnapshotBundle &drawSnapshot) :
				_sourceDrawable(sourceDrawable),
				_drawSnapshot(drawSnapshot),
				_modelMatrix(sourceDrawable->GetModelMatrix()),
				_inverseModelMatrix(sourceDrawable->GetInverseModelMatrix())
			{
				sourceDrawable->GetMeshBufferSnapshot(_meshBuffers);
			}

			Drawable *GetSourceDrawableForPreparation() const { return _sourceDrawable; }
			const Mesh::DrawSnapshot &GetMesh() const { return _drawSnapshot.GetMesh(); }
			const Mesh::BufferSnapshot &GetMeshBuffers() const { return _meshBuffers; }
			const Material::DrawSnapshot &GetMaterial() const { return _drawSnapshot.GetMaterial(); }
			const Skeleton::DrawSnapshot &GetSkeleton() const { return _drawSnapshot.GetSkeleton(); }
			const Matrix &GetModelMatrix() const { return _modelMatrix; }
			const Matrix &GetInverseModelMatrix() const { return _inverseModelMatrix; }
			uint64 GetMaterialSnapshotVersion() const { return _drawSnapshot.GetMaterialSnapshotVersion(); }
			bool CanInstanceWith(const DrawItem &other) const { return GetMesh().CanInstanceWith(other.GetMesh()) && _meshBuffers.CanInstanceWith(other._meshBuffers); }

		private:
			Drawable *_sourceDrawable;
			Drawable::DrawSnapshotBundle _drawSnapshot;
			Mesh::BufferSnapshot _meshBuffers;
			Matrix _modelMatrix;
			Matrix _inverseModelMatrix;
		};

		class Pass
		{
		public:
			Pass(const RenderPass::DrawSnapshot &drawSnapshot, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialCacheIdentity, uint64 overrideMaterialSnapshotVersion) :
				_drawSnapshot(drawSnapshot),
				_hasOverrideMaterial(overrideMaterialSnapshot != nullptr),
				_overrideMaterialCacheIdentity(overrideMaterialSnapshot ? overrideMaterialCacheIdentity : 0),
				_overrideMaterialSnapshotVersion(overrideMaterialSnapshot ? overrideMaterialSnapshotVersion : 0)
			{
				if(_hasOverrideMaterial)
					_overrideMaterialSnapshot = *overrideMaterialSnapshot;
			}

			void AddDrawItemIndex(size_t drawItemIndex)
			{
				_drawItemIndices.push_back(drawItemIndex);
			}

			void SetCameraSnapshot(const CameraSnapshot &cameraSnapshot) { _cameraSnapshot = cameraSnapshot; }
			const CameraSnapshot &GetCameraSnapshot() const { return _cameraSnapshot; }
			void AddMultiviewCameraSnapshot(const CameraSnapshot &cameraSnapshot) { _multiviewCameraSnapshots.push_back(cameraSnapshot); }
			uint8 GetMultiviewCameraCount() const { return static_cast<uint8>(_multiviewCameraSnapshots.size()); }
			const std::vector<CameraSnapshot> &GetMultiviewCameraSnapshots() const { return _multiviewCameraSnapshots; }

			void AddDirectionalLight(const DirectionalLight &light) { _directionalLights.push_back(light); }
			void AddPointLight(const PointLight &light) { _pointLights.push_back(light); }
			void AddSpotLight(const SpotLight &light) { _spotLights.push_back(light); }
			const std::vector<DirectionalLight> &GetDirectionalLights() const { return _directionalLights; }
			const std::vector<PointLight> &GetPointLights() const { return _pointLights; }
			const std::vector<SpotLight> &GetSpotLights() const { return _spotLights; }
			void SetLightClusterSnapshot(const LightManager::DrawSnapshot &snapshot) { _lightClusterSnapshot = snapshot; }
			const LightManager::DrawSnapshot &GetLightClusterSnapshot() const { return _lightClusterSnapshot; }

			void SetDirectionalShadowDepthTexture(Texture *texture) { _directionalShadowDepthTexture = texture; }
			Texture *GetDirectionalShadowDepthTexture() const { return _directionalShadowDepthTexture; }
			void SetDirectionalShadowMatrices(const std::vector<Matrix> &matrices) { _directionalShadowMatrices = matrices; }
			const std::vector<Matrix> &GetDirectionalShadowMatrices() const { return _directionalShadowMatrices; }
			void SetDirectionalShadowInfo(const Vector2 &info) { _directionalShadowInfo = info; }
			const Vector2 &GetDirectionalShadowInfo() const { return _directionalShadowInfo; }

			const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return _drawSnapshot; }
			const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const { return _hasOverrideMaterial ? &_overrideMaterialSnapshot : nullptr; }
			uint64 GetOverrideMaterialCacheIdentity() const { return _overrideMaterialCacheIdentity; }
			uint64 GetOverrideMaterialSnapshotVersion() const { return _overrideMaterialSnapshotVersion; }
			const std::vector<size_t> &GetDrawItemIndices() const { return _drawItemIndices; }

		private:
			RenderPass::DrawSnapshot _drawSnapshot;
			bool _hasOverrideMaterial;
			uint64 _overrideMaterialCacheIdentity;
			uint64 _overrideMaterialSnapshotVersion;
			Material::DrawSnapshot _overrideMaterialSnapshot;
			std::vector<size_t> _drawItemIndices;
			CameraSnapshot _cameraSnapshot;
			std::vector<CameraSnapshot> _multiviewCameraSnapshots;
			std::vector<DirectionalLight> _directionalLights;
			std::vector<PointLight> _pointLights;
			std::vector<SpotLight> _spotLights;
			LightManager::DrawSnapshot _lightClusterSnapshot;
			std::vector<Matrix> _directionalShadowMatrices;
			Texture *_directionalShadowDepthTexture = nullptr;
			Vector2 _directionalShadowInfo;
		};

		static constexpr size_t InvalidPassIndex = static_cast<size_t>(-1);
		static constexpr size_t InvalidDrawItemIndex = static_cast<size_t>(-1);

		void Clear()
		{
			_frameID = 0;
			_passes.clear();
			_drawItems.clear();
			_cameraStatistics.clear();
		}

		uint64 GetFrameID() const { return _frameID; }

		size_t AddPass(const RenderPass::DrawSnapshot &drawSnapshot, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialCacheIdentity, uint64 overrideMaterialSnapshotVersion)
		{
			_passes.emplace_back(drawSnapshot, overrideMaterialSnapshot, overrideMaterialCacheIdentity, overrideMaterialSnapshotVersion);
			return _passes.size() - 1;
		}

		size_t AddDrawItem(Drawable *sourceDrawable)
		{
			if(_drawItems.size() == _drawItems.capacity())
			{
				size_t capacity = _drawItems.capacity();
				_drawItems.reserve(capacity + (capacity > 0 ? capacity : RN_RENDERING_DRAW_ITEM_RESERVE_BLOCK_SIZE));
			}

			Drawable::DrawSnapshotBundle drawSnapshot = sourceDrawable->GetDrawSnapshotBundleForFrame(_frameID);
			_drawItems.emplace_back(sourceDrawable, drawSnapshot);
			return _drawItems.size() - 1;
		}

		void ReserveDrawItems(size_t count)
		{
			_drawItems.reserve(count);
		}

		const DrawItem &GetDrawItem(size_t index) const
		{
			RN_DEBUG_ASSERT(index < _drawItems.size(), "Invalid render frame draw item index");
			return _drawItems[index];
		}

		size_t GetDrawItemCount() const { return _drawItems.size(); }

		Pass &GetPass(size_t index)
		{
			RN_DEBUG_ASSERT(index < _passes.size(), "Invalid render frame pass index");
			return _passes[index];
		}

		const Pass &GetPass(size_t index) const
		{
			RN_DEBUG_ASSERT(index < _passes.size(), "Invalid render frame pass index");
			return _passes[index];
		}

		size_t GetPassCount() const { return _passes.size(); }

		size_t AddCameraStatistics()
		{
			_cameraStatistics.emplace_back();
			return _cameraStatistics.size() - 1;
		}

		CameraStatistics &GetCameraStatistics(size_t index)
		{
			RN_DEBUG_ASSERT(index < _cameraStatistics.size(), "Invalid render frame camera statistics index");
			return _cameraStatistics[index];
		}

		const std::vector<CameraStatistics> &GetCameraStatistics() const { return _cameraStatistics; }

	private:
		friend class Renderer;

		void SetFrameID(uint64 frameID) { _frameID = frameID; }

		uint64 _frameID = 0;
		std::deque<Pass> _passes;
		std::vector<DrawItem> _drawItems;
		std::vector<CameraStatistics> _cameraStatistics;
	};
} // namespace RN

#endif /* __RAYNE_RENDERFRAME_H__ */
