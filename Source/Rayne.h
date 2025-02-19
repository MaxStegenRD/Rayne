//
//  Rayne.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_RAYNE_H__
#define __RAYNE_RAYNE_H__

#include <tracy/Tracy.hpp>

#include "Base/RNApplication.h"
#include "Base/RNArgumentParser.h"
#include "Base/RNBase.h"
#include "Base/RNKernel.h"
#include "Base/RNMemoryPool.h"
#include "Base/RNNotification.h"
#include "Base/RNNotificationManager.h"
#include "Base/RNSettings.h"
#include "Base/RNUnicode.h"

#include "Assets/RNAsset.h"
#include "Assets/RNAssetLoader.h"
#include "Assets/RNAssetManager.h"
#include "Assets/RNAudioAsset.h"
#include "Assets/RNBitmap.h"

#include "Assets/RNPNGAssetWriter.h"

#include "Data/RNAny.h"
#include "Data/RNAtomicRingBuffer.h"
#include "Data/RNIntrusiveList.h"

#include "Debug/RNLogFormatter.h"
#include "Debug/RNLogger.h"
#include "Debug/RNLoggingEngine.h"

#include "Input/RNHIDDevice.h"
#include "Input/RNInputControl.h"
#include "Input/RNInputDevice.h"
#include "Input/RNInputManager.h"

#include "Math/RNAABB.h"
#include "Math/RNAlgorithm.h"
#include "Math/RNColor.h"
#include "Math/RNConstants.h"
#include "Math/RNHalfVector.h"
#include "Math/RNInterpolation.h"
#include "Math/RNMath.h"
#include "Math/RNMatrix.h"
#include "Math/RNMatrixQuaternion.h"
#include "Math/RNQuaternion.h"
#include "Math/RNRandom.h"
#include "Math/RNSphere.h"
#include "Math/RNVector.h"

#include "Modules/RNExtensionPoint.h"
#include "Modules/RNModule.h"
#include "Modules/RNModuleManager.h"

#include "Objects/RNArray.h"
#include "Objects/RNAutoreleasePool.h"
#include "Objects/RNCatalogue.h"
#include "Objects/RNCharacterSet.h"
#include "Objects/RNCountedSet.h"
#include "Objects/RNDictionary.h"
#include "Objects/RNJSONSerialization.h"
#include "Objects/RNKVO.h"
#include "Objects/RNNull.h"
#include "Objects/RNNumber.h"
#include "Objects/RNObject.h"
#include "Objects/RNRingBuffer.h"
#include "Objects/RNSerialization.h"
#include "Objects/RNSet.h"
#include "Objects/RNString.h"
#include "Objects/RNValue.h"

#include "Rendering/RNFramebuffer.h"
#include "Rendering/RNGPUBuffer.h"
#include "Rendering/RNGPUResource.h"
#include "Rendering/RNMaterial.h"
#include "Rendering/RNMesh.h"
#include "Rendering/RNModel.h"
#include "Rendering/RNPostProcessing.h"
#include "Rendering/RNRenderPass.h"
#include "Rendering/RNRenderer.h"
#include "Rendering/RNRendererDescriptor.h"
#include "Rendering/RNRenderingDevice.h"
#include "Rendering/RNShader.h"
#include "Rendering/RNShaderLibrary.h"
#include "Rendering/RNSkeleton.h"
#include "Rendering/RNWindow.h"

#include "Scene/RNCamera.h"
#include "Scene/RNEntity.h"
#include "Scene/RNLight.h"
#include "Scene/RNParticleEmitter.h"
#include "Scene/RNScene.h"
#include "Scene/RNSceneAttachment.h"
#include "Scene/RNSceneBasic.h"
#include "Scene/RNSceneManager.h"
#include "Scene/RNSceneNode.h"
#include "Scene/RNSceneNodeAttachment.h"
#include "Scene/RNSceneWithVisibilityLists.h"
#include "Scene/RNVoxelEntity.h"

#include "System/RNFile.h"
#include "System/RNFileManager.h"
#include "System/RNScreen.h"

#include "Threads/RNCondition.h"
#include "Threads/RNLockTools.h"
#include "Threads/RNLockWrapper.h"
#include "Threads/RNLockable.h"
#include "Threads/RNRecursiveLockable.h"
#include "Threads/RNRunLoop.h"
#include "Threads/RNSemaphore.h"
#include "Threads/RNThread.h"
#include "Threads/RNThreadLocalStorage.h"
#include "Threads/RNWorkGroup.h"
#include "Threads/RNWorkQueue.h"

#endif /* _RAYNE_RAYNE_H__ */
