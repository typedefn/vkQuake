//vk_rt.c - wrapping vulkan ray tracing

#include "vk_rt.h"
blasinput_t blas_inputs;
blasinput_t *all_blas;

/*
 ================
 Staging
 ================
 */
#define NUM_STAGING_BUFFERS   2

void VK_Init() {
  all_blas = (blasinput_t*) malloc(sizeof(blasinput_t) * MAX_MODELS);
}

void VK_Destroy() {
  free(all_blas);
}

void createAcceleration(VkAccelerationStructureCreateInfoNV *accel_,
    VkAccelerationStructureNV *result_accel) {

  // Create the acceleration structure
  fpCreateAccelerationStructureNV(vulkan_globals.device, accel_, NULL,
      result_accel);

  // Find memory requirements
  VkAccelerationStructureMemoryRequirementsInfoNV accel_mem_info;
  memset(&accel_mem_info, 0, sizeof(accel_mem_info));
  accel_mem_info.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
  accel_mem_info.accelerationStructure = *(result_accel);
  VkMemoryRequirements2 mem_reqs;
  memset(&mem_reqs, 0, sizeof(mem_reqs));
  mem_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

  fpGetAccelerationStructureMemoryRequirementsNV(vulkan_globals.device,
      &accel_mem_info, &mem_reqs);

//  // Allocate memory
//  MemAllocateInfo
//  info(mem_reqs.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
//

  struct mem_info_s mem_info;
  VkMemoryAllocateInfo mem_alloc_info;
  memset(&mem_alloc_info, 0, sizeof(mem_alloc_info));
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

  VkResult result = vkAllocateMemory(vulkan_globals.device, &mem_alloc_info, NULL,
      &(mem_info.memory));

  if (result != VK_SUCCESS) {
    Sys_Error("Could not vkAllocateMemory");
    return;
  }

  // Bind memory with acceleration structure
  VkBindAccelerationStructureMemoryInfoNV bind;
  bind.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
  bind.accelerationStructure = *(result_accel);
  bind.memory = mem_info.memory;
  bind.memoryOffset = mem_info.offset;
  fpBindAccelerationStructureMemoryNV(vulkan_globals.device, 1, &bind);

  vkFreeMemory(vulkan_globals.device, mem_info.memory, NULL);
}

VkDeviceAddress Get_Buffer_Device_Address(VkDevice device, VkBuffer buffer) {
  VkBufferDeviceAddressInfo info;
  memset(&info, 0, sizeof(info));
  info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  info.buffer = buffer;
  return vkGetBufferDeviceAddress(device, &info);
}

struct buffer_s Create_Buffer(VkDeviceSize size, const void *data,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props) {
//  Need this command buffer
//  VkCommandBuffer * cmd_buf = &command_buffers[current_command_buffer];
  VkBufferCreateInfo create_info;
  int remaining_size;
  memset(&create_info, 0, sizeof(create_info));
  create_info.size = size;
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  struct buffer_s result_buffer;

//  vkCreateBuffer(vulkan_globals.device, &create_info, NULL,
//      result_buffer.buffer);

  const int size_to_copy = q_min(remaining_size,
      vulkan_globals.staging_buffer_size);
  VkCommandBuffer command_buffer;
  int staging_offset;
  unsigned char *staging_memory = R_StagingAllocate(size_to_copy, 1,
      &command_buffer, &result_buffer.buffer, &staging_offset);

  /*
   * // Allocate a query pool for storing the needed size for every BLAS compaction.
   VkQueryPool queryPool{VK_NULL_HANDLE};
   if(nbCompactions > 0)  // Is compaction requested?
   {
   assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
   VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
   qpci.queryCount = nbBlas;
   qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
   vkCreateQueryPool(m_device, &qpci, nullptr, &queryPool);
   }
   */

//
//  memcpy(staging_memory, (byte*) vbodata + copy_offset, size_to_copy);
//
//  VkBufferCopy region;
//  region.srcOffset = staging_offset;
//  region.dstOffset = copy_offset;
//  region.size = size_to_copy;
//  vkCmdCopyBuffer(command_buffer, staging_buffer, m->vertex_buffer, 1, &region);
  return result_buffer;
}

void Init_Blas_Inputs() {
  memset(&blas_inputs, 0, sizeof(blas_inputs));
}

