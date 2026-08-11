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

		RNAPI void AddNode(SceneNode *node, uint8 maxDepth);
		RNAPI void AddNode(SceneNode *node) override;
		RNAPI void RemoveNode(SceneNode *node) override;

		RNAPI void RelocateNodeIfNeeded(SceneNode *node);

	protected:
		RNAPI SceneQuadtree(const AABB &worldBounds, float minNodeSize = 8.0f);

		RNAPI void Update(float delta) override;
		RNAPI void Render(Renderer *renderer) override;
		RNAPI void PrepareForShutdown() override;

		RNAPI void TraverseTree(RN::Camera *camera, std::vector<SceneNode *> &sceneNodesToRender);
		RNAPI void FlushAdditionQueue();
		RNAPI void FlushDeletionQueue();

		RNAPI void AddRenderNode(SceneNode *node);
		RNAPI void RemoveRenderNode(SceneNode *node);

		RNAPI void MakeDrawablesDirty();

		//Should probably be private, but this makes it easy to visualize
		OcclusionCuller *_occlusionCuller;

	private:
		class TreeBounds
		{
		public:
			TreeBounds() = default;
			TreeBounds(const Vector3 &min, const Vector3 &max) : min(min), max(max) {}

			Vector3 min;
			Vector3 max;
		};

		class TreeNode
		{
		public:
			bool Contains(const TreeBounds &box) const
			{
				return Contains(box.min) && Contains(box.max);
			}
			bool Contains(const Vector3 &position) const
			{
				//Check if the position is inside the bounds, ignoring the y bounds
				if(position.x > bounds.max.x)
					return false;
				if(position.x < bounds.min.x)
					return false;
				if(position.z > bounds.max.z)
					return false;
				if(position.z < bounds.min.z)
					return false;
				
				return true;
			}

			TreeBounds bounds;
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

		void RemoveAllNodes();
		TreeBounds GetSceneNodeTreeBounds(const SceneNode *node) const;
		uint32 FindTreeNode(const TreeBounds &box, bool isInserting, uint8 maxDepth = UINT8_MAX);

		PositionType _treeOrigin;
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
		uint8 maxDepth;

		__RNDeclareMetaInternal(SceneQuadtreeInfo)
	};
} // namespace RN


#endif /* __RAYNE_SCENEQUADTREE_H__ */
