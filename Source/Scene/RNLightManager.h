//
//  RNLightManager.h
//  Rayne
//
//  Copyright 2025 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_LIGHT_MANAGER_H__
#define __RAYNE_LIGHT_MANAGER_H__

#include "RNSceneNode.h"
#include "RNCamera.h"
#include "RNLight.h"
#include "../Rendering/RNGPUBuffer.h"

namespace RN
{
    class LightManager : public Object
    {
    public:
        struct ClusterRecord
        {
            //Each cluster record on the GPU needs to be 16 bytes aligned, so fill in with lots of padding for now
            uint32 offset;
            uint8 pointCount;
            uint8 spotCount;
            uint16 pad0;
            uint32 pad1;
            uint32 pad2;
            //TODO: Use one shared offset and store the counts for 6 clusters in the remaining bytes
        };

        struct PointLightPacked
        {
            Vector4 positionRange; // xyz = world position, w = range
            Vector4 color;         // xyz = color*intensity, w unused
        };

        struct SpotLightPacked
        {
            Vector4 positionRange; // xyz = world position, w = range
            Vector4 color;         // xyz = color*intensity, w unused
            Vector4 dirCos;        // xyz = direction, w = cos(angle)
        };

        struct ClusterGridInfo
        {
            uint32 clustersX;
            uint32 clustersY;
            uint32 clustersZ;
            uint32 pad0;
            float zFirstSliceDepth; //meters from near; if > 0, slice 0 is fixed depth
            float zLogFactor; //0..1 blend between linear/log slicing
            float clipNear;
            float clipFar;
            float viewportWidth;
            float viewportHeight;
            float projA;
            float projB;
        };

        RNAPI LightManager();
        RNAPI ~LightManager() override;

        RNAPI void SetClusterGridInfo(uint32 x, uint32 y, uint32 z, float zLogFactor = 0.0f);
        RNAPI void SetZLogFactor(float zLogFactor);
        RNAPI void SetZFirstSliceDepth(float meters);
        RNAPI const ClusterGridInfo &GetClusterGridInfo() const { return _grid; }

        // Call each frame before rendering drawables for a camera
        RNAPI void BuildForCamera(Camera *camera, const std::vector<Light *> &lights);

        // GPU buffers for shaders (optional; backends may fetch CPU data instead)
        RNAPI GPUBuffer *GetPointLightBuffer() const { return _pointLightBuffer; }
        RNAPI GPUBuffer *GetSpotLightBuffer() const { return _spotLightBuffer; }
        RNAPI GPUBuffer *GetClusterIndexBuffer() const { return _clusterIndexBuffer; }
        RNAPI GPUBuffer *GetClusterRecordsBuffer() const { return _clusterRecordsBuffer; }

        // CPU accessors (for debugging or CPU-driven pipelines)
        RNAPI const std::vector<PointLightPacked> &GetPackedPointLights() const { return _packedPointLights; }
        RNAPI const std::vector<SpotLightPacked> &GetPackedSpotLights() const { return _packedSpotLights; }
        RNAPI const std::vector<uint16_t> &GetClusterLightIndices() const { return _clusterLightIndices; }
        RNAPI const std::vector<ClusterRecord> &GetClusterRecords() const { return _clusterRecords; }

        RNAPI void SetMaxLightsPerCluster(uint16_t max);
        uint16_t GetMaxLightsPerCluster() const { return _maxLightsPerCluster; }

    private:
        void ClearData();
        void PackLights(const std::vector<Light *> &lights);
        void BuildClusters(Camera *camera);
        void UploadBuffers();
        void PreallocateBuffers(uint32 pointEstimate, uint32 spotEstimate, uint16 pointPerClusterEstimate, uint16 spotPerClusterEstimate);

        // Helpers
        uint32 ComputeClusterCount() const { return _grid.clustersX * _grid.clustersY * _grid.clustersZ; }
        uint32 EncodeClusterIndex(uint32 x, uint32 y, uint32 z) const { return (z * _grid.clustersY + y) * _grid.clustersX + x; }
        float ComputeZSlice(const Camera *camera, float viewZ) const;

        ClusterGridInfo _grid;

        std::vector<PointLightPacked> _packedPointLights;
        std::vector<SpotLightPacked> _packedSpotLights;

        std::vector<uint16_t> _clusterLightIndices; // point indices then spot indices per cluster
        std::vector<ClusterRecord> _clusterRecords;

        GPUBuffer *_pointLightBuffer;
        GPUBuffer *_spotLightBuffer;
        GPUBuffer *_clusterIndexBuffer;
        GPUBuffer *_clusterRecordsBuffer;

        // Cached per-camera parameters for cluster header
        float _lastViewportWidth;
        float _lastViewportHeight;
        float _lastProjA;
        float _lastProjB;
        float _lastClipNear;
        float _lastClipFar;

        uint16_t _maxLightsPerCluster;

        __RNDeclareMetaInternal(LightManager)
    };
}

#endif