void Object_To_VkGeometryKHR(qmodel_t *m, const aliashdr_t *hdr) {

  size_t i = blas_inputs.current_index;

  VkDeviceAddress vertex_address = Get_Buffer_Device_Address(
      vulkan_globals.device, m->vertex_buffer);
  VkDeviceAddress index_address = Get_Buffer_Device_Address(
      vulkan_globals.device, m->index_buffer);

  uint32_t max_primitive_count = hdr->numtris;

  VkAccelerationStructureGeometryTrianglesDataKHR triangles;
  memset(&triangles, 0, sizeof(triangles));
  triangles.sType =
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT; // vec3 vertex position data.
  triangles.vertexData.deviceAddress = vertex_address;
  triangles.vertexStride = sizeof(trivertx_t);
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = index_address;
  triangles.maxVertex = hdr->numverts;

  VkAccelerationStructureGeometryKHR asGeom;
  memset(&asGeom, 0, sizeof(asGeom));
  asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex = 0;
  offset.primitiveCount = max_primitive_count;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  blas_inputs.as_geometry[i] = asGeom;
  blas_inputs.as_build_offset_info[i] = offset;

  blas_inputs.current_index = (blas_inputs.current_index + 1) % MAX_VK_GEOMETRY;
}

void Build_Blas(VkBuildAccelerationStructureFlagsKHR flags) {
  uint32_t nb_blas = blas_inputs.current_index;
  // Memory size of all allocated BLAS.
  VkDeviceSize as_total_size;
  memset(&as_total_size, 0, sizeof(as_total_size));
  // Number of BLAS requesting compaction.
  uint32_t nb_compactions = 0;
  // Largest scratch size.
  VkDeviceSize max_scratch_size;
  memset(&max_scratch_size, 0, sizeof(max_scratch_size));

  build_acceleration_structure_t build_as[nb_blas];

  for (uint32_t idx = 0; idx < nb_blas; idx++) {
    build_as[idx].size_info.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    build_as[idx].build_info.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_as[idx].build_info.type =
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_as[idx].build_info.mode =
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_as[idx].build_info.flags = blas_inputs.flags[idx] | flags;
    build_as[idx].build_info.geometryCount = nb_blas;
    build_as[idx].build_info.pGeometries = &blas_inputs.as_geometry[idx];

    // Build range info
    build_as[idx].offset_info = blas_inputs.as_build_offset_info[idx];

    // Find sizes to create acceleration structures and scratch
    uint32_t max_prim_count[nb_blas];

    for (int tt = 0; tt < nb_blas; tt++) {
      max_prim_count[tt] = blas_inputs.as_build_offset_info[tt].primitiveCount; // Number of triangles.
    }

    fpGetAccelerationStructureBuildSizesKHR(vulkan_globals.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_as[idx].build_info, max_prim_count, &build_as[idx].size_info);

    // extra info
    as_total_size += build_as[idx].size_info.accelerationStructureSize;
    max_scratch_size = q_max(max_scratch_size,
        build_as[idx].size_info.buildScratchSize);
    nb_compactions += has_flag(build_as[idx].build_info.flags,
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
  }

  // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
//  std::vector<uint32_t> indices;  // Indices of the BLAS to create
  uint32_t indices[nb_blas];
  VkDeviceSize batchSize = 0;

  VkDeviceSize batchLimit = 256000000; // 256 MB
  // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
//  VkBufferDeviceAddressInfo buffer_info;
//  memset(&buffer_info, 0, sizeof(buffer_info));
//  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
//  buffer_info.pNext = NULL;
//  buffer_info.buffer = result_buffer.buffer;

//  VkDeviceAddress scratch_address = (
//      vulkan_globals.device, &buffer_info);

//  VkQueryPool query_pool = VK_NULL_HANDLE;

//  for(uint32_t idx = 0; idx < nb_blas; idx++)
//  {
//    indices[idx] = idx;
//    batchSize += build_as[idx].size_info.accelerationStructureSize;
//    // Over the limit or last BLAS element
//    if(batchSize >= batchLimit || idx == nb_blas - 1)
//    {
////      VkCommandBuffer cmdBuf = m_cmdPool.createCommandBuffer();
////      cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
////      m_cmdPool.submitAndWait(cmdBuf);
//
////      if(queryPool)
////      {
////        VkCommandBuffer cmdBuf = m_cmdPool.createCommandBuffer();
////        cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
////        m_cmdPool.submitAndWait(cmdBuf);  // Submit command buffer and call vkQueueWaitIdle
////
////        // Destroy the non-compacted version
////        destroyNonCompacted(indices, buildAs);
////      }
//      // Reset
//
//      batchSize = 0;
//      memset(&indices, 0, sizeof(uint32_t) * nb_blas);
//    }
//  }

//  vkDestroyQueryPool(vulkan_globals.device, query_pool, NULL);
}

