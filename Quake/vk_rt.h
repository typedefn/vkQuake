#ifndef _VK_RT_H
#define _VK_RT_H

#include "quakedef.h"

#define MAX_VK_GEOMETRY 50

uint32_t queue_index;


struct Camera {
  float position[4];
  float right[4];
  float up[4];
  float forward[4];

  uint32_t frameCount;
};

typedef struct accel_khr_s {
  VkAccelerationStructureKHR accel;
  VkBuffer buffer;
} accel_khr_t;

typedef struct blasinput_s {
  VkAccelerationStructureGeometryKHR as_geometry[MAX_VK_GEOMETRY];
  VkAccelerationStructureBuildRangeInfoKHR as_build_offset_info[MAX_VK_GEOMETRY];
  VkBuildAccelerationStructureFlagsKHR flags[MAX_VK_GEOMETRY];
  size_t current_index;
} blasinput_t;

typedef struct buffer_s {
  VkBuffer buffer;
} buffer_t;

typedef struct rtx_s {
  VkDeviceMemory vertexPositionBufferMemory;
  VkBuffer vertexPositionBuffer;
  VkCommandPool commandPool;

  VkDescriptorSet materialDescriptorSet;
  VkBuffer materialIndexBuffer;
  VkDeviceMemory materialIndexBufferMemory;
  VkBuffer materialBuffer;

  VkImageView rayTraceImageView;
  VkImage rayTraceImage;
  VkDeviceMemory rayTraceImageMemory;

  VkAccelerationStructureKHR bottomLevelAccelerationStructure;
  VkBuffer bottomLevelAccelerationStructureBuffer;
  VkDeviceMemory bottomLevelAccelerationStructureBufferMemory;

  VkAccelerationStructureKHR topLevelAccelerationStructure;
  VkBuffer topLevelAccelerationStructureBuffer;
  VkDeviceMemory topLevelAccelerationStructureBufferMemory;

  VkDescriptorSetLayout* rayTraceDescriptorSetLayouts;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet rayTraceDescriptorSet;

  VkPipelineLayout rayTracePipelineLayout;
  VkPipeline rayTracePipeline;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;

  VkBuffer uniformBuffer;
  VkDeviceMemory uniformBufferMemory;

} rtx_t;

typedef struct build_acceleration_structure_s {
  VkAccelerationStructureBuildGeometryInfoKHR build_info;
  VkAccelerationStructureBuildSizesInfoKHR size_info;
  VkAccelerationStructureBuildRangeInfoKHR offset_info;
  accel_khr_t as;
  accel_khr_t cleanup_as;
} build_acceleration_structure_t;

// Bottom-level acceleration structure

typedef struct blas_s {
  VkAccelerationStructureNV accel;
  VkAccelerationStructureInfoNV as_info; //{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV, nullptr, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV};
  VkGeometryNV geometry;
} blas_t;

typedef struct mem_info_s {
  VkDeviceMemory memory;
  VkDeviceSize offset;
  VkDeviceSize size;
} mem_info_t;

extern blasinput_t blas_inputs;
extern blasinput_t *all_blas;

void VK_Init();
void VK_Destroy();
void createCommandPool();

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, int copy_offset);
void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags, VkBuffer *buffer, VkDeviceMemory *bufferMemory);
void createVertexBuffer(qmodel_t *m, const aliashdr_t *hdr);
byte * readFile(char * filename, size_t * filesize);
void createDescriptorSets();
void createUniformBuffer();

#endif

