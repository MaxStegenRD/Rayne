//
//  RNSceneQuadtree.h
//  Rayne
//
//  Copyright 2025 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//
 
#ifndef __RAYNE_SCENEQUADTREE_H__
#define __RAYNE_SCENEQUADTREE_H__

#include "RNScene.h"
#include "RNOcclusionCuller.h"

namespace RN
{
	class SceneQuadtree : public Scene
	{
	public:
		RNAPI ~SceneQuadtree();

		RNAPI void AddNode(SceneNode *node) override;
		RNAPI void RemoveNode(SceneNode *node) override;

	protected:
		RNAPI SceneQuadtree(AABB worldBounds, float minNodeSize = 8.0f);

		RNAPI void Update(float delta) override;
		RNAPI void Render(Renderer *renderer) override;

		RNAPI void TraverseTree(RN::Camera *camera, std::vector<SceneNode *> &sceneNodesToRender);
		RNAPI uint32 FindTreeNode(const AABB& box, bool isInserting);

		RNAPI void FlushAdditionQueue();
		RNAPI void FlushDeletionQueue();

		RNAPI void AddRenderNode(SceneNode *node);
		RNAPI void RemoveRenderNode(SceneNode *node);

		RNAPI void MakeDrawablesDirty();

		//Should probably be private, but this makes it easy to visualize
		OcclusionCuller *_occlusionCuller;

	private:
		class TreeNode
		{
		public:
			bool Contains(const AABB& box) const
			{
				return Contains(box.minExtend + box.position) && Contains(box.maxExtend + box.position);
			}
			bool Contains(const Vector3& position) const
			{
				//Check if the position is inside the bounds, ignoring the y bounds
				if(position.x - bounds.position.x > bounds.maxExtend.x)
					return false;
				if(position.x - bounds.position.x < bounds.minExtend.x)
					return false;
				if(position.z - bounds.position.z > bounds.maxExtend.z)
					return false;
				if(position.z - bounds.position.z < bounds.minExtend.z)
					return false;
				
				return true;
			}

			AABB bounds;
			uint32 firstChild = UINT32_MAX;
			uint32 numberOfObjects = 0;
			std::vector<SceneNode *> objects;
		};
		std::vector<TreeNode> _treeNodes;

		IntrusiveList<SceneNode> _updateNodes[4];
		IntrusiveList<Light> _lights;
		IntrusiveList<Camera> _cameras;
		Array *_nodesToRemove;
		Array *_nodesToAdd;

		size_t _currentFrameCount;

		__RNDeclareMetaInternal(SceneQuadtree)
	};

	class SceneQuadtreeInfo : public SceneInfo
	{
	public:
		SceneQuadtreeInfo(Scene *scene);

		uint16 occludedFrameCounter;
		float occluderSize;
		float occluderDistance;
		bool isActiveOccluder;
		bool isVisibleOccluder;

		uint32 quadtreeNodeIndex;

		__RNDeclareMetaInternal(SceneQuadtreeInfo)
	};
} // namespace RN


#endif /* __RAYNE_SCENEQUADTREE_H__ */


