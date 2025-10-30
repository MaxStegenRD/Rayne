//
//  RNScene.cpp
//  Rayne
//
//  Copyright 2019 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNSceneBasic.h"
#include "../Debug/RNLogger.h"
#include "../Math/RNRandom.h"
#include "../Objects/RNAutoreleasePool.h"
#include "../Rendering/RNMesh.h"
#include "../Rendering/RNModel.h"
#include "../Scene/RNEntity.h"
#include "../Threads/RNWorkGroup.h"
#include "../Threads/RNWorkQueue.h"
#include "../Scene/RNLightManager.h"

#define kRNSceneUpdateBatchSize 8192 //1024
#define kRNSceneRenderBatchSize 32

#define OCCLUSION_FRAMECOUNT 50
#define OCCLUSION_JITTER 0.15f

namespace RN
{
	RNDefineMeta(SceneBasic, Scene)
	RNDefineMeta(SceneBasicInfo, SceneInfo)

    

	SceneBasicInfo::SceneBasicInfo(Scene *scene) :
		SceneInfo(scene), occludedFrameCounter(0)
	{
		ZoneScoped;
	}

    SceneBasic::SceneBasic() :
        _nodesToRemove(new Array()), _currentFrameCount(0)
    {
        ZoneScoped;
        _occlusionCuller = new OcclusionCuller(40, 40);
    }

    SceneBasic::~SceneBasic()
    {
        ZoneScoped;
        _nodesToRemove->Release();
        delete _occlusionCuller;
    }

	void SceneBasic::Update(float delta)
	{
		ZoneScoped;
		WillUpdate(delta);

		WorkQueue *queue = WorkQueue::GetGlobalQueue(WorkQueue::Priority::Default);

		for(size_t i = 0; i < 4; i++)
		{
			ZoneScopedN("Update by Priority");
			WorkGroup *group = nullptr;
			IntrusiveList<SceneNode>::Member *member = _updateNodes[i].GetHead();
			IntrusiveList<SceneNode>::Member *first = member;

			size_t count = 0;

			while(member)
			{
				if(count == kRNSceneUpdateBatchSize)
				{
					if(!group) group = new WorkGroup();
					group->Perform(queue, [&, member, first] {
						ZoneScopedN("Update Batch");

						AutoreleasePool pool;
						auto iterator = first;

						while(iterator != member)
						{
							SceneNode *node = iterator->Get();
							UpdateNode(node, delta);
							iterator = iterator->GetNext();
						}
					});

					first = member;
					count = 0;
				}

				member = member->GetNext();
				count++;
			}

			//Update remaining less than kRNSceneUpdateBatchSize number of nodes
			if(first != member)
			{
				//				group->Perform(queue, [&, member, first] {

				ZoneScopedN("Update Last Batch");

				AutoreleasePool pool;
				auto iterator = first;

				while(iterator != member)
				{
					SceneNode *node = iterator->Get();
					UpdateNode(node, delta);
					iterator = iterator->GetNext();
				}

				//				});
			}

			if(group)
			{
				group->Wait();
				group->Release();
			}
		}

		Scene::Update(delta);
		DidUpdate(delta);

		FlushDeletionQueue();
	}

	void SceneBasic::FlushDeletionQueue()
	{
		ZoneScoped;
		bool didUpdateCameras = false;
		_nodesToRemove->Enumerate<SceneNode>([&](SceneNode *node, size_t index, bool &stop) {
			if(node->IsKindOfClass(Camera::GetMetaClass()))
			{
				Camera *camera = static_cast<Camera *>(node);
				_cameras.Erase(camera->_cameraSceneEntry);
				didUpdateCameras = true;
			}
			else if(node->IsKindOfClass(Light::GetMetaClass()))
			{
				Light *light = static_cast<Light *>(node);
				_lights.Erase(light->_lightSceneEntry);
			}
			else
			{
				RemoveRenderNode(node);
			}

			if(node->GetUpdatePriority() != SceneNode::UpdatePriority::UpdateNever)
			{
				_updateNodes[static_cast<size_t>(node->GetUpdatePriority())].Erase(node->_sceneUpdateEntry);
			}

			node->UpdateSceneInfo(nullptr);
			node->_scheduledForRemovalFromScene = false; //Make sure it can be added and removed again, if the object doesn't actually get deleted here!
			node->Autorelease(); //Autorelease here causes the constructors to be called later (otherwise RemoveAllObjects below would do it), this will cause objects removed from the scene in the destructor to be removed in the next frame, working around some issue I've been having otherwise
			//TODO: Somehow make objects removed in the destructor also be deleted here
		});

		_nodesToRemove->RemoveAllObjects();
	}

    

