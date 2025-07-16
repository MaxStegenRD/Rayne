//
//  RNModel.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#define RN_MODEL_LOD_DISABLED 1

#ifndef __RAYNE_MODEL_H_
	#define __RAYNE_MODEL_H_

	#include "../Assets/RNAsset.h"
	#include "../Base/RNBase.h"
	#include "RNMaterial.h"
	#include "RNMesh.h"

namespace RN
{
	class Skeleton;
	class ShadowVolume;
	class Camera;
	class Model : public Asset
	{
	public:
		friend class Entity;
		class LODStage : public Object
		{
		public:
			friend class Model;
			friend class Entity;

			LODStage(float distance) :
				_count(0),
				_distance(distance)
			{}

			LODStage(const LODStage *other) :
				_distance(other->_distance),
				_count(other->_count),
				_index(other->_index)
			{
				_meshes.reserve(_count);
				_materials.reserve(_count);
				for(size_t i = 0; i < _count; i++)
				{
					_meshes.push_back(other->_meshes[i]->Retain());
					_materials.push_back(other->_materials[i]->Copy());
				}
			}

			~LODStage()
			{
				for(size_t i = 0; i < _count; i++)
				{
					_meshes[i]->Release();
					_materials[i]->Release();
				}
			}

			void AddMesh(Mesh *mesh, Material *material)
			{
				_meshes.push_back(mesh->Retain());
				_materials.push_back(material->Retain());
				_count = _meshes.size();
			}

			void ReplaceMesh(Mesh *mesh, size_t index)
			{
				_meshes[index]->Autorelease();
				_meshes[index] = mesh->Retain();
			}
			void ReplaceMaterial(Material *material, size_t index)
			{
				_materials[index]->Autorelease();
				_materials[index] = material->Retain();
			}

			Mesh *GetMeshAtIndex(size_t index) const { return _meshes[index]; }
			Material *GetMaterialAtIndex(size_t index) const { return _materials[index]; }

			size_t GetCount() const { return _count; }
			size_t GetIndex() const { return _index; }
			float GetDistance() const { return _distance; }

		private:
			size_t _count;
			float _distance;
			std::vector<Material*> _materials;
			std::vector<Mesh*> _meshes;
			size_t _index;

			__RNDeclareMetaInternal(LODStage)
		};

		RNAPI Model();
		RNAPI Model(Mesh *mesh);
		RNAPI Model(Mesh *mesh, Material *material);
		RNAPI Model(const Model *other);
		RNAPI ~Model();

		RNAPI void Warmup(Camera *camera); //Depending on the renderer and setup, this may initialize some things to speed up the first rendering of the model with the specified camera

		RNAPI static Model *WithName(const String *name, const Dictionary *settings = nullptr);
		RNAPI static Model *WithSkycube(const String *left, const String *front, const String *right, const String *back, const String *up, const String *down);
		RNAPI static Model *WithSkydome(const String *texture);
		RNAPI static Model *WithCube(const RN::Color &color);

		RNAPI LODStage *AddLODStage(float distance);
		RNAPI void RemoveLODStage(size_t index);

		RNAPI LODStage *GetLODStage(size_t index) const;
		RNAPI LODStage *GetLODStageForDistance(float distance) const;

		size_t GetLODStageCount() const
		{
	#if RN_MODEL_LOD_DISABLED
			return _lodStage ? 1 : 0;
	#else
			return _lodStages->GetCount();
	#endif
		}

		RNAPI void SetSkeleton(Skeleton *skeleton);
		RNAPI Skeleton *GetSkeleton() const;

		RNAPI void SetShadowVolume(ShadowVolume *shadowVolume);
		RNAPI ShadowVolume *GetShadowVolume() const;

		RNAPI void CalculateBoundingVolumes();

		const AABB &GetBoundingBox() const { return _boundingBox; }
		const Sphere &GetBoundingSphere() const { return _boundingSphere; }

		RNAPI static const std::vector<float> &GetDefaultLODFactors();
		RNAPI static void SetDefaultLODFactors(const std::vector<float> &factors);

	private:
	#if RN_MODEL_LOD_DISABLED
		LODStage *_lodStage;
	#else
		Array *_lodStages;
	#endif
		Skeleton *_skeleton;
		ShadowVolume *_shadowVolume;

		AABB _boundingBox;
		Sphere _boundingSphere;

		__RNDeclareMetaInternal(Model)
	};
} // namespace RN


#endif /* __RAYNE_MODEL_H_ */
