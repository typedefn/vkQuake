//vk_rt.c - wrapping vulkan ray tracing

#include "vk_rt.h"
uint32_t current_offset;
rtx_t rtx;

#define NUMVERTEXNORMALS   162
extern  float r_avertexnormals[NUMVERTEXNORMALS][3];

void VK_Init() {
  current_offset = 0;
}

void VK_Destroy() {

}

void createCommandPool() {
  VkCommandPoolCreateInfo commandPoolCreateInfo = { };
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex =
      vulkan_globals.gfx_queue_family_index;

  if (vkCreateCommandPool(vulkan_globals.device, &commandPoolCreateInfo, NULL,
      &rtx.commandPool) != VK_SUCCESS) {
    Sys_Error("Could not vkCreateCommandPool()");
  }
}

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, int copy_offset) {
  VkCommandBufferAllocateInfo bufferAllocateInfo = { };
  bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandPool = rtx.commandPool;
  bufferAllocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(vulkan_globals.device, &bufferAllocateInfo,
      &commandBuffer);

  VkCommandBufferBeginInfo commandBufferBeginInfo = { };
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  VkBufferCopy bufferCopy = { };
  bufferCopy.size = size;
  bufferCopy.dstOffset = copy_offset;
  bufferCopy.srcOffset = current_offset;

  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopy);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = { };
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(vulkan_globals.queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(vulkan_globals.queue);

  vkFreeCommandBuffers(vulkan_globals.device, rtx.commandPool, 1,
      &commandBuffer);

  current_offset += size;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags, VkBuffer *buffer,
    VkDeviceMemory *bufferMemory) {

  vulkan_globals.device_idle = false;

  VkBufferCreateInfo bufferCreateInfo = { };
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = usageFlags;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(vulkan_globals.device, &bufferCreateInfo, NULL, buffer)
      != VK_SUCCESS) {
    Sys_Error("Could not vkCreateBuffer()");
    return;
  }

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(vulkan_globals.device, *buffer,
      &memoryRequirements);

  VkMemoryAllocateInfo memoryAllocateInfo = { };
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;

  uint32_t memoryTypeIndex = -1;
  for (int x = 0; x < vulkan_globals.memory_properties.memoryTypeCount; x++) {
    if ((memoryRequirements.memoryTypeBits & (1 << x))
        && (vulkan_globals.memory_properties.memoryTypes[x].propertyFlags
            & propertyFlags) == propertyFlags) {
      memoryTypeIndex = x;
      break;
    }
  }
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

  if (vkAllocateMemory(vulkan_globals.device, &memoryAllocateInfo, NULL,
      bufferMemory) != VK_SUCCESS) {
    Sys_Error("Could not vkAllocateMemory()");
  }

  vkBindBufferMemory(vulkan_globals.device, *buffer, *bufferMemory, 0);
}

void createVertexBuffer(qmodel_t *m, const aliashdr_t *hdr) {
  int totalvbosize = 0;
  totalvbosize += (hdr->numposes * hdr->numverts_vbo * sizeof(meshxyz_t)); // ericw -- what RMQEngine called nummeshframes is called numposes in QuakeSpasm
  byte *vbodata;
  const trivertx_t *trivertexes;
  const aliasmesh_t *desc;
  int f;
  int copy_offset;
  int remaining_size;

  // ericw -- RMQEngine stored these vbo*ofs values in aliashdr_t, but we must not
  // mutate Mod_Extradata since it might be reloaded from disk, so I moved them to qmodel_t
  // (test case: roman1.bsp from arwop, 64mb heap)
  m->vboindexofs = 0;

  m->vboxyzofs = 0;
  totalvbosize += (hdr->numposes * hdr->numverts_vbo * sizeof(meshxyz_t)); // ericw -- what RMQEngine called nummeshframes is called numposes in QuakeSpasm

  m->vbostofs = totalvbosize;
  totalvbosize += (hdr->numverts_vbo * sizeof(meshst_t));

  if (isDedicated)
    return;
  if (!hdr->numindexes)
    return;
  if (!totalvbosize)
    return;

  // grab the pointers to data in the extradata

  desc = (aliasmesh_t*) ((byte*) hdr + hdr->meshdesc);
  trivertexes = (trivertx_t*) ((byte*) hdr + hdr->vertexes);

  // create the vertex buffer (empty)

  vbodata = (byte*) malloc(totalvbosize);
  memset(vbodata, 0, totalvbosize);

  // fill in the vertices at the start of the buffer
  for (f = 0; f < hdr->numposes; f++) // ericw -- what RMQEngine called nummeshframes is called numposes in QuakeSpasm
      {
    int v;
    meshxyz_t *xyz = (meshxyz_t*) (vbodata
        + (f * hdr->numverts_vbo * sizeof(meshxyz_t)));
    const trivertx_t *tv = trivertexes + (hdr->numverts * f);

    for (v = 0; v < hdr->numverts_vbo; v++) {
      trivertx_t trivert = tv[desc[v].vertindex];

      xyz[v].xyz[0] = trivert.v[0];
      xyz[v].xyz[1] = trivert.v[1];
      xyz[v].xyz[2] = trivert.v[2];
      xyz[v].xyz[3] = 1;  // need w 1 for 4 byte vertex compression

      // map the normal coordinates in [-1..1] to [-127..127] and store in an unsigned char.
      // this introduces some error (less than 0.004), but the normals were very coarse
      // to begin with
      xyz[v].normal[0] = 127 * r_avertexnormals[trivert.lightnormalindex][0];
      xyz[v].normal[1] = 127 * r_avertexnormals[trivert.lightnormalindex][1];
      xyz[v].normal[2] = 127 * r_avertexnormals[trivert.lightnormalindex][2];
      xyz[v].normal[3] = 0; // unused; for 4-byte alignment
    }
  }

  VkBuffer positionStagingBuffer;
  VkDeviceMemory positionStagingBufferMemory;
  createBuffer(totalvbosize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &positionStagingBuffer,
      &positionStagingBufferMemory);

  remaining_size = totalvbosize;
  copy_offset = 0;

  while (remaining_size > 0) {
    const int size_to_copy = q_min(remaining_size,
        vulkan_globals.staging_buffer_size);
    void *positionData;
    vkMapMemory(vulkan_globals.device, positionStagingBufferMemory, 0,
        totalvbosize, 0, &positionData);
    memcpy(positionData, (byte*) vbodata + copy_offset, size_to_copy);
    vkUnmapMemory(vulkan_globals.device, positionStagingBufferMemory);

    createBuffer(size_to_copy,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m->vertex_buffer,
        &m->vertexPositionBufferMemory);

    copyBuffer(positionStagingBuffer, m->vertex_buffer, size_to_copy, copy_offset);

    copy_offset += size_to_copy;
    remaining_size -= size_to_copy;
  }

  vkDestroyBuffer(vulkan_globals.device, positionStagingBuffer, NULL);
  vkFreeMemory(vulkan_globals.device, positionStagingBufferMemory, NULL);
}