	void SceneBasic::Render(Renderer *renderer)
	{
		ZoneScoped;
		WillRender(renderer);

		//Run camera PostUpdate once for each camera
		IntrusiveList<Camera>::Member *cameraMember = _cameras.GetHead();
		while(cameraMember)
		{
			Camera *camera = cameraMember->Get();
			camera->PostUpdate();
			cameraMember = cameraMember->GetNext();
		}

		for(int cameraPriority = 0; cameraPriority < 3; cameraPriority++)
		{
			cameraMember = _cameras.GetHead();
			while(cameraMember)
			{
				ZoneScoped;
				Camera *camera = cameraMember->Get();

				//Early out if camera is not supposed to render or if this isn't it's priority loop
				if(camera->GetFlags() & Camera::Flags::NoRender || (cameraPriority == 0 && !(camera->GetFlags() & Camera::Flags::RenderEarly)) || (cameraPriority == 1 && (camera->GetFlags() & Camera::Flags::RenderEarly || camera->GetFlags() & Camera::Flags::RenderLate)) || (cameraPriority == 2 && !(camera->GetFlags() & Camera::Flags::RenderLate)))
				{
					cameraMember = cameraMember->GetNext();
					continue;
				}

				//Multiview cameras need to be skipped, they are rendered through their parent camera
				if(camera->GetIsMultiviewCamera())
				{
					cameraMember = cameraMember->GetNext();
					continue;
				}

				std::vector<SceneNode *> occluders;
				std::vector<SceneNode *> sceneNodesToRender;
				size_t firstTransparentIndex = 0;
				size_t lastTransparentIndex = 0;

				occluders.reserve(_renderNodes.GetCount());
				sceneNodesToRender.reserve(_renderNodes.GetCount());

				IntrusiveList<SceneNode>::Member *firstNodeMember = camera->_firstNodeMember ? camera->_firstNodeMember : _renderNodes.GetHead();
				IntrusiveList<SceneNode>::Member *nodeMember = firstNodeMember;
				if(!(camera->GetFlags() & Camera::Flags::NoOcclusionCulling) && !camera->GetRenderNodes())
				{
					ZoneScopedN("Collect Occluders");
					const RN::Vector3 cameraWorldPosition = camera->GetWorldPosition();
					//Collect all occluders
					while(nodeMember)
					{
						SceneNode *node = nodeMember->Get();
						if(node->GetRenderPriority() >= SceneNode::RenderPriority::RenderSky) break;

						if(node->HasFlags(SceneNode::Flags::Occluder) && node->CanRender(renderer, camera))
						{
							SceneBasicInfo *sceneInfo = static_cast<SceneBasicInfo *>(node->GetSceneInfo());
							sceneInfo->occluderDistance = std::max(node->GetWorldPosition().GetSquaredDistance(cameraWorldPosition), 1.0f);
							sceneInfo->occluderSize = node->GetBoundingSphere().radius * node->GetBoundingSphere().radius / sceneInfo->occluderDistance;
							sceneInfo->isActiveOccluder = false;
							occluders.push_back(node);
						}

						nodeMember = nodeMember->GetNext();
					}

					nodeMember = firstNodeMember;
				}

				//Do occlusion culling if there are 1 or more occluders!
				if(occluders.size() > 0)
				{
					ZoneScopedN("Collect Entities with Occlusion Culling");
					{
						ZoneScopedN("Find 30 biggest occluders");

						//Sort occluders by approximated size on the screen
						int clampedCount = std::min(static_cast<int>(occluders.size()), 30);
						std::nth_element(occluders.begin(), occluders.begin() + clampedCount, occluders.end(), [](SceneNode *a, SceneNode *b) {
							SceneBasicInfo *sceneInfoA = static_cast<SceneBasicInfo *>(a->GetSceneInfo());
							SceneBasicInfo *sceneInfoB = static_cast<SceneBasicInfo *>(b->GetSceneInfo());
							return sceneInfoA->occluderSize > sceneInfoB->occluderSize;
						});

						occluders.resize(std::min(static_cast<size_t>(30), occluders.size())); //Only keep the biggest 30 occluders in the list

						//Sort remaining occluders front to back
						std::sort(occluders.begin(), occluders.end(), [](SceneNode *a, SceneNode *b) {
							SceneBasicInfo *sceneInfoA = static_cast<SceneBasicInfo *>(a->GetSceneInfo());
							SceneBasicInfo *sceneInfoB = static_cast<SceneBasicInfo *>(b->GetSceneInfo());
							return sceneInfoA->occluderDistance < sceneInfoB->occluderDistance;
						});
					}

                    //Clear occlusion depth map
                    _occlusionCuller->Clear();

					Vector2 screenPixelSize = Vector2(1.0f / camera->GetRenderPass()->GetFrame().width, 1.0f / camera->GetRenderPass()->GetFrame().height);

					Vector3 randomCameraOffset = RandomNumberGenerator::GetSharedGenerator()->GetRandomVector3Range(RN::Vector3(-OCCLUSION_JITTER, -OCCLUSION_JITTER, 0.0f), RN::Vector3(OCCLUSION_JITTER, OCCLUSION_JITTER, 0.0f));
					Matrix matViewProj = camera->GetProjectionMatrix() * Matrix::WithTranslation(randomCameraOffset) * camera->GetViewMatrix();
					if(camera->GetIsMultiviewCamera())
					{
						size_t multiviewIndex = _currentFrameCount % camera->GetMultiviewCameras()->GetCount();
						RN::Camera *multiviewCamera = camera->GetMultiviewCameras()->GetObjectAtIndex<RN::Camera>(multiviewIndex);
						matViewProj = multiviewCamera->GetProjectionMatrix() * multiviewCamera->GetViewMatrix();
					}

					{
						ZoneScopedN("Render Occluder Depth");

						//Render occluders to depth buffer first (first test if the bounding box is visible at all)
						for(SceneNode *node : occluders)
						{
                            bool testResult = _occlusionCuller->TestBoundingBox(matViewProj, node->GetBoundingBox(), screenPixelSize);
							SceneBasicInfo *sceneInfo = static_cast<SceneBasicInfo *>(node->GetSceneInfo());
							sceneInfo->isActiveOccluder = true;
							if(!testResult && sceneInfo->occludedFrameCounter < 1000)
							{
								sceneInfo->occludedFrameCounter += 1;
							}
							if(testResult || sceneInfo->occludedFrameCounter < OCCLUSION_FRAMECOUNT)
							{
								if(testResult)
								{
									sceneInfo->occludedFrameCounter = 0;
								}
							}

							if(testResult)
							{
								RN::Entity *entity = node->Downcast<Entity>();
								if(entity)
								{
									Matrix matModelViewProj = matViewProj * node->GetWorldTransform();

									Model *model = entity->GetModel();
									RN::Model::LODStage *lodStage = model->GetLODStage(0);
									for(int i = 0; i < lodStage->GetCount(); i++)
									{
                                        RN::Mesh *mesh = lodStage->GetMeshAtIndex(i);
                                        _occlusionCuller->RasterizeMesh(matModelViewProj, mesh);
									}
								}
							}
						}
					}

					{
						ZoneScopedN("Test Objects");

						//Test all visible objects against depth buffer
						while(nodeMember)
						{
							SceneNode *node = nodeMember->Get();
							nodeMember = nodeMember->GetNext();
							if(!node->CanRender(renderer, camera)) continue;
							if(node->GetRenderPriority() >= SceneNode::RenderSky)
							{
								if(node->GetRenderPriority() == SceneNode::RenderTransparent)
								{
									if(firstTransparentIndex == 0)
									{
										firstTransparentIndex = sceneNodesToRender.size();
									}
									lastTransparentIndex = sceneNodesToRender.size();
								}
								sceneNodesToRender.push_back(node);
								continue;
							}

							if(node->GetFlags() & SceneNode::Flags::Occluder)
							{
								SceneBasicInfo *sceneInfo = static_cast<SceneBasicInfo *>(node->GetSceneInfo());
								if(sceneInfo->isActiveOccluder && sceneInfo->occludedFrameCounter < OCCLUSION_FRAMECOUNT)
								{
									sceneNodesToRender.push_back(node);
									continue;
								}
							}

                            bool testResult = _occlusionCuller->TestBoundingBox(matViewProj, node->GetBoundingBox(), screenPixelSize);
							SceneBasicInfo *sceneInfo = static_cast<SceneBasicInfo *>(node->GetSceneInfo());
							if(!testResult && sceneInfo->occludedFrameCounter < 1000)
							{
								sceneInfo->occludedFrameCounter += 1;
							}
							if(testResult || sceneInfo->occludedFrameCounter < OCCLUSION_FRAMECOUNT)
							{
								if(testResult)
								{
									sceneInfo->occludedFrameCounter = 0;
								}

								sceneNodesToRender.push_back(node);
							}
						}
					}
				}
				else if(camera->GetFlags() & Camera::Flags::UseUIFastPath)
				{
					ZoneScopedN("Collect UI Entities");
					while(nodeMember)
					{
						SceneNode *node = nodeMember->Get();
						if((node->GetRenderGroup() & camera->GetRenderGroup()) == 0 || node->HasFlags(RN::SceneNode::Flags::Hidden))
						{
							nodeMember = nodeMember->GetNext();
							continue;
						}

						if(node->GetRenderPriority() == SceneNode::RenderTransparent)
						{
							if(firstTransparentIndex == 0)
							{
								firstTransparentIndex = sceneNodesToRender.size();
							}
							lastTransparentIndex = sceneNodesToRender.size();
						}
						sceneNodesToRender.push_back(node);

						nodeMember = nodeMember->GetNext();
					}
				}
				else
				{
					ZoneScopedN("Collect Entities");
					if(camera->GetRenderNodes())
					{
						camera->GetRenderNodes()->Enumerate<SceneNode>([&](SceneNode *node, size_t index, bool &stop) {
							//if(node->CanRender(renderer, camera))
							{
								if(node->GetRenderPriority() == SceneNode::RenderTransparent)
								{
									if(firstTransparentIndex == 0)
									{
										firstTransparentIndex = sceneNodesToRender.size();
									}
									lastTransparentIndex = sceneNodesToRender.size();
								}
								sceneNodesToRender.push_back(node);
							}
						});
					}
					else
					{
						while(nodeMember)
						{
							SceneNode *node = nodeMember->Get();
							if(node->CanRender(renderer, camera))
							{
								if(node->GetRenderPriority() == SceneNode::RenderTransparent)
								{
									if(firstTransparentIndex == 0)
									{
										firstTransparentIndex = sceneNodesToRender.size();
									}
									lastTransparentIndex = sceneNodesToRender.size();
								}
								sceneNodesToRender.push_back(node);
							}
							
							nodeMember = nodeMember->GetNext();
						}
					}
				}

				if(camera->GetFlags() & Camera::Flags::SortFrontToBack)
				{
					ZoneScopedN("Sort Opaque");
					const RN::Vector3 cameraWorldPosition = camera->GetWorldPosition();
					std::sort(sceneNodesToRender.begin(), sceneNodesToRender.end(), [cameraWorldPosition](SceneNode *a, SceneNode *b) {
						if(a->GetRenderPriority() == b->GetRenderPriority() && b->GetRenderPriority() < SceneNode::RenderSky)
						{
							return a->GetWorldPosition().GetSquaredDistance(cameraWorldPosition) < b->GetWorldPosition().GetSquaredDistance(cameraWorldPosition);
						}
						return a->GetRenderPriority() < b->GetRenderPriority();
					});
				}

				if(camera->GetFlags() & Camera::Flags::SortTransparentBackToFront && firstTransparentIndex < lastTransparentIndex)
				{
					ZoneScopedN("Sort Transparent");
					const RN::Vector3 cameraWorldPosition = camera->GetWorldPosition();
					std::sort(sceneNodesToRender.begin() + firstTransparentIndex, sceneNodesToRender.begin() + lastTransparentIndex + 1, [cameraWorldPosition](SceneNode *a, SceneNode *b) {
						return a->GetWorldPosition().GetSquaredDistance(cameraWorldPosition) > b->GetWorldPosition().GetSquaredDistance(cameraWorldPosition);
					});
				}

				//RNInfo("Number of objects: " << sceneNodesToRender.size());

				renderer->SubmitCamera(camera, [&] {
					ZoneScopedN("Submit Drawables");
					//TODO: Add back some multithreading while not breaking the priorities.

					//Submit lights first
					IntrusiveList<Light>::Member *lightMember = _lights.GetHead();
					std::vector<Light *> visibleLights;
					while(lightMember)
					{
						Light *light = lightMember->Get();
						if(light->CanRender(renderer, camera))
						{
							visibleLights.push_back(light);
							light->Render(renderer, camera);
						}

						lightMember = lightMember->GetNext();
					}

					if(LightManager *lm = camera->GetLightManager())
					{
						lm->BuildForCamera(camera, visibleLights);
					}

					//Submit all drawables for rendering
					for(SceneNode *node : sceneNodesToRender)
					{
						node->Render(renderer, camera);
					}
				});

				cameraMember = cameraMember->GetNext();
			}
		}

		_currentFrameCount += 1;
		_currentFrameCount %= 10000;

		DidRender(renderer);
	}