void createImage(uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags, VkImage *image,
    VkDeviceMemory *imageMemory) {
  VkImageCreateInfo imageCreateInfo = { };
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.extent.width = width;
  imageCreateInfo.extent.height = height;
  imageCreateInfo.extent.depth = 1;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.format = format;
  imageCreateInfo.tiling = tiling;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.usage = usageFlags;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(vulkan_globals.device, &imageCreateInfo, NULL, image)
      != VK_SUCCESS) {
    Sys_Error("Could not vkCreateImage()");
    return;
  }

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(vulkan_globals.device, *image,
      &memoryRequirements);

  VkMemoryAllocateInfo memoryAllocateInfo = { };
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;

  uint32_t memoryTypeIndex = -1;
  for (int x = 0; x < vulkan_globals.memory_properties.memoryTypeCount; x++) {
    if ((memoryRequirements.memoryTypeBits & (1 << x))
        && (vulkan_globals.memory_properties.memoryTypes[x].propertyFlags
            & propertyFlags) == propertyFlags) {
      memoryTypeIndex = x;
      break;
    }
  }
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

  if (vkAllocateMemory(vulkan_globals.device, &memoryAllocateInfo, NULL,
      imageMemory) != VK_SUCCESS) {
    Sys_Error("Could not vkAllocateMemory()");
    return;
  }

  vkBindImageMemory(vulkan_globals.device, *image, *imageMemory, 0);
}

void createIndexBuffer(qmodel_t *m, const aliashdr_t *hdr) {
  int totalvbosize = 0;
  int remaining_size;
  int copy_offset;
  const aliasmesh_t *desc;
  const short *indexes;
  const trivertx_t *trivertexes;
  byte *vbodata;
  int f;

  m->vboindexofs = 0;

  m->vboxyzofs = 0;
  totalvbosize += (hdr->numposes * hdr->numverts_vbo * sizeof(meshxyz_t));

  m->vbostofs = totalvbosize;
  totalvbosize += (hdr->numverts_vbo * sizeof(meshst_t));

  if (isDedicated)
    return;
  if (!hdr->numindexes)
    return;
  if (!totalvbosize)
    return;

  // grab the pointers to data in the extradata

  desc = (aliasmesh_t*) ((byte*) hdr + hdr->meshdesc);
  indexes = (short*) ((byte*) hdr + hdr->indexes);
  trivertexes = (trivertx_t*) ((byte*) hdr + hdr->vertexes);

  vbodata = (byte *) malloc(totalvbosize);
  memset(vbodata, 0, totalvbosize);

  VkDeviceSize bufferSize = hdr->numindexes * sizeof(unsigned short);

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
      &stagingBufferMemory);

  remaining_size = bufferSize;
  copy_offset = 0;

  while (remaining_size > 0) {
    const int size_to_copy = q_min(remaining_size,
        vulkan_globals.staging_buffer_size);

    void *data;
    vkMapMemory(vulkan_globals.device, stagingBufferMemory, 0, size_to_copy, 0,
        &data);
    memcpy(data, (byte*) indexes + copy_offset, size_to_copy);
    vkUnmapMemory(vulkan_globals.device, stagingBufferMemory);

    createBuffer(size_to_copy,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m->index_buffer,
        &m->indexBufferMemory);

    copyBuffer(stagingBuffer, m->index_buffer, size_to_copy, copy_offset);

    copy_offset += size_to_copy;
    remaining_size -= size_to_copy;
  }

  free (vbodata);

  vkDestroyBuffer(vulkan_globals.device, stagingBuffer, NULL);
  vkFreeMemory(vulkan_globals.device, stagingBufferMemory, NULL);
}