	void SceneBasic::AddRenderNode(SceneNode *node)
	{
		ZoneScoped;

		Lock();
		int32 renderPriority = node->GetRenderPriority();
		if(!_renderNodes.GetHead() || _renderNodes.GetHead()->Get()->GetRenderPriority() >= renderPriority)
		{
			_renderNodes.PushFront(node->_sceneRenderEntry);
		}
		else if(!_renderNodes.GetTail() || _renderNodes.GetTail()->Get()->GetRenderPriority() <= renderPriority)
		{
			_renderNodes.PushBack(node->_sceneRenderEntry);
		}
		else
		{
			IntrusiveList<SceneNode>::Member *member = _renderNodes.GetTail();
			while(member->Get()->GetRenderPriority() > renderPriority)
			{
				member = member->GetPrevious();
			}

			_renderNodes.InsertAfter(node->_sceneRenderEntry, member);
		}

		Unlock();
	}

	void SceneBasic::RemoveRenderNode(SceneNode *node)
	{
		ZoneScoped;
		_renderNodes.Erase(node->_sceneRenderEntry);
	}

	void SceneBasic::AddNode(SceneNode *node)
	{
		ZoneScoped;
		//Remove from deletion list if scheduled for deletion if the scene didn't change.
		if(node->GetSceneInfo() && node->GetSceneInfo()->GetScene() == this && node->_scheduledForRemovalFromScene)
		{
			_nodesToRemove->Lock();
			_nodesToRemove->RemoveObject(node);
			_nodesToRemove->Unlock();

			//Remove and insert at the correct position, respecting the render priority.
			Lock();
			SceneInfo *sceneInfo = node->GetSceneInfo();
			sceneInfo->Retain();
			RemoveRenderNode(node);
			node->UpdateSceneInfo(nullptr);
			Unlock();

			node->UpdateSceneInfo(sceneInfo);
			AddRenderNode(node);
			sceneInfo->Release();

			node->_scheduledForRemovalFromScene = false; //Do this last, so scene change callbacks can check if this is just readding or completely new

			return;
		}

		RN_ASSERT(node->GetSceneInfo() == nullptr, "AddNode() must be called on a Node not owned by a scene");

		node->Retain();
		SceneBasicInfo *sceneInfo = new SceneBasicInfo(this);
		node->UpdateSceneInfo(sceneInfo);
		sceneInfo->Release();

		if(node->IsKindOfClass(Camera::GetMetaClass()))
		{
			Camera *camera = static_cast<Camera *>(node);
			_cameras.PushFront(camera->_cameraSceneEntry);
		}
		else if(node->IsKindOfClass(Light::GetMetaClass()))
		{
			Light *light = static_cast<Light *>(node);
			_lights.PushFront(light->_lightSceneEntry);
		}
		else
		{
			AddRenderNode(node);
		}

		//Lock to prevent race condition of multiple threads adding nodes at the same time
		Lock();
		//PushFront to prevent race condition with scene iterating over the nodes.
		if(node->GetUpdatePriority() != SceneNode::UpdatePriority::UpdateNever)
		{
			_updateNodes[static_cast<size_t>(node->GetUpdatePriority())].PushFront(node->_sceneUpdateEntry);
		}
		Unlock();
	}

	void SceneBasic::RemoveNode(SceneNode *node)
	{
		ZoneScoped;
		RN_ASSERT(node->GetSceneInfo() && node->GetSceneInfo()->GetScene() == this && node->_scheduledForRemovalFromScene == false, "RemoveNode() must be called on a Node owned by the scene");

		_nodesToRemove->Lock();
		_nodesToRemove->AddObject(node);
		_nodesToRemove->Unlock();
		node->_scheduledForRemovalFromScene = true;
	}
} // namespace RN
