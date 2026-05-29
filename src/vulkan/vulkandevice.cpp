#ifdef RW_VULKAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwplugins.h"

#include "rwvulkan.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace vulkan {

#include "vulkanshaders.inc"

struct DisplayMode
{
	SDL_DisplayMode mode;
	int32 depth;
	uint32 flags;
};

struct VulkanBuffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize capacity;
	void *mapped;
};

struct FrameTextureUploadGarbage
{
	VulkanBuffer staging[16];
	Raster *raster[16];
	uint32 numStaging;
};

struct RayAccelerationStructure
{
	VkAccelerationStructureKHR as;
	VulkanBuffer buffer;
	uint32 capacity;
};

struct RayGeometry
{
	Geometry *geometry;
	uint16 serialNumber;
	RayAccelerationStructure blas;
};

struct RayInstanceRef
{
	RayGeometry *geometry;
	Matrix transform;
};

struct RayFrameResources
{
	RayAccelerationStructure tlas;
	VulkanBuffer instances;
	VulkanBuffer scratch;
	uint32 capacity;
	bool32 valid;
};

struct Im2DPushConstants
{
	float xform[4];
	float matColor[4];
	float alphaRef[4];
};

struct Color3DPushConstants
{
	float mvp[16];
	float matColor[4];
	float alphaRef[4];
	float rtParams[4];
	float rtLight[4];
};

enum PipelineKind
{
	PIPE_IM2D,
	PIPE_IM2D_ZTEST,
	PIPE_IM2D_ZWRITE,
	PIPE_IM2D_ZTEST_ZWRITE,
	PIPE_COLOR3D,
	PIPE_COLOR3D_NOZWRITE,
	PIPE_COLOR3D_NOZTEST,
	PIPE_COLOR3D_NOZTEST_NOZWRITE,
	PIPE_COUNT
};

enum BlendPipelineMode
{
	PIPE_BLEND_OPAQUE,
	PIPE_BLEND_ALPHA,
	PIPE_BLEND_ADD,
	PIPE_BLEND_SRCALPHA_ADD,
	PIPE_BLEND_ONE_INVSRCALPHA,
	PIPE_BLEND_ZERO_ONE,
	PIPE_BLEND_ZERO_SRCCOLOR,
	PIPE_BLEND_ZERO_INVSRCCOLOR,
	PIPE_BLEND_INVDESTCOLOR_ZERO,
	PIPE_BLEND_SRCALPHA_INVDESTALPHA,
	PIPE_BLEND_DESTALPHA_INVDESTALPHA,
	PIPE_BLEND_COUNT
};

enum {
	MAX_PRIM_PIPELINES = 7,
	MAX_FRAMES_IN_FLIGHT = 3,
	MAX_PENDING_TEXTURE_UPLOADS = 4096
};

enum PresentModePreference
{
	PRESENT_MODE_AUTO,
	PRESENT_MODE_FIFO,
	PRESENT_MODE_FIFO_RELAXED,
	PRESENT_MODE_MAILBOX,
	PRESENT_MODE_IMMEDIATE,
	PRESENT_MODE_COUNT
};

static const VkDeviceSize DYNAMIC_VERTEX_BUFFER_SIZE = 32ull * 1024ull * 1024ull;
static const VkDeviceSize DYNAMIC_INDEX_BUFFER_SIZE = 8ull * 1024ull * 1024ull;
static const VkDeviceSize TEXTURE_UPLOAD_BYTES_PER_FRAME = 16ull * 1024ull * 1024ull;
static const uint32 TEXTURE_UPLOADS_PER_FRAME = 12;
static const uint64 TEXTURE_UPLOAD_USECS_PER_FRAME = 4000;
static const uint32 MAX_RAY_PENDING_INSTANCES = 8192;

struct VulkanGlobals
{
	Context context;
	SDL_Window **pWindow;
	SDL_DisplayID *displays;
	int numDisplays;
	int currentDisplay;
	DisplayMode *modes;
	int numModes;
	int currentMode;
	int winWidth, winHeight;
	const char *winTitle;
	RGBA clearColor;
	uint32 clearMode;
	bool32 canCopyFromSwapchain;
	bool32 commandReady;
	int32 presentModePreference;
	VkPresentModeKHR activePresentMode;
	bool32 swapchainRecreateRequested;
	void *renderStates[GSALPHATESTREF + 1];
	float view[16];
	float proj[16];
	VulkanBuffer dynamicVertexBuffers[MAX_FRAMES_IN_FLIGHT];
	VulkanBuffer dynamicIndexBuffers[MAX_FRAMES_IN_FLIGHT];
	VkDeviceSize dynamicVertexOffset;
	VkDeviceSize dynamicIndexOffset;
	VkDescriptorSetLayout textureSetLayout;
	VkDescriptorSetLayout raySetLayout;
	VkDescriptorPool descriptorPool;
	VkImage whiteImage;
	VkDeviceMemory whiteImageMemory;
	VkImageView whiteImageView;
	VkSampler whiteSampler;
	VkDescriptorSet whiteDescriptorSet;
	VkDescriptorSet rayDescriptorSets[MAX_FRAMES_IN_FLIGHT];
	Raster *pendingTextureUploads[MAX_PENDING_TEXTURE_UPLOADS];
	uint32 numPendingTextureUploads;
	FrameTextureUploadGarbage textureUploadGarbage[MAX_FRAMES_IN_FLIGHT];
	VkPipelineLayout pipelineLayouts[PIPE_COUNT];
	VkPipeline pipelines[PIPE_COUNT][PIPE_BLEND_COUNT][MAX_PRIM_PIPELINES];
	RayFrameResources rayFrames[MAX_FRAMES_IN_FLIGHT];
	RayAccelerationStructure dummyBlas;
	RayAccelerationStructure dummyTlas;
	VulkanBuffer dummyInstanceBuffer;
	RayGeometry *rayGeometries;
	uint32 numRayGeometries;
	uint32 rayGeometryCapacity;
	RayInstanceRef *rayPendingInstances;
	uint32 numRayPendingInstances;
	uint32 rayPendingInstanceCapacity;
	bool32 rayTracingUserEnabled;
	bool32 rayResourcesReady;
	bool32 raySceneHasInstances;
	bool32 raySceneLogged;
	Im3DVertex *im3dVertices;
	uint32 im3dNumVertices;
	Matrix im3dWorld;
	uint32 im3dFlags;
	Im3DVertex *tempVertices;
	uint32 tempVertexCapacity;
	uint16 *tempIndices;
	uint32 tempIndexCapacity;
	VkPipeline currentPipeline;
	VkDescriptorSet currentDescriptorSet;
	PipelineKind currentDescriptorSetKind;
	bool renderStateSet[GSALPHATESTREF+1];
};

static VulkanGlobals vkGlobals;

static bool32
vkOk(VkResult result, const char *what)
{
	if(result == VK_SUCCESS)
		return 1;
	fprintf(stderr, "Vulkan error in %s: %d\n", what, result);
	RWERROR((ERR_GENERAL, what));
	return 0;
}

static void copyIdentity(float *m);
static void queueTextureUpload(Raster *raster);
static void removeTextureUploadGarbageForRaster(Raster *raster);
static void destroyTextureHandles(VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkSampler *sampler);

static bool32
apiVersionAtLeast(uint32 version, uint32 major, uint32 minor)
{
	return VK_VERSION_MAJOR(version) > major ||
	       (VK_VERSION_MAJOR(version) == major && VK_VERSION_MINOR(version) >= minor);
}

static uint32
findMemoryType(uint32 typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vkGlobals.context.physicalDevice, &memProperties);
	for(uint32 i = 0; i < memProperties.memoryTypeCount; i++)
		if((typeFilter & (1u << i)) &&
		   (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	assert(0 && "No compatible Vulkan memory type");
	return 0;
}

static void
destroyBuffer(VulkanBuffer *buffer)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE)
		return;
	if(buffer->buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(ctx->device, buffer->buffer, nil);
	if(buffer->mapped != nil)
		vkUnmapMemory(ctx->device, buffer->memory);
	if(buffer->memory != VK_NULL_HANDLE)
		vkFreeMemory(ctx->device, buffer->memory, nil);
	buffer->buffer = VK_NULL_HANDLE;
	buffer->memory = VK_NULL_HANDLE;
	buffer->mapped = nil;
	buffer->capacity = 0;
}

static bool32
createBuffer(VulkanBuffer *buffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
	Context *ctx = &vkGlobals.context;
	memset(buffer, 0, sizeof(*buffer));
	buffer->buffer = VK_NULL_HANDLE;
	buffer->memory = VK_NULL_HANDLE;
	buffer->mapped = nil;

	VkBufferCreateInfo bufferInfo;
	memset(&bufferInfo, 0, sizeof(bufferInfo));
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateBuffer(ctx->device, &bufferInfo, nil, &buffer->buffer), "vkCreateBuffer"))
		return 0;

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(ctx->device, buffer->buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, &buffer->memory), "vkAllocateMemory")){
		destroyBuffer(buffer);
		return 0;
	}
	if(!vkOk(vkBindBufferMemory(ctx->device, buffer->buffer, buffer->memory, 0), "vkBindBufferMemory")){
		destroyBuffer(buffer);
		return 0;
	}
	if(!vkOk(vkMapMemory(ctx->device, buffer->memory, 0, size, 0, &buffer->mapped), "vkMapMemory")){
		destroyBuffer(buffer);
		return 0;
	}
	buffer->capacity = size;
	return 1;
}

static bool32
createRawBuffer(VulkanBuffer *buffer, VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties, bool32 mapMemory, bool32 deviceAddress)
{
	Context *ctx = &vkGlobals.context;
	memset(buffer, 0, sizeof(*buffer));
	buffer->buffer = VK_NULL_HANDLE;
	buffer->memory = VK_NULL_HANDLE;
	buffer->mapped = nil;

	VkBufferCreateInfo bufferInfo;
	memset(&bufferInfo, 0, sizeof(bufferInfo));
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size ? size : 1;
	bufferInfo.usage = usage;
	if(deviceAddress)
		bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateBuffer(ctx->device, &bufferInfo, nil, &buffer->buffer), "vkCreateBuffer(raw)"))
		return 0;

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(ctx->device, buffer->buffer, &memRequirements);

	VkMemoryAllocateFlagsInfo flagsInfo;
	memset(&flagsInfo, 0, sizeof(flagsInfo));
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	if(deviceAddress)
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = deviceAddress ? &flagsInfo : nil;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, &buffer->memory), "vkAllocateMemory(raw)")){
		destroyBuffer(buffer);
		return 0;
	}
	if(!vkOk(vkBindBufferMemory(ctx->device, buffer->buffer, buffer->memory, 0), "vkBindBufferMemory(raw)")){
		destroyBuffer(buffer);
		return 0;
	}
	if(mapMemory &&
	   !vkOk(vkMapMemory(ctx->device, buffer->memory, 0, bufferInfo.size, 0, &buffer->mapped), "vkMapMemory(raw)")){
		destroyBuffer(buffer);
		return 0;
	}
	buffer->capacity = bufferInfo.size;
	return 1;
}

static VkDeviceAddress
getBufferDeviceAddress(VkBuffer buffer)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || ctx->getBufferDeviceAddressKHR == nil || buffer == VK_NULL_HANDLE)
		return 0;
	VkBufferDeviceAddressInfo info;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.buffer = buffer;
	return ctx->getBufferDeviceAddressKHR(ctx->device, &info);
}

static bool32
ensureBuffer(VulkanBuffer *buffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
	if(size == 0)
		size = 1;
	if(buffer->buffer != VK_NULL_HANDLE && buffer->capacity >= size)
		return 1;
	if((buffer->buffer != VK_NULL_HANDLE || buffer->memory != VK_NULL_HANDLE) &&
	   vkGlobals.context.device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(vkGlobals.context.device);
	destroyBuffer(buffer);
	VkDeviceSize capacity = 4096;
	while(capacity < size)
		capacity *= 2;
	return createBuffer(buffer, capacity, usage);
}

static bool32
uploadBuffer(VulkanBuffer *buffer, const void *data, VkDeviceSize size, VkBufferUsageFlags usage)
{
	if(!ensureBuffer(buffer, size, usage))
		return 0;
	memcpy(buffer->mapped, data, (size_t)size);
	return 1;
}

static VkDeviceSize
alignDynamicOffset(VkDeviceSize offset, VkDeviceSize alignment)
{
	return (offset + alignment - 1) & ~(alignment - 1);
}

static bool32
uploadDynamicBuffer(VulkanBuffer *buffer, VkDeviceSize *cursor, const void *data,
	VkDeviceSize size, VkBufferUsageFlags usage, VkDeviceSize alignment, VkDeviceSize *offsetOut)
{
	VkDeviceSize offset = alignDynamicOffset(*cursor, alignment);
	VkDeviceSize end = offset + size;

	if(buffer->buffer == VK_NULL_HANDLE || buffer->capacity < end){
		if(*cursor != 0){
			fprintf(stderr, "Vulkan dynamic buffer exhausted: requested %llu bytes, capacity %llu bytes\n",
				(unsigned long long)end, (unsigned long long)buffer->capacity);
			return 0;
		}
		if(!ensureBuffer(buffer, end, usage))
			return 0;
	}

	memcpy((uint8*)buffer->mapped + offset, data, (size_t)size);

	*offsetOut = offset;
	*cursor = end;
	return 1;
}

static VkCommandBuffer
beginSingleTimeCommands(void)
{
	Context *ctx = &vkGlobals.context;
	VkCommandBufferAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = ctx->commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	if(!vkOk(vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer), "vkAllocateCommandBuffers"))
		return VK_NULL_HANDLE;

	VkCommandBufferBeginInfo beginInfo;
	memset(&beginInfo, 0, sizeof(beginInfo));
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if(!vkOk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer")){
		vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &commandBuffer);
		return VK_NULL_HANDLE;
	}
	return commandBuffer;
}

static bool32
endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	Context *ctx = &vkGlobals.context;
	if(!vkOk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer"))
		return 0;

	VkSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(submitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceInfo;
	memset(&fenceInfo, 0, sizeof(fenceInfo));
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	bool32 ok = vkOk(vkCreateFence(ctx->device, &fenceInfo, nil, &fence), "vkCreateFence(single)");
	if(ok)
		ok = vkOk(vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, fence), "vkQueueSubmit");
	if(ok)
		ok = vkOk(vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences(single)");
	if(fence != VK_NULL_HANDLE)
		vkDestroyFence(ctx->device, fence, nil);
	vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &commandBuffer);
	return ok;
}

static void
destroyRayAccelerationStructure(RayAccelerationStructure *as)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device != VK_NULL_HANDLE && as->as != VK_NULL_HANDLE && ctx->destroyAccelerationStructureKHR)
		ctx->destroyAccelerationStructureKHR(ctx->device, as->as, nil);
	as->as = VK_NULL_HANDLE;
	destroyBuffer(&as->buffer);
	as->capacity = 0;
}

static bool32
createRayAccelerationStructure(RayAccelerationStructure *as,
	VkAccelerationStructureTypeKHR type, VkDeviceSize size, uint32 capacity)
{
	Context *ctx = &vkGlobals.context;
	destroyRayAccelerationStructure(as);
	if(size == 0 || ctx->createAccelerationStructureKHR == nil)
		return 0;
	if(!createRawBuffer(&as->buffer, size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 0))
		return 0;

	VkAccelerationStructureCreateInfoKHR createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = as->buffer.buffer;
	createInfo.size = size;
	createInfo.type = type;
	if(!vkOk(ctx->createAccelerationStructureKHR(ctx->device, &createInfo, nil, &as->as),
	         "vkCreateAccelerationStructureKHR")){
		destroyRayAccelerationStructure(as);
		return 0;
	}
	as->capacity = capacity;
	return 1;
}

static VkDeviceAddress
getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || ctx->getAccelerationStructureDeviceAddressKHR == nil || as == VK_NULL_HANDLE)
		return 0;
	VkAccelerationStructureDeviceAddressInfoKHR info;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	info.accelerationStructure = as;
	return ctx->getAccelerationStructureDeviceAddressKHR(ctx->device, &info);
}

static void
rayBuildBarrier(VkCommandBuffer commandBuffer)
{
	VkMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 1, &barrier, 0, nil, 0, nil);
}

static bool32
buildBottomLevelAccelerationStructure(RayAccelerationStructure *blas,
	const V3d *vertices, uint32 numVertices, const uint16 *indices, uint32 numTriangles)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || vertices == nil || indices == nil || numVertices == 0 || numTriangles == 0)
		return 0;

	VulkanBuffer vertexBuffer, indexBuffer, scratchBuffer;
	memset(&vertexBuffer, 0, sizeof(vertexBuffer));
	memset(&indexBuffer, 0, sizeof(indexBuffer));
	memset(&scratchBuffer, 0, sizeof(scratchBuffer));

	VkDeviceSize vertexSize = (VkDeviceSize)numVertices * sizeof(V3d);
	VkDeviceSize indexSize = (VkDeviceSize)numTriangles * 3 * sizeof(uint16);
	if(!createRawBuffer(&vertexBuffer, vertexSize,
		   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, 1) ||
	   !createRawBuffer(&indexBuffer, indexSize,
		   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, 1)){
		destroyBuffer(&vertexBuffer);
		destroyBuffer(&indexBuffer);
		return 0;
	}
	memcpy(vertexBuffer.mapped, vertices, (size_t)vertexSize);
	memcpy(indexBuffer.mapped, indices, (size_t)indexSize);

	VkAccelerationStructureGeometryKHR geometry;
	memset(&geometry, 0, sizeof(geometry));
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry.geometry.triangles.vertexData.deviceAddress = getBufferDeviceAddress(vertexBuffer.buffer);
	geometry.geometry.triangles.vertexStride = sizeof(V3d);
	geometry.geometry.triangles.maxVertex = numVertices - 1;
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
	geometry.geometry.triangles.indexData.deviceAddress = getBufferDeviceAddress(indexBuffer.buffer);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
	memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
	memset(&sizeInfo, 0, sizeof(sizeInfo));
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	ctx->getAccelerationStructureBuildSizesKHR(ctx->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &numTriangles, &sizeInfo);

	if(!createRayAccelerationStructure(blas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		   sizeInfo.accelerationStructureSize, numTriangles) ||
	   !createRawBuffer(&scratchBuffer, sizeInfo.buildScratchSize,
		   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 1)){
		destroyBuffer(&vertexBuffer);
		destroyBuffer(&indexBuffer);
		destroyBuffer(&scratchBuffer);
		destroyRayAccelerationStructure(blas);
		return 0;
	}

	buildInfo.dstAccelerationStructure = blas->as;
	buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.buffer);

	VkAccelerationStructureBuildRangeInfoKHR range;
	memset(&range, 0, sizeof(range));
	range.primitiveCount = numTriangles;
	const VkAccelerationStructureBuildRangeInfoKHR *ranges[] = { &range };

	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	if(commandBuffer == VK_NULL_HANDLE){
		destroyBuffer(&vertexBuffer);
		destroyBuffer(&indexBuffer);
		destroyBuffer(&scratchBuffer);
		destroyRayAccelerationStructure(blas);
		return 0;
	}
	ctx->cmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, ranges);
	rayBuildBarrier(commandBuffer);
	bool32 ok = endSingleTimeCommands(commandBuffer);

	destroyBuffer(&vertexBuffer);
	destroyBuffer(&indexBuffer);
	destroyBuffer(&scratchBuffer);
	if(!ok)
		destroyRayAccelerationStructure(blas);
	return ok;
}

static VkTransformMatrixKHR
makeRayTransform(const Matrix *m)
{
	VkTransformMatrixKHR t;
	memset(&t, 0, sizeof(t));
	t.matrix[0][0] = m->right.x; t.matrix[0][1] = m->up.x; t.matrix[0][2] = m->at.x; t.matrix[0][3] = m->pos.x;
	t.matrix[1][0] = m->right.y; t.matrix[1][1] = m->up.y; t.matrix[1][2] = m->at.y; t.matrix[1][3] = m->pos.y;
	t.matrix[2][0] = m->right.z; t.matrix[2][1] = m->up.z; t.matrix[2][2] = m->at.z; t.matrix[2][3] = m->pos.z;
	return t;
}

static bool32
ensureTopLevelAccelerationStructure(RayAccelerationStructure *tlas, VulkanBuffer *instances,
	VulkanBuffer *scratch, uint32 *capacity, uint32 requiredInstances)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || requiredInstances == 0)
		return 0;
	if(*capacity >= requiredInstances && tlas->as != VK_NULL_HANDLE &&
	   instances->buffer != VK_NULL_HANDLE && scratch->buffer != VK_NULL_HANDLE)
		return 1;

	uint32 newCapacity = *capacity ? *capacity : 1;
	while(newCapacity < requiredInstances)
		newCapacity *= 2;

	destroyRayAccelerationStructure(tlas);
	destroyBuffer(instances);
	destroyBuffer(scratch);

	if(!createRawBuffer(instances, (VkDeviceSize)newCapacity * sizeof(VkAccelerationStructureInstanceKHR),
		   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, 1))
		return 0;

	VkAccelerationStructureGeometryKHR geometry;
	memset(&geometry, 0, sizeof(geometry));
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
	memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
	memset(&sizeInfo, 0, sizeof(sizeInfo));
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	ctx->getAccelerationStructureBuildSizesKHR(ctx->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &newCapacity, &sizeInfo);

	if(!createRayAccelerationStructure(tlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		   sizeInfo.accelerationStructureSize, newCapacity) ||
	   !createRawBuffer(scratch, sizeInfo.buildScratchSize,
		   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 1)){
		destroyRayAccelerationStructure(tlas);
		destroyBuffer(instances);
		destroyBuffer(scratch);
		*capacity = 0;
		return 0;
	}
	*capacity = newCapacity;
	return 1;
}

static bool32
buildTopLevelAccelerationStructure(RayAccelerationStructure *tlas, VulkanBuffer *instances,
	VulkanBuffer *scratch, uint32 *capacity, const VkAccelerationStructureInstanceKHR *instanceData,
	uint32 instanceCount, VkCommandBuffer commandBuffer)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || instanceData == nil || instanceCount == 0)
		return 0;
	if(!ensureTopLevelAccelerationStructure(tlas, instances, scratch, capacity, instanceCount))
		return 0;
	memcpy(instances->mapped, instanceData, (size_t)instanceCount * sizeof(VkAccelerationStructureInstanceKHR));

	VkAccelerationStructureGeometryKHR geometry;
	memset(&geometry, 0, sizeof(geometry));
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data.deviceAddress = getBufferDeviceAddress(instances->buffer);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
	memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = tlas->as;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;
	buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratch->buffer);

	VkAccelerationStructureBuildRangeInfoKHR range;
	memset(&range, 0, sizeof(range));
	range.primitiveCount = instanceCount;
	const VkAccelerationStructureBuildRangeInfoKHR *ranges[] = { &range };

	bool32 ownCommandBuffer = commandBuffer == VK_NULL_HANDLE;
	if(ownCommandBuffer){
		commandBuffer = beginSingleTimeCommands();
		if(commandBuffer == VK_NULL_HANDLE)
			return 0;
	}
	ctx->cmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, ranges);
	rayBuildBarrier(commandBuffer);
	if(ownCommandBuffer)
		return endSingleTimeCommands(commandBuffer);
	return 1;
}

static void
transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	}else if(oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}else if((oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ||
	          oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) &&
	         newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL){
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
	         newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}

	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
		0, nil, 0, nil, 1, &barrier);
}

static bool32
createTextureImage(uint32 width, uint32 height, const uint8 *rgba,
	VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkSampler *sampler, VkDescriptorSet *descriptorSet)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE || ctx->commandPool == VK_NULL_HANDLE ||
	   vkGlobals.textureSetLayout == VK_NULL_HANDLE || vkGlobals.descriptorPool == VK_NULL_HANDLE)
		return 0;

	VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
	VulkanBuffer staging;
	memset(&staging, 0, sizeof(staging));
	if(!uploadBuffer(&staging, rgba, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		return 0;

	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateImage(ctx->device, &imageInfo, nil, image), "vkCreateImage")){
		destroyBuffer(&staging);
		return 0;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(ctx->device, *image, &memRequirements);
	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, memory), "vkAllocateMemory") ||
	   !vkOk(vkBindImageMemory(ctx->device, *image, *memory, 0), "vkBindImageMemory")){
		destroyBuffer(&staging);
		return 0;
	}

	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	if(commandBuffer == VK_NULL_HANDLE){
		destroyBuffer(&staging);
		return 0;
	}
	transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkBufferImageCopy region;
	memset(&region, 0, sizeof(region));
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(commandBuffer, staging.buffer, *image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	bool32 ok = endSingleTimeCommands(commandBuffer);
	destroyBuffer(&staging);
	if(!ok)
		return 0;

	VkImageViewCreateInfo viewInfo;
	memset(&viewInfo, 0, sizeof(viewInfo));
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if(!vkOk(vkCreateImageView(ctx->device, &viewInfo, nil, view), "vkCreateImageView"))
		return 0;

	VkSamplerCreateInfo samplerInfo;
	memset(&samplerInfo, 0, sizeof(samplerInfo));
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	if(!vkOk(vkCreateSampler(ctx->device, &samplerInfo, nil, sampler), "vkCreateSampler"))
		return 0;

	VkDescriptorSetAllocateInfo descAlloc;
	memset(&descAlloc, 0, sizeof(descAlloc));
	descAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAlloc.descriptorPool = vkGlobals.descriptorPool;
	descAlloc.descriptorSetCount = 1;
	descAlloc.pSetLayouts = &vkGlobals.textureSetLayout;
	if(!vkOk(vkAllocateDescriptorSets(ctx->device, &descAlloc, descriptorSet), "vkAllocateDescriptorSets"))
		return 0;

	VkDescriptorImageInfo imageDesc;
	memset(&imageDesc, 0, sizeof(imageDesc));
	imageDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageDesc.imageView = *view;
	imageDesc.sampler = *sampler;

	VkWriteDescriptorSet write;
	memset(&write, 0, sizeof(write));
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = *descriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageDesc;
	vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nil);
	return 1;
}

static bool32
createTextureImageForFrame(uint32 width, uint32 height, const uint8 *rgba, VkCommandBuffer commandBuffer,
	VulkanBuffer *stagingOut, VkImage *image, VkDeviceMemory *memory, VkImageView *view,
	VkSampler *sampler, VkDescriptorSet *descriptorSet)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE ||
	   vkGlobals.textureSetLayout == VK_NULL_HANDLE || vkGlobals.descriptorPool == VK_NULL_HANDLE)
		return 0;

	memset(stagingOut, 0, sizeof(*stagingOut));
	*image = VK_NULL_HANDLE;
	*memory = VK_NULL_HANDLE;
	*view = VK_NULL_HANDLE;
	*sampler = VK_NULL_HANDLE;
	*descriptorSet = VK_NULL_HANDLE;

	VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
	if(!uploadBuffer(stagingOut, rgba, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		return 0;

	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateImage(ctx->device, &imageInfo, nil, image), "vkCreateImage(frame texture)")){
		destroyBuffer(stagingOut);
		return 0;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(ctx->device, *image, &memRequirements);
	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, memory), "vkAllocateMemory(frame texture)") ||
	   !vkOk(vkBindImageMemory(ctx->device, *image, *memory, 0), "vkBindImageMemory(frame texture)")){
		destroyTextureHandles(image, memory, view, sampler);
		destroyBuffer(stagingOut);
		return 0;
	}

	transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkBufferImageCopy region;
	memset(&region, 0, sizeof(region));
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(commandBuffer, stagingOut->buffer, *image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkImageViewCreateInfo viewInfo;
	memset(&viewInfo, 0, sizeof(viewInfo));
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if(!vkOk(vkCreateImageView(ctx->device, &viewInfo, nil, view), "vkCreateImageView(frame texture)")){
		destroyTextureHandles(image, memory, view, sampler);
		destroyBuffer(stagingOut);
		return 0;
	}

	VkSamplerCreateInfo samplerInfo;
	memset(&samplerInfo, 0, sizeof(samplerInfo));
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	if(!vkOk(vkCreateSampler(ctx->device, &samplerInfo, nil, sampler), "vkCreateSampler(frame texture)")){
		destroyTextureHandles(image, memory, view, sampler);
		destroyBuffer(stagingOut);
		return 0;
	}

	VkDescriptorSetAllocateInfo descAlloc;
	memset(&descAlloc, 0, sizeof(descAlloc));
	descAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAlloc.descriptorPool = vkGlobals.descriptorPool;
	descAlloc.descriptorSetCount = 1;
	descAlloc.pSetLayouts = &vkGlobals.textureSetLayout;
	if(!vkOk(vkAllocateDescriptorSets(ctx->device, &descAlloc, descriptorSet),
	         "vkAllocateDescriptorSets(frame texture)")){
		destroyTextureHandles(image, memory, view, sampler);
		destroyBuffer(stagingOut);
		return 0;
	}

	VkDescriptorImageInfo imageDesc;
	memset(&imageDesc, 0, sizeof(imageDesc));
	imageDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageDesc.imageView = *view;
	imageDesc.sampler = *sampler;

	VkWriteDescriptorSet write;
	memset(&write, 0, sizeof(write));
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = *descriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageDesc;
	vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nil);
	return 1;
}

static bool32
createEmptySampledImage(uint32 width, uint32 height, VkFormat format, VkImageUsageFlags usage,
	VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkSampler *sampler, VkDescriptorSet *descriptorSet)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE || vkGlobals.textureSetLayout == VK_NULL_HANDLE ||
	   vkGlobals.descriptorPool == VK_NULL_HANDLE)
		return 0;

	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateImage(ctx->device, &imageInfo, nil, image), "vkCreateImage(sampled)"))
		return 0;

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(ctx->device, *image, &memRequirements);
	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, memory), "vkAllocateMemory(sampled)") ||
	   !vkOk(vkBindImageMemory(ctx->device, *image, *memory, 0), "vkBindImageMemory(sampled)"))
		return 0;

	VkImageViewCreateInfo viewInfo;
	memset(&viewInfo, 0, sizeof(viewInfo));
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if(!vkOk(vkCreateImageView(ctx->device, &viewInfo, nil, view), "vkCreateImageView(sampled)"))
		return 0;

	VkSamplerCreateInfo samplerInfo;
	memset(&samplerInfo, 0, sizeof(samplerInfo));
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	if(!vkOk(vkCreateSampler(ctx->device, &samplerInfo, nil, sampler), "vkCreateSampler(sampled)"))
		return 0;

	VkDescriptorSetAllocateInfo descAlloc;
	memset(&descAlloc, 0, sizeof(descAlloc));
	descAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAlloc.descriptorPool = vkGlobals.descriptorPool;
	descAlloc.descriptorSetCount = 1;
	descAlloc.pSetLayouts = &vkGlobals.textureSetLayout;
	if(!vkOk(vkAllocateDescriptorSets(ctx->device, &descAlloc, descriptorSet), "vkAllocateDescriptorSets(sampled)"))
		return 0;

	VkDescriptorImageInfo imageDesc;
	memset(&imageDesc, 0, sizeof(imageDesc));
	imageDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageDesc.imageView = *view;
	imageDesc.sampler = *sampler;

	VkWriteDescriptorSet write;
	memset(&write, 0, sizeof(write));
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = *descriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageDesc;
	vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nil);
	return 1;
}

static void
destroyTextureHandles(VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkSampler *sampler)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE)
		return;
	if(*sampler != VK_NULL_HANDLE)
		vkDestroySampler(ctx->device, *sampler, nil);
	if(*view != VK_NULL_HANDLE)
		vkDestroyImageView(ctx->device, *view, nil);
	if(*image != VK_NULL_HANDLE)
		vkDestroyImage(ctx->device, *image, nil);
	if(*memory != VK_NULL_HANDLE)
		vkFreeMemory(ctx->device, *memory, nil);
	*image = VK_NULL_HANDLE;
	*memory = VK_NULL_HANDLE;
	*view = VK_NULL_HANDLE;
	*sampler = VK_NULL_HANDLE;
}

void
destroyRasterTexture(Raster *raster)
{
	if(raster == nil || nativeRasterOffset == 0)
		return;
	for(uint32 i = 0; i < vkGlobals.numPendingTextureUploads; ){
		if(vkGlobals.pendingTextureUploads[i] == raster){
			vkGlobals.pendingTextureUploads[i] =
				vkGlobals.pendingTextureUploads[--vkGlobals.numPendingTextureUploads];
			continue;
		}
		i++;
	}
	removeTextureUploadGarbageForRaster(raster);
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	destroyTextureHandles(&natras->image, &natras->imageMemory, &natras->imageView, &natras->sampler);
	natras->descriptorSet = VK_NULL_HANDLE;
	natras->imageFormat = VK_FORMAT_UNDEFINED;
	natras->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	natras->gpuReady = 0;
	natras->gpuDirty = 1;
	natras->uploadSubmitted = 0;
}

static void
convertRasterToRGBA(Raster *raster, uint8 *dst)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	RasterLevels *levels = natras->levels;
	if(levels == nil || levels->numlevels <= 0)
		return;

	uint8 *src = levels->levels[0].data;
	uint32 pixels = (uint32)levels->levels[0].width * levels->levels[0].height;
	for(uint32 i = 0; i < pixels; i++){
		if(natras->bpp == 4){
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			src += 4;
		}else if(natras->bpp == 3){
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
			src += 3;
		}else{
			uint16 p = (uint16)(src[0] | (src[1] << 8));
			switch(raster->format & 0xF00){
			case Raster::C565:
				dst[0] = (uint8)(((p >> 11) & 0x1F) * 255 / 31);
				dst[1] = (uint8)(((p >> 5) & 0x3F) * 255 / 63);
				dst[2] = (uint8)((p & 0x1F) * 255 / 31);
				dst[3] = 255;
				break;
			case Raster::C555:
				dst[0] = (uint8)(((p >> 10) & 0x1F) * 255 / 31);
				dst[1] = (uint8)(((p >> 5) & 0x1F) * 255 / 31);
				dst[2] = (uint8)((p & 0x1F) * 255 / 31);
				dst[3] = 255;
				break;
			case Raster::C4444:
				dst[0] = (uint8)(((p >> 8) & 0x0F) * 255 / 15);
				dst[1] = (uint8)(((p >> 4) & 0x0F) * 255 / 15);
				dst[2] = (uint8)((p & 0x0F) * 255 / 15);
				dst[3] = (uint8)(((p >> 12) & 0x0F) * 255 / 15);
				break;
			case Raster::C1555:
				dst[0] = (uint8)(((p >> 11) & 0x1F) * 255 / 31);
				dst[1] = (uint8)((((p >> 6) & 0x03) | (((p >> 8) & 0x07) << 2)) * 255 / 31);
				dst[2] = (uint8)(((p >> 1) & 0x1F) * 255 / 31);
				dst[3] = (p & 0x0001) ? 255 : 0;
				break;
			default:
				dst[0] = dst[1] = dst[2] = 255;
				dst[3] = 255;
				break;
			}
			src += 2;
		}
		dst += 4;
	}
}

bool32
ensureTextureUploaded(Raster *raster)
{
	if(raster == nil || nativeRasterOffset == 0)
		return 0;

	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	if(natras->gpuReady && !natras->gpuDirty)
		return 1;

	queueTextureUpload(raster);
	return 0;
}

static void
retireTextureUploadGarbage(uint32 frame)
{
	if(frame >= MAX_FRAMES_IN_FLIGHT)
		return;

	FrameTextureUploadGarbage *garbage = &vkGlobals.textureUploadGarbage[frame];
	for(uint32 i = 0; i < garbage->numStaging; i++){
		if(garbage->raster[i]){
			VulkanRaster *natras = GETVULKANRASTEREXT(garbage->raster[i]);
			natras->uploadSubmitted = 0;
		}
		destroyBuffer(&garbage->staging[i]);
		garbage->raster[i] = nil;
	}
	garbage->numStaging = 0;
}

static void
removeTextureUploadGarbageForRaster(Raster *raster)
{
	Context *ctx = &vkGlobals.context;
	bool32 found = 0;
	for(uint32 frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++){
		FrameTextureUploadGarbage *garbage = &vkGlobals.textureUploadGarbage[frame];
		for(uint32 i = 0; i < garbage->numStaging; i++)
			if(garbage->raster[i] == raster)
				found = 1;
	}
	if(found && ctx->device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(ctx->device);

	for(uint32 frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++){
		FrameTextureUploadGarbage *garbage = &vkGlobals.textureUploadGarbage[frame];
		for(uint32 i = 0; i < garbage->numStaging; ){
			if(garbage->raster[i] != raster){
				i++;
				continue;
			}
			destroyBuffer(&garbage->staging[i]);
			garbage->numStaging--;
			if(i != garbage->numStaging){
				garbage->staging[i] = garbage->staging[garbage->numStaging];
				garbage->raster[i] = garbage->raster[garbage->numStaging];
			}
			memset(&garbage->staging[garbage->numStaging], 0, sizeof(garbage->staging[garbage->numStaging]));
			garbage->raster[garbage->numStaging] = nil;
		}
	}
}

static void
destroyTextureUploadResources(void)
{
	for(uint32 frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
		retireTextureUploadGarbage(frame);
	vkGlobals.numPendingTextureUploads = 0;
}

static bool32
keepTextureUploadStaging(uint32 frame, Raster *raster, VulkanBuffer *staging)
{
	if(frame >= MAX_FRAMES_IN_FLIGHT)
		return 0;
	FrameTextureUploadGarbage *garbage = &vkGlobals.textureUploadGarbage[frame];
	if(garbage->numStaging >= nelem(garbage->staging))
		return 0;

	garbage->staging[garbage->numStaging] = *staging;
	garbage->raster[garbage->numStaging] = raster;
	garbage->numStaging++;
	memset(staging, 0, sizeof(*staging));
	return 1;
}

static uint64
uploadElapsedUsecs(uint64 startCounter)
{
	uint64 frequency = SDL_GetPerformanceFrequency();
	if(frequency == 0)
		return 0;
	return (SDL_GetPerformanceCounter() - startCounter) * 1000000ull / frequency;
}

static bool32
uploadTextureForFrame(Raster *raster, VkCommandBuffer commandBuffer, VkDeviceSize *uploadedBytes)
{
	if(uploadedBytes)
		*uploadedBytes = 0;
	if(raster == nil || nativeRasterOffset == 0)
		return 1;

	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	if(natras->gpuReady && !natras->gpuDirty)
		return 1;

	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
		return 0;
	if(vkGlobals.descriptorPool == VK_NULL_HANDLE || vkGlobals.textureSetLayout == VK_NULL_HANDLE)
		return 0;
	if(natras->levels == nil)
		return 1;

	int32 width = natras->levels->levels[0].width;
	int32 height = natras->levels->levels[0].height;
	if(width <= 0 || height <= 0)
		return 1;

	uint8 *rgba = rwNewT(uint8, (uint32)width * height * 4, MEMDUR_FUNCTION | ID_DRIVER);
	if(rgba == nil)
		return 0;
	convertRasterToRGBA(raster, rgba);

	removeTextureUploadGarbageForRaster(raster);
	if(natras->image != VK_NULL_HANDLE || natras->imageMemory != VK_NULL_HANDLE ||
	   natras->imageView != VK_NULL_HANDLE || natras->sampler != VK_NULL_HANDLE)
		vkDeviceWaitIdle(ctx->device);
	destroyTextureHandles(&natras->image, &natras->imageMemory, &natras->imageView, &natras->sampler);
	natras->descriptorSet = VK_NULL_HANDLE;
	natras->imageFormat = VK_FORMAT_UNDEFINED;
	natras->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VulkanBuffer staging;
	bool32 ok = createTextureImageForFrame((uint32)width, (uint32)height, rgba, commandBuffer, &staging,
		&natras->image, &natras->imageMemory, &natras->imageView, &natras->sampler, &natras->descriptorSet);
	rwFree(rgba);
	if(ok && !keepTextureUploadStaging(ctx->currentFrame, raster, &staging)){
		destroyTextureHandles(&natras->image, &natras->imageMemory, &natras->imageView, &natras->sampler);
		natras->descriptorSet = VK_NULL_HANDLE;
		destroyBuffer(&staging);
		ok = 0;
	}
	natras->gpuReady = ok;
	natras->gpuDirty = !ok;
	natras->uploadSubmitted = ok;
	natras->imageFormat = ok ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED;
	natras->imageLayout = ok ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
	if(ok && uploadedBytes)
		*uploadedBytes = (VkDeviceSize)width * height * 4;
	return ok;
}

static void
queueTextureUpload(Raster *raster)
{
	if(raster == nil || nativeRasterOffset == 0)
		return;

	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	if(natras->gpuReady && !natras->gpuDirty)
		return;

	for(uint32 i = 0; i < vkGlobals.numPendingTextureUploads; i++)
		if(vkGlobals.pendingTextureUploads[i] == raster)
			return;

	if(vkGlobals.numPendingTextureUploads < MAX_PENDING_TEXTURE_UPLOADS)
		vkGlobals.pendingTextureUploads[vkGlobals.numPendingTextureUploads++] = raster;
}

static void
flushPendingTextureUploads(VkCommandBuffer commandBuffer)
{
	uint32 n = vkGlobals.numPendingTextureUploads;
	uint32 out = 0;
	uint32 uploadedCount = 0;
	VkDeviceSize uploadedBytes = 0;
	uint64 startCounter = SDL_GetPerformanceCounter();

	for(uint32 i = 0; i < n; i++){
		Raster *raster = vkGlobals.pendingTextureUploads[i];
		if(raster == nil)
			continue;

		VulkanRaster *natras = GETVULKANRASTEREXT(raster);
		if(natras->gpuReady && !natras->gpuDirty)
			continue;

		VkDeviceSize textureBytes = 0;
		if(natras->levels && natras->levels->numlevels > 0)
			textureBytes = (VkDeviceSize)natras->levels->levels[0].width *
				natras->levels->levels[0].height * 4;

		bool32 budgetFull = uploadedCount >= TEXTURE_UPLOADS_PER_FRAME ||
			(uploadedCount != 0 && uploadedBytes + textureBytes > TEXTURE_UPLOAD_BYTES_PER_FRAME) ||
			(uploadedCount != 0 && uploadElapsedUsecs(startCounter) >= TEXTURE_UPLOAD_USECS_PER_FRAME);
		if(commandBuffer == VK_NULL_HANDLE || budgetFull){
			vkGlobals.pendingTextureUploads[out++] = raster;
			continue;
		}

		VkDeviceSize bytesThisTexture = 0;
		if(uploadTextureForFrame(raster, commandBuffer, &bytesThisTexture)){
			uploadedCount++;
			uploadedBytes += bytesThisTexture;
		}else
			vkGlobals.pendingTextureUploads[out++] = raster;
	}
	vkGlobals.numPendingTextureUploads = out;
}

static VkDescriptorSet
getTextureDescriptor(Raster *raster)
{
	if(raster && nativeRasterOffset != 0){
		VulkanRaster *natras = GETVULKANRASTEREXT(raster);
		if(natras->gpuReady && !natras->gpuDirty && natras->descriptorSet != VK_NULL_HANDLE)
			return natras->descriptorSet;
	}
	queueTextureUpload(raster);
	return vkGlobals.whiteDescriptorSet;
}

static VkShaderModule
createShaderModule(const uint32 *code, size_t size)
{
	Context *ctx = &vkGlobals.context;
	VkShaderModuleCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = size;
	createInfo.pCode = code;

	VkShaderModule shader = VK_NULL_HANDLE;
	if(!vkOk(vkCreateShaderModule(ctx->device, &createInfo, nil, &shader), "vkCreateShaderModule"))
		return VK_NULL_HANDLE;
	return shader;
}

static VkPrimitiveTopology
mapTopology(PrimitiveType primType)
{
	switch(primType){
	case PRIMTYPELINELIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PRIMTYPEPOLYLINE: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case PRIMTYPETRILIST: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PRIMTYPETRISTRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case PRIMTYPETRIFAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	case PRIMTYPEPOINTLIST: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
}

static void
copyIdentity(float *m)
{
	memset(m, 0, sizeof(float)*16);
	m[0] = 1.0f;
	m[5] = 1.0f;
	m[10] = 1.0f;
	m[15] = 1.0f;
}

static void
multiplyMat4(float *dst, const float *a, const float *b)
{
	float out[16];
	for(int c = 0; c < 4; c++)
		for(int r = 0; r < 4; r++)
			out[c*4 + r] =
				a[0*4 + r]*b[c*4 + 0] +
				a[1*4 + r]*b[c*4 + 1] +
				a[2*4 + r]*b[c*4 + 2] +
				a[3*4 + r]*b[c*4 + 3];
	memcpy(dst, out, sizeof(out));
}

static void
makeRawMatrix(float *dst, Matrix *src)
{
	RawMatrix raw;
	convMatrix(&raw, src);
	memcpy(dst, &raw, sizeof(raw));
}

static void
makeMVP(float *dst, Matrix *world)
{
	float worldRaw[16], viewWorld[16];
	makeRawMatrix(worldRaw, world);
	multiplyMat4(viewWorld, vkGlobals.view, worldRaw);
	multiplyMat4(dst, vkGlobals.proj, viewWorld);
}

static void
colorToFloat(float *dst, const RGBA &color)
{
	RGBAf c;
	convColor(&c, &color);
	dst[0] = c.red;
	dst[1] = c.green;
	dst[2] = c.blue;
	dst[3] = c.alpha;
}

static uint32
getRenderStateUInt(int32 state, uint32 fallback)
{
	if(state < 0 || state > GSALPHATESTREF)
		return fallback;
	if(!vkGlobals.renderStateSet[state])
		return fallback;
	return (uint32)(uintptr)vkGlobals.renderStates[state];
}

static bool32
rasterHasAlpha(Raster *raster)
{
	if(raster == nil)
		return 0;
	if(nativeRasterOffset){
		VulkanRaster *natras = GETVULKANRASTEREXT(raster);
		if(natras->hasAlpha)
			return 1;
	}
	return Raster::formatHasAlpha(raster->format);
}

static bool32
geometryHasVertexAlpha(Geometry *geo)
{
	if(geo == nil || (geo->flags & Geometry::PRELIT) == 0 || geo->colors == nil)
		return 0;
	for(int32 i = 0; i < geo->numVertices; i++)
		if(geo->colors[i].alpha != 0xFF)
			return 1;
	return 0;
}

static RayGeometry*
findRayGeometry(Geometry *geo)
{
	for(uint32 i = 0; i < vkGlobals.numRayGeometries; i++)
		if(vkGlobals.rayGeometries[i].geometry == geo)
			return &vkGlobals.rayGeometries[i];
	return nil;
}

static RayGeometry*
createRayGeometryEntry(Geometry *geo)
{
	if(vkGlobals.numRayGeometries >= vkGlobals.rayGeometryCapacity){
		uint32 newCapacity = vkGlobals.rayGeometryCapacity ? vkGlobals.rayGeometryCapacity * 2 : 256;
		RayGeometry *newEntries = rwNewT(RayGeometry, newCapacity, MEMDUR_EVENT | ID_DRIVER);
		if(newEntries == nil)
			return nil;
		memset(newEntries, 0, sizeof(RayGeometry)*newCapacity);
		if(vkGlobals.rayGeometries){
			memcpy(newEntries, vkGlobals.rayGeometries, sizeof(RayGeometry)*vkGlobals.numRayGeometries);
			rwFree(vkGlobals.rayGeometries);
		}
		vkGlobals.rayGeometries = newEntries;
		vkGlobals.rayGeometryCapacity = newCapacity;
	}
	RayGeometry *entry = &vkGlobals.rayGeometries[vkGlobals.numRayGeometries++];
	memset(entry, 0, sizeof(*entry));
	entry->geometry = geo;
	return entry;
}

static RayGeometry*
getRayGeometry(Geometry *geo)
{
	if(!vkGlobals.rayResourcesReady || geo == nil || geo->meshHeader == nil ||
	   geo->numVertices <= 0 || geo->numTriangles <= 0 || geo->triangles == nil ||
	   geo->numMorphTargets <= 0 || geo->morphTargets == nil)
		return nil;

	RayGeometry *entry = findRayGeometry(geo);
	uint16 serial = geo->meshHeader->serialNum;
	if(entry && entry->blas.as != VK_NULL_HANDLE && entry->serialNumber == serial)
		return entry;
	if(entry == nil)
		entry = createRayGeometryEntry(geo);
	if(entry == nil)
		return nil;

	destroyRayAccelerationStructure(&entry->blas);
	entry->serialNumber = serial;
	uint32 numIndices = (uint32)geo->numTriangles * 3;
	uint16 *indices = rwNewT(uint16, numIndices, MEMDUR_FUNCTION | ID_DRIVER);
	if(indices == nil)
		return nil;
	for(int32 i = 0; i < geo->numTriangles; i++){
		indices[i*3 + 0] = geo->triangles[i].v[0];
		indices[i*3 + 1] = geo->triangles[i].v[1];
		indices[i*3 + 2] = geo->triangles[i].v[2];
	}

	bool32 ok = buildBottomLevelAccelerationStructure(&entry->blas,
		geo->morphTargets[0].vertices, (uint32)geo->numVertices, indices, (uint32)geo->numTriangles);
	rwFree(indices);
	if(!ok)
		return nil;
	return entry;
}

static void
registerRayInstance(Geometry *geo, Matrix *world)
{
	if(!vkGlobals.rayResourcesReady || vkGlobals.numRayPendingInstances >= MAX_RAY_PENDING_INSTANCES)
		return;
	RayGeometry *rayGeo = getRayGeometry(geo);
	if(rayGeo == nil || rayGeo->blas.as == VK_NULL_HANDLE)
		return;
	if(vkGlobals.numRayPendingInstances >= vkGlobals.rayPendingInstanceCapacity){
		uint32 newCapacity = vkGlobals.rayPendingInstanceCapacity ? vkGlobals.rayPendingInstanceCapacity * 2 : 512;
		if(newCapacity > MAX_RAY_PENDING_INSTANCES)
			newCapacity = MAX_RAY_PENDING_INSTANCES;
		RayInstanceRef *newInstances = rwNewT(RayInstanceRef, newCapacity, MEMDUR_EVENT | ID_DRIVER);
		if(newInstances == nil)
			return;
		if(vkGlobals.rayPendingInstances){
			memcpy(newInstances, vkGlobals.rayPendingInstances,
				sizeof(RayInstanceRef)*vkGlobals.numRayPendingInstances);
			rwFree(vkGlobals.rayPendingInstances);
		}
		vkGlobals.rayPendingInstances = newInstances;
		vkGlobals.rayPendingInstanceCapacity = newCapacity;
	}
	RayInstanceRef *inst = &vkGlobals.rayPendingInstances[vkGlobals.numRayPendingInstances++];
	inst->geometry = rayGeo;
	inst->transform = *world;
}

static void
makeAlphaRef(float *dst, bool32 enable)
{
	uint32 func = getRenderStateUInt(ALPHATESTFUNC, ALPHAGREATEREQUAL);
	float ref = getRenderStateUInt(ALPHATESTREF, 10) / 255.0f;

	if(!enable || func == ALPHAALWAYS){
		dst[0] = 0.0f;
		dst[1] = 2.0f;
	}else if(func == ALPHALESS){
		dst[0] = 0.0f;
		dst[1] = ref;
	}else{
		dst[0] = ref;
		dst[1] = 2.0f;
	}
	dst[2] = 0.0f;
	dst[3] = 0.0f;
}

static void setRenderState(int32 state, void *value);

static void
resetRenderState(void)
{
	memset(vkGlobals.renderStates, 0, sizeof(vkGlobals.renderStates));
	memset(vkGlobals.renderStateSet, 0, sizeof(vkGlobals.renderStateSet));
	setRenderState(SRCBLEND, (void*)(uintptr)BLENDSRCALPHA);
	setRenderState(DESTBLEND, (void*)(uintptr)BLENDINVSRCALPHA);
	setRenderState(ZTESTENABLE, (void*)(uintptr)1);
	setRenderState(ZWRITEENABLE, (void*)(uintptr)1);
	setRenderState(CULLMODE, (void*)(uintptr)CULLNONE);
	setRenderState(ALPHATESTFUNC, (void*)(uintptr)ALPHAGREATEREQUAL);
	setRenderState(ALPHATESTREF, (void*)(uintptr)10);
	setRenderState(GSALPHATESTREF, (void*)(uintptr)128);
}

static BlendPipelineMode
selectBlendMode(bool32 alpha)
{
	uint32 src = getRenderStateUInt(SRCBLEND, BLENDSRCALPHA);
	uint32 dst = getRenderStateUInt(DESTBLEND, BLENDINVSRCALPHA);

	if(src == BLENDZERO && dst == BLENDONE)
		return PIPE_BLEND_ZERO_ONE;
	if(src == BLENDONE && dst == BLENDZERO)
		return PIPE_BLEND_OPAQUE;
	if(src == BLENDONE && dst == BLENDONE)
		return PIPE_BLEND_ADD;
	if(src == BLENDSRCALPHA && dst == BLENDONE)
		return PIPE_BLEND_SRCALPHA_ADD;
	if(src == BLENDONE && dst == BLENDINVSRCALPHA)
		return PIPE_BLEND_ONE_INVSRCALPHA;
	if(src == BLENDZERO && dst == BLENDSRCCOLOR)
		return PIPE_BLEND_ZERO_SRCCOLOR;
	if(src == BLENDZERO && dst == BLENDINVSRCCOLOR)
		return PIPE_BLEND_ZERO_INVSRCCOLOR;
	if(src == BLENDINVDESTCOLOR && dst == BLENDZERO)
		return PIPE_BLEND_INVDESTCOLOR_ZERO;
	if(src == BLENDSRCALPHA && dst == BLENDINVDESTALPHA)
		return PIPE_BLEND_SRCALPHA_INVDESTALPHA;
	if(src == BLENDDESTALPHA && dst == BLENDINVDESTALPHA)
		return PIPE_BLEND_DESTALPHA_INVDESTALPHA;
	if(alpha || getRenderStateUInt(VERTEXALPHA, 0))
		return PIPE_BLEND_ALPHA;
	return PIPE_BLEND_OPAQUE;
}

static void
getBlendFactors(BlendPipelineMode blendMode, VkBlendFactor *src, VkBlendFactor *dst)
{
	switch(blendMode){
	case PIPE_BLEND_ADD:
		*src = VK_BLEND_FACTOR_ONE;
		*dst = VK_BLEND_FACTOR_ONE;
		break;
	case PIPE_BLEND_SRCALPHA_ADD:
		*src = VK_BLEND_FACTOR_SRC_ALPHA;
		*dst = VK_BLEND_FACTOR_ONE;
		break;
	case PIPE_BLEND_ONE_INVSRCALPHA:
		*src = VK_BLEND_FACTOR_ONE;
		*dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	case PIPE_BLEND_ZERO_ONE:
		*src = VK_BLEND_FACTOR_ZERO;
		*dst = VK_BLEND_FACTOR_ONE;
		break;
	case PIPE_BLEND_ZERO_SRCCOLOR:
		*src = VK_BLEND_FACTOR_ZERO;
		*dst = VK_BLEND_FACTOR_SRC_COLOR;
		break;
	case PIPE_BLEND_ZERO_INVSRCCOLOR:
		*src = VK_BLEND_FACTOR_ZERO;
		*dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		break;
	case PIPE_BLEND_INVDESTCOLOR_ZERO:
		*src = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		*dst = VK_BLEND_FACTOR_ZERO;
		break;
	case PIPE_BLEND_SRCALPHA_INVDESTALPHA:
		*src = VK_BLEND_FACTOR_SRC_ALPHA;
		*dst = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case PIPE_BLEND_DESTALPHA_INVDESTALPHA:
		*src = VK_BLEND_FACTOR_DST_ALPHA;
		*dst = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		break;
	case PIPE_BLEND_ALPHA:
	case PIPE_BLEND_OPAQUE:
	default:
		*src = VK_BLEND_FACTOR_SRC_ALPHA;
		*dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		break;
	}
}

static PipelineKind
selectIm2DPipelineKind(void)
{
	bool32 ztest = getRenderStateUInt(ZTESTENABLE, 1);
	bool32 zwrite = getRenderStateUInt(ZWRITEENABLE, 1);
	if(ztest)
		return zwrite ? PIPE_IM2D_ZTEST_ZWRITE : PIPE_IM2D_ZTEST;
	return zwrite ? PIPE_IM2D_ZWRITE : PIPE_IM2D;
}

static PipelineKind
selectColor3DPipelineKind(void)
{
	bool32 ztest = getRenderStateUInt(ZTESTENABLE, 1);
	bool32 zwrite = getRenderStateUInt(ZWRITEENABLE, 1);
	if(!ztest)
		return zwrite ? PIPE_COLOR3D_NOZTEST : PIPE_COLOR3D_NOZTEST_NOZWRITE;
	return zwrite ? PIPE_COLOR3D : PIPE_COLOR3D_NOZWRITE;
}

static bool32 createDrawPipelines(void);
static void destroyDrawPipelines(void);
static void destroyDrawResources(void);

static bool32
createTextureDescriptors(void)
{
	Context *ctx = &vkGlobals.context;

	VkDescriptorSetLayoutBinding samplerBinding;
	memset(&samplerBinding, 0, sizeof(samplerBinding));
	samplerBinding.binding = 0;
	samplerBinding.descriptorCount = 1;
	samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo;
	memset(&layoutInfo, 0, sizeof(layoutInfo));
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerBinding;
	if(!vkOk(vkCreateDescriptorSetLayout(ctx->device, &layoutInfo, nil, &vkGlobals.textureSetLayout),
	         "vkCreateDescriptorSetLayout"))
		return 0;

	if(ctx->rayQueryEnabled){
		VkDescriptorSetLayoutBinding rayBinding;
		memset(&rayBinding, 0, sizeof(rayBinding));
		rayBinding.binding = 0;
		rayBinding.descriptorCount = 1;
		rayBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		rayBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo rayLayoutInfo;
		memset(&rayLayoutInfo, 0, sizeof(rayLayoutInfo));
		rayLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		rayLayoutInfo.bindingCount = 1;
		rayLayoutInfo.pBindings = &rayBinding;
		if(!vkOk(vkCreateDescriptorSetLayout(ctx->device, &rayLayoutInfo, nil, &vkGlobals.raySetLayout),
		         "vkCreateDescriptorSetLayout(ray)"))
			return 0;
	}

	VkDescriptorPoolSize poolSizes[2];
	memset(poolSizes, 0, sizeof(poolSizes));
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 8192;
	uint32 poolSizeCount = 1;
	if(ctx->rayQueryEnabled){
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
		poolSizeCount = 2;
	}

	VkDescriptorPoolCreateInfo poolInfo;
	memset(&poolInfo, 0, sizeof(poolInfo));
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 8192 + (ctx->rayQueryEnabled ? MAX_FRAMES_IN_FLIGHT : 0);
	if(!vkOk(vkCreateDescriptorPool(ctx->device, &poolInfo, nil, &vkGlobals.descriptorPool),
	         "vkCreateDescriptorPool"))
		return 0;

	if(ctx->rayQueryEnabled){
		VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
		for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			layouts[i] = vkGlobals.raySetLayout;
		VkDescriptorSetAllocateInfo allocInfo;
		memset(&allocInfo, 0, sizeof(allocInfo));
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vkGlobals.descriptorPool;
		allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
		allocInfo.pSetLayouts = layouts;
		if(!vkOk(vkAllocateDescriptorSets(ctx->device, &allocInfo, vkGlobals.rayDescriptorSets),
		         "vkAllocateDescriptorSets(ray)"))
			return 0;
	}

	uint8 white[] = { 255, 255, 255, 255 };
	return createTextureImage(1, 1, white, &vkGlobals.whiteImage, &vkGlobals.whiteImageMemory,
		&vkGlobals.whiteImageView, &vkGlobals.whiteSampler, &vkGlobals.whiteDescriptorSet);
}

static void
updateRayDescriptorSet(uint32 frame, VkAccelerationStructureKHR as)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled || frame >= MAX_FRAMES_IN_FLIGHT ||
	   vkGlobals.rayDescriptorSets[frame] == VK_NULL_HANDLE || as == VK_NULL_HANDLE)
		return;

	VkWriteDescriptorSetAccelerationStructureKHR asInfo;
	memset(&asInfo, 0, sizeof(asInfo));
	asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asInfo.accelerationStructureCount = 1;
	asInfo.pAccelerationStructures = &as;

	VkWriteDescriptorSet write;
	memset(&write, 0, sizeof(write));
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = &asInfo;
	write.dstSet = vkGlobals.rayDescriptorSets[frame];
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nil);
}

static bool32
createDummyRayScene(void)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->rayQueryEnabled)
		return 0;
	V3d vertices[3] = {
		makeV3d(100000.0f, 100000.0f, 100000.0f),
		makeV3d(100001.0f, 100000.0f, 100000.0f),
		makeV3d(100000.0f, 100001.0f, 100000.0f)
	};
	uint16 indices[3] = { 0, 1, 2 };
	if(!buildBottomLevelAccelerationStructure(&vkGlobals.dummyBlas, vertices, 3, indices, 1))
		return 0;

	Matrix ident;
	ident.setIdentity();
	VkAccelerationStructureInstanceKHR instance;
	memset(&instance, 0, sizeof(instance));
	instance.transform = makeRayTransform(&ident);
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = getAccelerationStructureDeviceAddress(vkGlobals.dummyBlas.as);
	VulkanBuffer dummyScratch;
	memset(&dummyScratch, 0, sizeof(dummyScratch));
	uint32 dummyCapacity = 0;
	if(instance.accelerationStructureReference == 0 ||
	   !buildTopLevelAccelerationStructure(&vkGlobals.dummyTlas, &vkGlobals.dummyInstanceBuffer,
		   &dummyScratch, &dummyCapacity, &instance, 1, VK_NULL_HANDLE)){
		destroyBuffer(&dummyScratch);
		destroyRayAccelerationStructure(&vkGlobals.dummyBlas);
		destroyRayAccelerationStructure(&vkGlobals.dummyTlas);
		destroyBuffer(&vkGlobals.dummyInstanceBuffer);
		return 0;
	}
	destroyBuffer(&dummyScratch);
	for(uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		updateRayDescriptorSet(i, vkGlobals.dummyTlas.as);
	return 1;
}

static void
destroyRayResources(bool32 fullDestroy = 1)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(ctx->device);

	for(uint32 i = 0; i < vkGlobals.numRayGeometries; i++)
		destroyRayAccelerationStructure(&vkGlobals.rayGeometries[i].blas);
	rwFree(vkGlobals.rayGeometries);
	vkGlobals.rayGeometries = nil;
	vkGlobals.numRayGeometries = 0;
	vkGlobals.rayGeometryCapacity = 0;

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		destroyRayAccelerationStructure(&vkGlobals.rayFrames[i].tlas);
		destroyBuffer(&vkGlobals.rayFrames[i].instances);
		destroyBuffer(&vkGlobals.rayFrames[i].scratch);
		vkGlobals.rayFrames[i].capacity = 0;
		vkGlobals.rayFrames[i].valid = 0;
		if(fullDestroy)
			vkGlobals.rayDescriptorSets[i] = VK_NULL_HANDLE;
	}
	destroyRayAccelerationStructure(&vkGlobals.dummyBlas);
	destroyRayAccelerationStructure(&vkGlobals.dummyTlas);
	destroyBuffer(&vkGlobals.dummyInstanceBuffer);
	rwFree(vkGlobals.rayPendingInstances);
	vkGlobals.rayPendingInstances = nil;
	vkGlobals.numRayPendingInstances = 0;
	vkGlobals.rayPendingInstanceCapacity = 0;
	vkGlobals.rayResourcesReady = 0;
	vkGlobals.raySceneHasInstances = 0;
	vkGlobals.raySceneLogged = 0;
}

static bool32
createPipelineLayout(PipelineKind kind, uint32 pushSize)
{
	Context *ctx = &vkGlobals.context;
	VkPushConstantRange pushRange;
	memset(&pushRange, 0, sizeof(pushRange));
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = pushSize;

	VkPipelineLayoutCreateInfo layoutInfo;
	memset(&layoutInfo, 0, sizeof(layoutInfo));
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkDescriptorSetLayout layouts[2] = { vkGlobals.textureSetLayout, vkGlobals.raySetLayout };
	layoutInfo.setLayoutCount = vkGlobals.context.rayQueryEnabled && vkGlobals.raySetLayout != VK_NULL_HANDLE ? 2 : 1;
	layoutInfo.pSetLayouts = layouts;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;
	return vkOk(vkCreatePipelineLayout(ctx->device, &layoutInfo, nil, &vkGlobals.pipelineLayouts[kind]),
	            "vkCreatePipelineLayout");
}

static bool32
createGraphicsPipeline(PipelineKind kind, BlendPipelineMode blendMode, PrimitiveType primType,
	const uint32 *vertCode, size_t vertSize, const uint32 *fragCode, size_t fragSize,
	VkVertexInputBindingDescription *bindingDesc,
	VkVertexInputAttributeDescription *attribDescs, uint32 attribCount)
{
	Context *ctx = &vkGlobals.context;
	VkShaderModule vertShader = createShaderModule(vertCode, vertSize);
	VkShaderModule fragShader = createShaderModule(fragCode, fragSize);
	if(vertShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE)
		return 0;

	VkPipelineShaderStageCreateInfo stages[2];
	memset(stages, 0, sizeof(stages));
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertShader;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragShader;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInput;
	memset(&vertexInput, 0, sizeof(vertexInput));
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = bindingDesc;
	vertexInput.vertexAttributeDescriptionCount = attribCount;
	vertexInput.pVertexAttributeDescriptions = attribDescs;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	memset(&inputAssembly, 0, sizeof(inputAssembly));
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = mapTopology(primType);
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState;
	memset(&viewportState, 0, sizeof(viewportState));
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer;
	memset(&rasterizer, 0, sizeof(rasterizer));
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = (kind == PIPE_IM2D_ZTEST) ? VK_TRUE : VK_FALSE;
	rasterizer.depthBiasConstantFactor = (kind == PIPE_IM2D_ZTEST) ? -2.0f : 0.0f;
	rasterizer.depthBiasSlopeFactor = (kind == PIPE_IM2D_ZTEST) ? -2.0f : 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling;
	memset(&multisampling, 0, sizeof(multisampling));
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil;
	memset(&depthStencil, 0, sizeof(depthStencil));
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	bool32 depthTest = kind == PIPE_IM2D_ZTEST || kind == PIPE_IM2D_ZTEST_ZWRITE ||
		kind == PIPE_COLOR3D || kind == PIPE_COLOR3D_NOZWRITE;
	bool32 depthWrite = kind == PIPE_IM2D_ZWRITE || kind == PIPE_IM2D_ZTEST_ZWRITE ||
		kind == PIPE_COLOR3D || kind == PIPE_COLOR3D_NOZTEST;
	bool32 depthAlways = kind == PIPE_IM2D_ZWRITE || kind == PIPE_COLOR3D_NOZTEST;
	depthStencil.depthTestEnable = (depthTest || depthWrite) ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = depthAlways ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment;
	memset(&blendAttachment, 0, sizeof(blendAttachment));
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.blendEnable = blendMode == PIPE_BLEND_OPAQUE ? VK_FALSE : VK_TRUE;
	getBlendFactors(blendMode, &blendAttachment.srcColorBlendFactor, &blendAttachment.dstColorBlendFactor);
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcAlphaBlendFactor = blendAttachment.srcColorBlendFactor;
	blendAttachment.dstAlphaBlendFactor = blendAttachment.dstColorBlendFactor;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending;
	memset(&colorBlending, 0, sizeof(colorBlending));
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &blendAttachment;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState;
	memset(&dynamicState, 0, sizeof(dynamicState));
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo;
	memset(&pipelineInfo, 0, sizeof(pipelineInfo));
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = vkGlobals.pipelineLayouts[kind];
	pipelineInfo.renderPass = ctx->renderPass;
	pipelineInfo.subpass = 0;

	bool32 ok = vkOk(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nil,
		&vkGlobals.pipelines[kind][blendMode][primType]), "vkCreateGraphicsPipelines");
	vkDestroyShaderModule(ctx->device, vertShader, nil);
	vkDestroyShaderModule(ctx->device, fragShader, nil);
	return ok;
}

static bool32
createDrawPipelines(void)
{
	if(!createPipelineLayout(PIPE_IM2D, sizeof(Im2DPushConstants)) ||
	   !createPipelineLayout(PIPE_IM2D_ZTEST, sizeof(Im2DPushConstants)) ||
	   !createPipelineLayout(PIPE_IM2D_ZWRITE, sizeof(Im2DPushConstants)) ||
	   !createPipelineLayout(PIPE_IM2D_ZTEST_ZWRITE, sizeof(Im2DPushConstants)) ||
	   !createPipelineLayout(PIPE_COLOR3D, sizeof(Color3DPushConstants)) ||
	   !createPipelineLayout(PIPE_COLOR3D_NOZWRITE, sizeof(Color3DPushConstants)) ||
	   !createPipelineLayout(PIPE_COLOR3D_NOZTEST, sizeof(Color3DPushConstants)) ||
	   !createPipelineLayout(PIPE_COLOR3D_NOZTEST_NOZWRITE, sizeof(Color3DPushConstants)))
		return 0;

	VkVertexInputBindingDescription im2dBinding;
	memset(&im2dBinding, 0, sizeof(im2dBinding));
	im2dBinding.binding = 0;
	im2dBinding.stride = sizeof(Im2DVertex);
	im2dBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription im2dAttribs[3];
	memset(im2dAttribs, 0, sizeof(im2dAttribs));
	im2dAttribs[0].location = 0;
	im2dAttribs[0].binding = 0;
	im2dAttribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	im2dAttribs[0].offset = offsetof(Im2DVertex, x);
	im2dAttribs[1].location = 1;
	im2dAttribs[1].binding = 0;
	im2dAttribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	im2dAttribs[1].offset = offsetof(Im2DVertex, r);
	im2dAttribs[2].location = 2;
	im2dAttribs[2].binding = 0;
	im2dAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	im2dAttribs[2].offset = offsetof(Im2DVertex, u);

	VkVertexInputBindingDescription color3dBinding;
	memset(&color3dBinding, 0, sizeof(color3dBinding));
	color3dBinding.binding = 0;
	color3dBinding.stride = sizeof(Im3DVertex);
	color3dBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription color3dAttribs[4];
	memset(color3dAttribs, 0, sizeof(color3dAttribs));
	color3dAttribs[0].location = 0;
	color3dAttribs[0].binding = 0;
	color3dAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	color3dAttribs[0].offset = offsetof(Im3DVertex, position);
	color3dAttribs[1].location = 1;
	color3dAttribs[1].binding = 0;
	color3dAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	color3dAttribs[1].offset = offsetof(Im3DVertex, normal);
	color3dAttribs[2].location = 2;
	color3dAttribs[2].binding = 0;
	color3dAttribs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	color3dAttribs[2].offset = offsetof(Im3DVertex, r);
	color3dAttribs[3].location = 3;
	color3dAttribs[3].binding = 0;
	color3dAttribs[3].format = VK_FORMAT_R32G32_SFLOAT;
	color3dAttribs[3].offset = offsetof(Im3DVertex, u);

	PrimitiveType prims[] = {
		PRIMTYPELINELIST, PRIMTYPEPOLYLINE, PRIMTYPETRILIST,
		PRIMTYPETRISTRIP, PRIMTYPETRIFAN, PRIMTYPEPOINTLIST
	};
	bool32 useRayTracing = vkGlobals.rayTracingUserEnabled && vkGlobals.rayResourcesReady;
	const uint32 *color3dFrag = useRayTracing ? color3d_ray_frag_spv : color3d_frag_spv;
	size_t color3dFragSize = useRayTracing ? color3d_ray_frag_spv_size : color3d_frag_spv_size;
	for(uint32 i = 0; i < sizeof(prims)/sizeof(prims[0]); i++){
		for(int b = 0; b < PIPE_BLEND_COUNT; b++){
			if(!createGraphicsPipeline(PIPE_IM2D, (BlendPipelineMode)b, prims[i],
			      im2d_vert_spv, im2d_vert_spv_size, im2d_frag_spv, im2d_frag_spv_size,
			      &im2dBinding, im2dAttribs, 3))
				return 0;
			if(!createGraphicsPipeline(PIPE_IM2D_ZTEST, (BlendPipelineMode)b, prims[i],
			      im2d_vert_spv, im2d_vert_spv_size, im2d_frag_spv, im2d_frag_spv_size,
			      &im2dBinding, im2dAttribs, 3))
				return 0;
			if(!createGraphicsPipeline(PIPE_IM2D_ZWRITE, (BlendPipelineMode)b, prims[i],
			      im2d_vert_spv, im2d_vert_spv_size, im2d_frag_spv, im2d_frag_spv_size,
			      &im2dBinding, im2dAttribs, 3))
				return 0;
			if(!createGraphicsPipeline(PIPE_IM2D_ZTEST_ZWRITE, (BlendPipelineMode)b, prims[i],
			      im2d_vert_spv, im2d_vert_spv_size, im2d_frag_spv, im2d_frag_spv_size,
			      &im2dBinding, im2dAttribs, 3))
				return 0;
			if(!createGraphicsPipeline(PIPE_COLOR3D, (BlendPipelineMode)b, prims[i],
			      color3d_vert_spv, color3d_vert_spv_size, color3dFrag, color3dFragSize,
			      &color3dBinding, color3dAttribs, 4))
				return 0;
			if(!createGraphicsPipeline(PIPE_COLOR3D_NOZWRITE, (BlendPipelineMode)b, prims[i],
			      color3d_vert_spv, color3d_vert_spv_size, color3dFrag, color3dFragSize,
			      &color3dBinding, color3dAttribs, 4))
				return 0;
			if(!createGraphicsPipeline(PIPE_COLOR3D_NOZTEST, (BlendPipelineMode)b, prims[i],
			      color3d_vert_spv, color3d_vert_spv_size, color3dFrag, color3dFragSize,
			      &color3dBinding, color3dAttribs, 4))
				return 0;
			if(!createGraphicsPipeline(PIPE_COLOR3D_NOZTEST_NOZWRITE, (BlendPipelineMode)b, prims[i],
			      color3d_vert_spv, color3d_vert_spv_size, color3dFrag, color3dFragSize,
			      &color3dBinding, color3dAttribs, 4))
				return 0;
		}
	}
	return 1;
}

static void
destroyDrawPipelines(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE)
		return;
	for(int k = 0; k < PIPE_COUNT; k++){
		for(int b = 0; b < PIPE_BLEND_COUNT; b++){
			for(int i = 0; i < MAX_PRIM_PIPELINES; i++){
				if(vkGlobals.pipelines[k][b][i] != VK_NULL_HANDLE){
					vkDestroyPipeline(ctx->device, vkGlobals.pipelines[k][b][i], nil);
					vkGlobals.pipelines[k][b][i] = VK_NULL_HANDLE;
				}
			}
		}
		if(vkGlobals.pipelineLayouts[k] != VK_NULL_HANDLE){
			vkDestroyPipelineLayout(ctx->device, vkGlobals.pipelineLayouts[k], nil);
			vkGlobals.pipelineLayouts[k] = VK_NULL_HANDLE;
		}
	}
}

static bool32
createDrawResources(void)
{
	if(!createTextureDescriptors())
		return 0;
	if(vkGlobals.rayTracingUserEnabled && vkGlobals.context.rayQueryEnabled)
		vkGlobals.rayResourcesReady = createDummyRayScene();
	else
		vkGlobals.rayResourcesReady = 0;
	if(vkGlobals.context.rayQueryEnabled){
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
			"Vulkan ray tracing render %s by menu",
			vkGlobals.rayResourcesReady ? "enabled" : "disabled");
#else
		SDL_Log("Vulkan ray tracing render %s by menu",
			vkGlobals.rayResourcesReady ? "enabled" : "disabled");
#endif
	}
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		if(!ensureBuffer(&vkGlobals.dynamicVertexBuffers[i], DYNAMIC_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
		   !ensureBuffer(&vkGlobals.dynamicIndexBuffers[i], DYNAMIC_INDEX_BUFFER_SIZE, VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			return 0;
	}
	return createDrawPipelines();
}

static void
destroyDrawResources(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE)
		return;
	destroyDrawPipelines();
	destroyTextureUploadResources();
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		destroyBuffer(&vkGlobals.dynamicVertexBuffers[i]);
		destroyBuffer(&vkGlobals.dynamicIndexBuffers[i]);
	}
	vkGlobals.dynamicVertexOffset = 0;
	vkGlobals.dynamicIndexOffset = 0;
	destroyTextureHandles(&vkGlobals.whiteImage, &vkGlobals.whiteImageMemory,
		&vkGlobals.whiteImageView, &vkGlobals.whiteSampler);
	vkGlobals.whiteDescriptorSet = VK_NULL_HANDLE;
	destroyRayResources();
	if(vkGlobals.descriptorPool != VK_NULL_HANDLE){
		vkDestroyDescriptorPool(ctx->device, vkGlobals.descriptorPool, nil);
		vkGlobals.descriptorPool = VK_NULL_HANDLE;
	}
	if(vkGlobals.raySetLayout != VK_NULL_HANDLE){
		vkDestroyDescriptorSetLayout(ctx->device, vkGlobals.raySetLayout, nil);
		vkGlobals.raySetLayout = VK_NULL_HANDLE;
	}
	if(vkGlobals.textureSetLayout != VK_NULL_HANDLE){
		vkDestroyDescriptorSetLayout(ctx->device, vkGlobals.textureSetLayout, nil);
		vkGlobals.textureSetLayout = VK_NULL_HANDLE;
	}
	rwFree(vkGlobals.tempVertices);
	vkGlobals.tempVertices = nil;
	vkGlobals.tempVertexCapacity = 0;
	rwFree(vkGlobals.tempIndices);
	vkGlobals.tempIndices = nil;
	vkGlobals.tempIndexCapacity = 0;
}

static void
resetContext(Context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->instance = VK_NULL_HANDLE;
	ctx->surface = VK_NULL_HANDLE;
	ctx->physicalDevice = VK_NULL_HANDLE;
	ctx->device = VK_NULL_HANDLE;
	ctx->swapchain = VK_NULL_HANDLE;
	ctx->renderPass = VK_NULL_HANDLE;
	ctx->loadRenderPass = VK_NULL_HANDLE;
	ctx->commandPool = VK_NULL_HANDLE;
	ctx->graphicsQueueFamily = 0xFFFFFFFF;
	ctx->presentQueueFamily = 0xFFFFFFFF;
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		ctx->imageAvailable[i] = VK_NULL_HANDLE;
		ctx->renderFinished[i] = VK_NULL_HANDLE;
		ctx->inFlight[i] = VK_NULL_HANDLE;
	}
}

Context*
getContext(void)
{
	return &vkGlobals.context;
}

VkInstance getInstance(void) { return vkGlobals.context.instance; }
VkDevice getDevice(void) { return vkGlobals.context.device; }
VkPhysicalDevice getPhysicalDevice(void) { return vkGlobals.context.physicalDevice; }
VkSurfaceKHR getSurface(void) { return vkGlobals.context.surface; }
VkQueue getGraphicsQueue(void) { return vkGlobals.context.graphicsQueue; }
VkQueue getPresentQueue(void) { return vkGlobals.context.presentQueue; }
uint32 getGraphicsQueueFamily(void) { return vkGlobals.context.graphicsQueueFamily; }
uint32 getPresentQueueFamily(void) { return vkGlobals.context.presentQueueFamily; }
bool32 isRayQueryEnabled(void) { return vkGlobals.context.rayQueryEnabled; }
bool32 isRayTracingEnabled(void) { return vkGlobals.rayTracingUserEnabled && vkGlobals.rayResourcesReady; }

void
setRayTracingEnabled(bool32 enabled)
{
	Context *ctx = &vkGlobals.context;
	enabled = enabled != 0;
	if(vkGlobals.rayTracingUserEnabled == enabled)
		return;

	vkGlobals.rayTracingUserEnabled = enabled;
	if(ctx->device == VK_NULL_HANDLE || vkGlobals.textureSetLayout == VK_NULL_HANDLE)
		return;
	if(enabled && !ctx->rayQueryEnabled){
		vkGlobals.rayTracingUserEnabled = 0;
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
			"Vulkan ray tracing render disabled (ray query not enabled for this device session)");
#else
		SDL_Log("Vulkan ray tracing render disabled (ray query not enabled for this device session)");
#endif
		return;
	}

	vkDeviceWaitIdle(ctx->device);
	destroyDrawPipelines();
	destroyRayResources(0);
	if(enabled && ctx->rayQueryEnabled)
		vkGlobals.rayResourcesReady = createDummyRayScene();
	else
		vkGlobals.rayResourcesReady = 0;

	if(!createDrawPipelines()){
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_ERROR, "librw-vulkan",
			"Failed to rebuild Vulkan pipelines after ray tracing toggle");
#else
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"Failed to rebuild Vulkan pipelines after ray tracing toggle");
#endif
	}

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
		"Vulkan ray tracing render %s (%s)",
		enabled && vkGlobals.rayResourcesReady ? "enabled" : "disabled",
		ctx->rayQueryEnabled ? "ray query available" : "ray query unavailable");
#else
	SDL_Log("Vulkan ray tracing render %s (%s)",
		enabled && vkGlobals.rayResourcesReady ? "enabled" : "disabled",
		ctx->rayQueryEnabled ? "ray query available" : "ray query unavailable");
#endif
}

static void
addVideoMode(const SDL_DisplayMode *mode)
{
	for(int i = 1; i < vkGlobals.numModes; i++){
		if(vkGlobals.modes[i].mode.w == mode->w &&
		   vkGlobals.modes[i].mode.h == mode->h &&
		   vkGlobals.modes[i].mode.format == mode->format){
			if(mode->refresh_rate > vkGlobals.modes[i].mode.refresh_rate)
				vkGlobals.modes[i].mode.refresh_rate = mode->refresh_rate;
			return;
		}
	}

	vkGlobals.modes[vkGlobals.numModes].mode = *mode;
	vkGlobals.modes[vkGlobals.numModes].flags = VIDEOMODEEXCLUSIVE;
	vkGlobals.numModes++;
}

static void
makeVideoModeList(int displayIndex)
{
	int num = 0;
	const SDL_DisplayMode *currentMode;
	SDL_DisplayMode **modes;

	currentMode = SDL_GetCurrentDisplayMode(vkGlobals.displays[displayIndex]);
	modes = SDL_GetFullscreenDisplayModes(vkGlobals.displays[displayIndex], &num);

	rwFree(vkGlobals.modes);
	vkGlobals.modes = rwNewT(DisplayMode, num + (currentMode != nil ? 1 : 0) + 1, ID_DRIVER | MEMDUR_EVENT);
	vkGlobals.numModes = 0;

	if(currentMode){
		vkGlobals.modes[0].mode = *currentMode;
#if defined(__ANDROID__)
		vkGlobals.modes[0].flags = VIDEOMODEEXCLUSIVE;
#else
		vkGlobals.modes[0].flags = 0;
#endif
		vkGlobals.numModes = 1;
	}

	for(int i = 0; i < num; i++)
		addVideoMode(modes[i]);

	for(int i = 0; i < vkGlobals.numModes; i++){
		int depth = SDL_BITSPERPIXEL(vkGlobals.modes[i].mode.format);
		for(vkGlobals.modes[i].depth = 1; vkGlobals.modes[i].depth < depth; vkGlobals.modes[i].depth <<= 1);
	}
	SDL_free(modes);
	vkGlobals.currentMode = 0;
}

static int
openSDL3(EngineOpenParams *openparams)
{
	resetContext(&vkGlobals.context);
	vkGlobals.winWidth = openparams->width;
	vkGlobals.winHeight = openparams->height;
	vkGlobals.winTitle = openparams->windowtitle;
	vkGlobals.pWindow = openparams->window;
	vkGlobals.clearColor = makeRGBA(0, 0, 0, 255);
	vkGlobals.clearMode = Camera::CLEARIMAGE | Camera::CLEARZ;
	vkGlobals.commandReady = 0;
	memset(vkGlobals.renderStates, 0, sizeof(vkGlobals.renderStates));
	copyIdentity(vkGlobals.view);
	copyIdentity(vkGlobals.proj);
	vkGlobals.im3dWorld.setIdentity();

	if(!SDL_InitSubSystem(SDL_INIT_VIDEO)){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}
	if(!SDL_Vulkan_LoadLibrary(nil)){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}

	vkGlobals.currentDisplay = 0;
	vkGlobals.displays = SDL_GetDisplays(&vkGlobals.numDisplays);
	if(vkGlobals.displays == nil || vkGlobals.numDisplays <= 0){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}

	makeVideoModeList(vkGlobals.currentDisplay);
	return 1;
}

static int
closeSDL3(void)
{
	rwFree(vkGlobals.modes);
	vkGlobals.modes = nil;
	SDL_free(vkGlobals.displays);
	vkGlobals.displays = nil;
	SDL_Vulkan_UnloadLibrary();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 1;
}

static bool32
createInstance(void)
{
	Context *ctx = &vkGlobals.context;
	Uint32 extensionCount = 0;
	const char * const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	if(extensions == nil){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}

	VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "librw";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.pEngineName = "librw";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = VK_API_VERSION_1_0;
	PFN_vkEnumerateInstanceVersion enumerateInstanceVersion =
		(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
	if(enumerateInstanceVersion){
		uint32 loaderVersion = VK_API_VERSION_1_0;
		if(enumerateInstanceVersion(&loaderVersion) == VK_SUCCESS){
			if(apiVersionAtLeast(loaderVersion, 1, 2))
				appInfo.apiVersion = VK_API_VERSION_1_2;
			else if(apiVersionAtLeast(loaderVersion, 1, 1))
				appInfo.apiVersion = VK_API_VERSION_1_1;
		}
	}

	VkInstanceCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensions;

	return vkOk(vkCreateInstance(&createInfo, nil, &ctx->instance), "vkCreateInstance");
}

static bool32
createSurface(void)
{
	Context *ctx = &vkGlobals.context;
	if(!SDL_Vulkan_CreateSurface(ctx->window, ctx->instance, nil, &ctx->surface)){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}
	return 1;
}

static bool32
hasDeviceExtension(VkExtensionProperties *extensions, uint32 count, const char *name)
{
	for(uint32 i = 0; i < count; i++)
		if(strcmp(extensions[i].extensionName, name) == 0)
			return 1;
	return 0;
}

static bool32
enumerateDeviceExtensions(VkPhysicalDevice device, VkExtensionProperties **extensions, uint32 *count)
{
	*extensions = nil;
	*count = 0;
	if(!vkOk(vkEnumerateDeviceExtensionProperties(device, nil, count, nil), "vkEnumerateDeviceExtensionProperties"))
		return 0;
	if(*count == 0)
		return 1;
	*extensions = rwNewT(VkExtensionProperties, *count, MEMDUR_FUNCTION | ID_DRIVER);
	if(!vkOk(vkEnumerateDeviceExtensionProperties(device, nil, count, *extensions), "vkEnumerateDeviceExtensionProperties")){
		rwFree(*extensions);
		*extensions = nil;
		*count = 0;
		return 0;
	}
	return 1;
}

static bool32
loadRayQueryDeviceFunctions(void)
{
	Context *ctx = &vkGlobals.context;
	ctx->createAccelerationStructureKHR =
		(PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(ctx->device, "vkCreateAccelerationStructureKHR");
	ctx->destroyAccelerationStructureKHR =
		(PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(ctx->device, "vkDestroyAccelerationStructureKHR");
	ctx->getAccelerationStructureBuildSizesKHR =
		(PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(ctx->device, "vkGetAccelerationStructureBuildSizesKHR");
	ctx->cmdBuildAccelerationStructuresKHR =
		(PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(ctx->device, "vkCmdBuildAccelerationStructuresKHR");
	ctx->getAccelerationStructureDeviceAddressKHR =
		(PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(ctx->device, "vkGetAccelerationStructureDeviceAddressKHR");
	ctx->getBufferDeviceAddressKHR =
		(PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(ctx->device, "vkGetBufferDeviceAddressKHR");

	return ctx->createAccelerationStructureKHR != nil &&
	       ctx->destroyAccelerationStructureKHR != nil &&
	       ctx->getAccelerationStructureBuildSizesKHR != nil &&
	       ctx->cmdBuildAccelerationStructuresKHR != nil &&
	       ctx->getAccelerationStructureDeviceAddressKHR != nil &&
	       ctx->getBufferDeviceAddressKHR != nil;
}

static bool32
deviceHasSwapchain(VkPhysicalDevice device)
{
	uint32 count = 0;
	VkExtensionProperties *extensions = nil;
	if(!enumerateDeviceExtensions(device, &extensions, &count))
		return 0;
	bool32 hasSwapchain = hasDeviceExtension(extensions, count, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	rwFree(extensions);
	return hasSwapchain;
}

static bool32
findQueueFamilies(VkPhysicalDevice device, uint32 *graphicsFamily, uint32 *presentFamily)
{
	Context *ctx = &vkGlobals.context;
	uint32 count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nil);
	VkQueueFamilyProperties *families = rwNewT(VkQueueFamilyProperties, count, MEMDUR_FUNCTION | ID_DRIVER);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

	*graphicsFamily = 0xFFFFFFFF;
	*presentFamily = 0xFFFFFFFF;
	for(uint32 i = 0; i < count; i++){
		if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			*graphicsFamily = i;
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, ctx->surface, &presentSupport);
		if(presentSupport)
			*presentFamily = i;
		if(*graphicsFamily != 0xFFFFFFFF && *presentFamily != 0xFFFFFFFF)
			break;
	}
	rwFree(families);
	return *graphicsFamily != 0xFFFFFFFF && *presentFamily != 0xFFFFFFFF;
}

static bool32
selectPhysicalDevice(void)
{
	Context *ctx = &vkGlobals.context;
	uint32 count = 0;
	if(!vkOk(vkEnumeratePhysicalDevices(ctx->instance, &count, nil), "vkEnumeratePhysicalDevices") || count == 0)
		return 0;

	VkPhysicalDevice *devices = rwNewT(VkPhysicalDevice, count, MEMDUR_FUNCTION | ID_DRIVER);
	vkEnumeratePhysicalDevices(ctx->instance, &count, devices);

	for(uint32 i = 0; i < count; i++){
		uint32 graphicsFamily, presentFamily;
		if(deviceHasSwapchain(devices[i]) &&
		   findQueueFamilies(devices[i], &graphicsFamily, &presentFamily)){
			ctx->physicalDevice = devices[i];
			ctx->graphicsQueueFamily = graphicsFamily;
			ctx->presentQueueFamily = presentFamily;
			rwFree(devices);
			return 1;
		}
	}
	rwFree(devices);
	RWERROR((ERR_GENERAL, "No Vulkan device with graphics and present support"));
	return 0;
}

static bool32
createLogicalDevice(void)
{
	Context *ctx = &vkGlobals.context;
	float priority = 1.0f;
	VkDeviceQueueCreateInfo queues[2];
	uint32 queueCount = ctx->graphicsQueueFamily == ctx->presentQueueFamily ? 1 : 2;
	memset(queues, 0, sizeof(queues));
	queues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queues[0].queueFamilyIndex = ctx->graphicsQueueFamily;
	queues[0].queueCount = 1;
	queues[0].pQueuePriorities = &priority;
	if(queueCount == 2){
		queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queues[1].queueFamilyIndex = ctx->presentQueueFamily;
		queues[1].queueCount = 1;
		queues[1].pQueuePriorities = &priority;
	}

	VkPhysicalDeviceFeatures features;
	memset(&features, 0, sizeof(features));

	uint32 deviceExtensionCount = 0;
	VkExtensionProperties *deviceExtensions = nil;
	if(!enumerateDeviceExtensions(ctx->physicalDevice, &deviceExtensions, &deviceExtensionCount))
		return 0;

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(ctx->physicalDevice, &properties);
	bool32 deviceIsVulkan12 = apiVersionAtLeast(properties.apiVersion, 1, 2);
	bool32 hasDeferredHostOperations = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	bool32 hasAccelerationStructure = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	bool32 hasRayQuery = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_RAY_QUERY_EXTENSION_NAME);
	bool32 hasBufferDeviceAddress = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	bool32 hasDescriptorIndexing = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	bool32 hasSpirv14 = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_SPIRV_1_4_EXTENSION_NAME);
	bool32 hasShaderFloatControls = hasDeviceExtension(deviceExtensions, deviceExtensionCount, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	bool32 hasRayQueryExtensionDeps = deviceIsVulkan12 || (hasDescriptorIndexing && hasSpirv14 && hasShaderFloatControls);
	PFN_vkGetPhysicalDeviceFeatures2 getPhysicalDeviceFeatures2 =
		(PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(ctx->instance, "vkGetPhysicalDeviceFeatures2");
	if(getPhysicalDeviceFeatures2 == nil)
		getPhysicalDeviceFeatures2 =
			(PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(ctx->instance, "vkGetPhysicalDeviceFeatures2KHR");

	VkPhysicalDeviceFeatures2 supportedFeatures2;
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
	memset(&supportedFeatures2, 0, sizeof(supportedFeatures2));
	memset(&bufferDeviceAddressFeatures, 0, sizeof(bufferDeviceAddressFeatures));
	memset(&accelerationStructureFeatures, 0, sizeof(accelerationStructureFeatures));
	memset(&rayQueryFeatures, 0, sizeof(rayQueryFeatures));
	supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	supportedFeatures2.pNext = &bufferDeviceAddressFeatures;
	bufferDeviceAddressFeatures.pNext = &accelerationStructureFeatures;
	accelerationStructureFeatures.pNext = &rayQueryFeatures;

	if(hasDeferredHostOperations && hasAccelerationStructure && hasRayQuery &&
	   hasBufferDeviceAddress && hasRayQueryExtensionDeps && getPhysicalDeviceFeatures2)
		getPhysicalDeviceFeatures2(ctx->physicalDevice, &supportedFeatures2);

	ctx->deferredHostOperationsSupported = hasDeferredHostOperations;
	ctx->bufferDeviceAddressSupported = hasBufferDeviceAddress && bufferDeviceAddressFeatures.bufferDeviceAddress;
	ctx->accelerationStructureSupported = hasAccelerationStructure && accelerationStructureFeatures.accelerationStructure;
	ctx->rayQuerySupported = hasRayQuery && rayQueryFeatures.rayQuery &&
		ctx->deferredHostOperationsSupported && ctx->bufferDeviceAddressSupported &&
		ctx->accelerationStructureSupported && hasRayQueryExtensionDeps;

	const char *extensions[8];
	uint32 extensionCount = 0;
	extensions[extensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkPhysicalDeviceFeatures2 enabledFeatures2;
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures;
	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures;
	memset(&enabledFeatures2, 0, sizeof(enabledFeatures2));
	memset(&enabledBufferDeviceAddressFeatures, 0, sizeof(enabledBufferDeviceAddressFeatures));
	memset(&enabledAccelerationStructureFeatures, 0, sizeof(enabledAccelerationStructureFeatures));
	memset(&enabledRayQueryFeatures, 0, sizeof(enabledRayQueryFeatures));
	enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	enabledFeatures2.pNext = &enabledBufferDeviceAddressFeatures;
	enabledBufferDeviceAddressFeatures.pNext = &enabledAccelerationStructureFeatures;
	enabledAccelerationStructureFeatures.pNext = &enabledRayQueryFeatures;

	bool32 requestRayQuery = vkGlobals.rayTracingUserEnabled && ctx->rayQuerySupported;
	if(requestRayQuery){
		extensions[extensionCount++] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
		extensions[extensionCount++] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
		extensions[extensionCount++] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
		extensions[extensionCount++] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
		if(!deviceIsVulkan12){
			extensions[extensionCount++] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
			extensions[extensionCount++] = VK_KHR_SPIRV_1_4_EXTENSION_NAME;
			extensions[extensionCount++] = VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME;
		}
		enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		enabledRayQueryFeatures.rayQuery = VK_TRUE;
	}
	rwFree(deviceExtensions);

	VkDeviceCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = queueCount;
	createInfo.pQueueCreateInfos = queues;
	createInfo.pEnabledFeatures = requestRayQuery ? nil : &features;
	createInfo.pNext = requestRayQuery ? &enabledFeatures2 : nil;
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensions;

	if(!vkOk(vkCreateDevice(ctx->physicalDevice, &createInfo, nil, &ctx->device), "vkCreateDevice"))
		return 0;
	ctx->rayQueryEnabled = requestRayQuery && loadRayQueryDeviceFunctions();
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
		"Vulkan ray query %s (%s, %s, %s, %s, %s, %s)",
		ctx->rayQueryEnabled ? "enabled" : "disabled",
		vkGlobals.rayTracingUserEnabled ? "user on" : "user off",
		hasRayQuery ? "ray_query ext" : "no ray_query ext",
		ctx->accelerationStructureSupported ? "accel struct" : "no accel struct",
		ctx->bufferDeviceAddressSupported ? "buffer address" : "no buffer address",
		ctx->deferredHostOperationsSupported ? "deferred host ops" : "no deferred host ops",
		ctx->rayQueryEnabled ? "entrypoints" : "no entrypoints");
#else
	SDL_Log("Vulkan ray query %s (%s, %s, %s, %s, %s, %s)",
		ctx->rayQueryEnabled ? "enabled" : "disabled",
		vkGlobals.rayTracingUserEnabled ? "user on" : "user off",
		hasRayQuery ? "ray_query ext" : "no ray_query ext",
		ctx->accelerationStructureSupported ? "accel struct" : "no accel struct",
		ctx->bufferDeviceAddressSupported ? "buffer address" : "no buffer address",
		ctx->deferredHostOperationsSupported ? "deferred host ops" : "no deferred host ops",
		ctx->rayQueryEnabled ? "entrypoints" : "no entrypoints");
#endif

	vkGetDeviceQueue(ctx->device, ctx->graphicsQueueFamily, 0, &ctx->graphicsQueue);
	vkGetDeviceQueue(ctx->device, ctx->presentQueueFamily, 0, &ctx->presentQueue);
	return 1;
}

static VkSurfaceFormatKHR
chooseSurfaceFormat(VkSurfaceFormatKHR *formats, uint32 count)
{
	for(uint32 i = 0; i < count; i++){
		if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
		   formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return formats[i];
	}
	for(uint32 i = 0; i < count; i++){
		if(formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			return formats[i];
	}
	return formats[0];
}

static VkPresentModeKHR
presentModeFromPreference(int32 preference)
{
	switch(preference){
	case PRESENT_MODE_FIFO:
		return VK_PRESENT_MODE_FIFO_KHR;
	case PRESENT_MODE_FIFO_RELAXED:
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	case PRESENT_MODE_MAILBOX:
		return VK_PRESENT_MODE_MAILBOX_KHR;
	case PRESENT_MODE_IMMEDIATE:
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	default:
		return VK_PRESENT_MODE_MAX_ENUM_KHR;
	}
}

static const char*
presentModeName(VkPresentModeKHR mode)
{
	switch(mode){
	case VK_PRESENT_MODE_IMMEDIATE_KHR:
		return "IMMEDIATE";
	case VK_PRESENT_MODE_MAILBOX_KHR:
		return "MAILBOX";
	case VK_PRESENT_MODE_FIFO_KHR:
		return "FIFO";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
		return "FIFO_RELAXED";
	default:
		return "UNKNOWN";
	}
}

static const char*
presentModePreferenceName(int32 preference)
{
	switch(preference){
	case PRESENT_MODE_FIFO:
		return "FIFO";
	case PRESENT_MODE_FIFO_RELAXED:
		return "FIFO_RELAXED";
	case PRESENT_MODE_MAILBOX:
		return "MAILBOX";
	case PRESENT_MODE_IMMEDIATE:
		return "IMMEDIATE";
	default:
		return "AUTO";
	}
}

static bool32
presentModeAvailable(VkPresentModeKHR *modes, uint32 count, VkPresentModeKHR mode)
{
	for(uint32 i = 0; i < count; i++)
		if(modes[i] == mode)
			return 1;
	return 0;
}

static VkPresentModeKHR
chooseAutoPresentMode(VkPresentModeKHR *modes, uint32 count)
{
	if(presentModeAvailable(modes, count, VK_PRESENT_MODE_MAILBOX_KHR))
		return VK_PRESENT_MODE_MAILBOX_KHR;
	if(presentModeAvailable(modes, count, VK_PRESENT_MODE_FIFO_RELAXED_KHR))
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkPresentModeKHR
choosePresentMode(VkPresentModeKHR *modes, uint32 count)
{
	VkPresentModeKHR preferred = presentModeFromPreference(vkGlobals.presentModePreference);
	if(preferred != VK_PRESENT_MODE_MAX_ENUM_KHR && presentModeAvailable(modes, count, preferred))
		return preferred;
	return chooseAutoPresentMode(modes, count);
}

int32
getPresentModePreference(void)
{
	return vkGlobals.presentModePreference;
}

void
setPresentModePreference(int32 preference)
{
	if(preference < 0 || preference >= PRESENT_MODE_COUNT)
		preference = PRESENT_MODE_AUTO;
	if(vkGlobals.presentModePreference == preference)
		return;

	vkGlobals.presentModePreference = preference;
	if(vkGlobals.context.device != VK_NULL_HANDLE &&
	   vkGlobals.context.swapchain != VK_NULL_HANDLE)
		vkGlobals.swapchainRecreateRequested = 1;

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
		"Vulkan present mode preference set to %s",
		presentModePreferenceName(vkGlobals.presentModePreference));
#else
	fprintf(stderr, "Vulkan present mode preference set to %s\n",
		presentModePreferenceName(vkGlobals.presentModePreference));
#endif
}

static VkExtent2D
chooseExtent(VkSurfaceCapabilitiesKHR *caps)
{
	if(caps->currentExtent.width != 0xFFFFFFFF)
		return caps->currentExtent;

	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(vkGlobals.context.window, &w, &h);
	VkExtent2D extent;
	extent.width = w < 1 ? 1 : (uint32)w;
	extent.height = h < 1 ? 1 : (uint32)h;
	if(extent.width < caps->minImageExtent.width) extent.width = caps->minImageExtent.width;
	if(extent.width > caps->maxImageExtent.width) extent.width = caps->maxImageExtent.width;
	if(extent.height < caps->minImageExtent.height) extent.height = caps->minImageExtent.height;
	if(extent.height > caps->maxImageExtent.height) extent.height = caps->maxImageExtent.height;
	return extent;
}

static void destroySwapchain(void);

static VkFormat
chooseDepthFormat(void)
{
	VkFormat candidates[] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};
	for(uint32 i = 0; i < nelem(candidates); i++){
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(vkGlobals.context.physicalDevice, candidates[i], &props);
		if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return candidates[i];
	}
	return VK_FORMAT_D16_UNORM;
}

static bool32
createDepthResources(void)
{
	Context *ctx = &vkGlobals.context;
	ctx->depthFormat = chooseDepthFormat();

	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = ctx->swapchainExtent.width;
	imageInfo.extent.height = ctx->swapchainExtent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = ctx->depthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(!vkOk(vkCreateImage(ctx->device, &imageInfo, nil, &ctx->depthImage), "vkCreateImage(depth)"))
		return 0;

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(ctx->device, ctx->depthImage, &memRequirements);
	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if(!vkOk(vkAllocateMemory(ctx->device, &allocInfo, nil, &ctx->depthImageMemory), "vkAllocateMemory(depth)") ||
	   !vkOk(vkBindImageMemory(ctx->device, ctx->depthImage, ctx->depthImageMemory, 0), "vkBindImageMemory(depth)"))
		return 0;

	VkImageViewCreateInfo viewInfo;
	memset(&viewInfo, 0, sizeof(viewInfo));
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = ctx->depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = ctx->depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	return vkOk(vkCreateImageView(ctx->device, &viewInfo, nil, &ctx->depthImageView), "vkCreateImageView(depth)");
}

static void
destroyDepthResources(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->depthImageView != VK_NULL_HANDLE)
		vkDestroyImageView(ctx->device, ctx->depthImageView, nil);
	if(ctx->depthImage != VK_NULL_HANDLE)
		vkDestroyImage(ctx->device, ctx->depthImage, nil);
	if(ctx->depthImageMemory != VK_NULL_HANDLE)
		vkFreeMemory(ctx->device, ctx->depthImageMemory, nil);
	ctx->depthImageView = VK_NULL_HANDLE;
	ctx->depthImage = VK_NULL_HANDLE;
	ctx->depthImageMemory = VK_NULL_HANDLE;
}

static bool32
createOneRenderPass(VkAttachmentLoadOp colorLoadOp, VkAttachmentLoadOp depthLoadOp,
	VkImageLayout colorInitialLayout, VkImageLayout depthInitialLayout, VkRenderPass *renderPass)
{
	Context *ctx = &vkGlobals.context;
	VkAttachmentDescription colorAttachment;
	memset(&colorAttachment, 0, sizeof(colorAttachment));
	colorAttachment.format = ctx->swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = colorLoadOp;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = colorInitialLayout;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment;
	memset(&depthAttachment, 0, sizeof(depthAttachment));
	depthAttachment.format = ctx->depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = depthLoadOp;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = depthInitialLayout;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef;
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthRef;
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass;
	memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	VkSubpassDependency dependency;
	memset(&dependency, 0, sizeof(dependency));
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = nelem(attachments);
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	return vkOk(vkCreateRenderPass(ctx->device, &createInfo, nil, renderPass), "vkCreateRenderPass");
}

static bool32
createRenderPass(void)
{
	Context *ctx = &vkGlobals.context;
	return createOneRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED, &ctx->renderPass) &&
	       createOneRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		&ctx->loadRenderPass);
}

static bool32
createSwapchain(void)
{
	Context *ctx = &vkGlobals.context;
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physicalDevice, ctx->surface, &caps);

	uint32 formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physicalDevice, ctx->surface, &formatCount, nil);
	if(formatCount == 0)
		return 0;
	VkSurfaceFormatKHR *formats = rwNewT(VkSurfaceFormatKHR, formatCount, MEMDUR_FUNCTION | ID_DRIVER);
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physicalDevice, ctx->surface, &formatCount, formats);
	VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats, formatCount);
	rwFree(formats);

	uint32 presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physicalDevice, ctx->surface, &presentModeCount, nil);
	VkPresentModeKHR *presentModes = rwNewT(VkPresentModeKHR, presentModeCount, MEMDUR_FUNCTION | ID_DRIVER);
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physicalDevice, ctx->surface, &presentModeCount, presentModes);
	VkPresentModeKHR presentMode = choosePresentMode(presentModes, presentModeCount);
	vkGlobals.activePresentMode = presentMode;
	char presentModesText[192];
	presentModesText[0] = '\0';
	for(uint32 i = 0; i < presentModeCount; i++){
		size_t len = strlen(presentModesText);
		if(len < sizeof(presentModesText)-1)
			snprintf(presentModesText + len, sizeof(presentModesText) - len,
				"%s%s", i ? " " : "", presentModeName(presentModes[i]));
	}
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
		"Vulkan present mode %s (requested %s, available: %s)",
		presentModeName(presentMode),
		presentModePreferenceName(vkGlobals.presentModePreference),
		presentModesText);
#else
	fprintf(stderr, "Vulkan present mode %s (requested %s, available: %s)\n",
		presentModeName(presentMode),
		presentModePreferenceName(vkGlobals.presentModePreference),
		presentModesText);
#endif
	rwFree(presentModes);

	VkExtent2D extent = chooseExtent(&caps);
	uint32 imageCount = caps.minImageCount + 1;
	if(caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
		imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = ctx->surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	vkGlobals.canCopyFromSwapchain = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
	if(vkGlobals.canCopyFromSwapchain)
		createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	uint32 queueFamilyIndices[] = { ctx->graphicsQueueFamily, ctx->presentQueueFamily };
	if(ctx->graphicsQueueFamily != ctx->presentQueueFamily){
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}else{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	VkSurfaceTransformFlagBitsKHR preTransform =
		(caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ?
		VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	fprintf(stderr, "Vulkan swapchain extent=%ux%u current=%ux%u transform=0x%x pre=0x%x supported=0x%x\n",
		extent.width, extent.height, caps.currentExtent.width, caps.currentExtent.height,
		(uint32)caps.currentTransform, (uint32)preTransform, (uint32)caps.supportedTransforms);

	if(!vkOk(vkCreateSwapchainKHR(ctx->device, &createInfo, nil, &ctx->swapchain), "vkCreateSwapchainKHR"))
		return 0;

	ctx->swapchainFormat = surfaceFormat.format;
	ctx->swapchainExtent = extent;
	vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->numSwapchainImages, nil);
	ctx->swapchainImages = rwNewT(VkImage, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->numSwapchainImages, ctx->swapchainImages);

	ctx->swapchainImageViews = rwNewT(VkImageView, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	for(uint32 i = 0; i < ctx->numSwapchainImages; i++){
		VkImageViewCreateInfo viewInfo;
		memset(&viewInfo, 0, sizeof(viewInfo));
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = ctx->swapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = ctx->swapchainFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if(!vkOk(vkCreateImageView(ctx->device, &viewInfo, nil, &ctx->swapchainImageViews[i]), "vkCreateImageView"))
			return 0;
	}

	if(!createDepthResources() || !createRenderPass())
		return 0;

	ctx->framebuffers = rwNewT(VkFramebuffer, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	ctx->loadFramebuffers = rwNewT(VkFramebuffer, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	memset(ctx->framebuffers, 0, sizeof(VkFramebuffer)*ctx->numSwapchainImages);
	memset(ctx->loadFramebuffers, 0, sizeof(VkFramebuffer)*ctx->numSwapchainImages);
	for(uint32 i = 0; i < ctx->numSwapchainImages; i++){
		VkImageView attachments[] = { ctx->swapchainImageViews[i], ctx->depthImageView };
		VkFramebufferCreateInfo fbInfo;
		memset(&fbInfo, 0, sizeof(fbInfo));
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = ctx->renderPass;
		fbInfo.attachmentCount = nelem(attachments);
		fbInfo.pAttachments = attachments;
		fbInfo.width = ctx->swapchainExtent.width;
		fbInfo.height = ctx->swapchainExtent.height;
		fbInfo.layers = 1;
		if(!vkOk(vkCreateFramebuffer(ctx->device, &fbInfo, nil, &ctx->framebuffers[i]), "vkCreateFramebuffer"))
			return 0;
		fbInfo.renderPass = ctx->loadRenderPass;
		if(!vkOk(vkCreateFramebuffer(ctx->device, &fbInfo, nil, &ctx->loadFramebuffers[i]), "vkCreateFramebuffer(load)"))
			return 0;
	}

	if(ctx->commandPool == VK_NULL_HANDLE){
		VkCommandPoolCreateInfo poolInfo;
		memset(&poolInfo, 0, sizeof(poolInfo));
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = ctx->graphicsQueueFamily;
		if(!vkOk(vkCreateCommandPool(ctx->device, &poolInfo, nil, &ctx->commandPool), "vkCreateCommandPool"))
			return 0;
	}

	ctx->commandBuffers = rwNewT(VkCommandBuffer, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	VkCommandBufferAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = ctx->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = ctx->numSwapchainImages;
	if(!vkOk(vkAllocateCommandBuffers(ctx->device, &allocInfo, ctx->commandBuffers), "vkAllocateCommandBuffers"))
		return 0;

	ctx->imagesInFlight = rwNewT(VkFence, ctx->numSwapchainImages, MEMDUR_EVENT | ID_DRIVER);
	if(ctx->imagesInFlight == nil)
		return 0;
	memset(ctx->imagesInFlight, 0, sizeof(VkFence)*ctx->numSwapchainImages);

	return 1;
}

static void
destroySwapchain(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device == VK_NULL_HANDLE)
		return;

	if(ctx->commandBuffers){
		vkFreeCommandBuffers(ctx->device, ctx->commandPool, ctx->numSwapchainImages, ctx->commandBuffers);
		rwFree(ctx->commandBuffers);
		ctx->commandBuffers = nil;
	}
	rwFree(ctx->imagesInFlight);
	ctx->imagesInFlight = nil;
	if(ctx->framebuffers){
		for(uint32 i = 0; i < ctx->numSwapchainImages; i++)
			if(ctx->framebuffers[i] != VK_NULL_HANDLE)
				vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], nil);
		rwFree(ctx->framebuffers);
		ctx->framebuffers = nil;
	}
	if(ctx->loadFramebuffers){
		for(uint32 i = 0; i < ctx->numSwapchainImages; i++)
			if(ctx->loadFramebuffers[i] != VK_NULL_HANDLE)
				vkDestroyFramebuffer(ctx->device, ctx->loadFramebuffers[i], nil);
		rwFree(ctx->loadFramebuffers);
		ctx->loadFramebuffers = nil;
	}
	if(ctx->renderPass != VK_NULL_HANDLE){
		vkDestroyRenderPass(ctx->device, ctx->renderPass, nil);
		ctx->renderPass = VK_NULL_HANDLE;
	}
	if(ctx->loadRenderPass != VK_NULL_HANDLE){
		vkDestroyRenderPass(ctx->device, ctx->loadRenderPass, nil);
		ctx->loadRenderPass = VK_NULL_HANDLE;
	}
	destroyDepthResources();
	if(ctx->swapchainImageViews){
		for(uint32 i = 0; i < ctx->numSwapchainImages; i++)
			if(ctx->swapchainImageViews[i] != VK_NULL_HANDLE)
				vkDestroyImageView(ctx->device, ctx->swapchainImageViews[i], nil);
		rwFree(ctx->swapchainImageViews);
		ctx->swapchainImageViews = nil;
	}
	rwFree(ctx->swapchainImages);
	ctx->swapchainImages = nil;
	if(ctx->swapchain != VK_NULL_HANDLE){
		vkDestroySwapchainKHR(ctx->device, ctx->swapchain, nil);
		ctx->swapchain = VK_NULL_HANDLE;
	}
	ctx->numSwapchainImages = 0;
}

static bool32
createSyncObjects(void)
{
	Context *ctx = &vkGlobals.context;
	VkSemaphoreCreateInfo semInfo;
	memset(&semInfo, 0, sizeof(semInfo));
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo;
	memset(&fenceInfo, 0, sizeof(fenceInfo));
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		if(!vkOk(vkCreateSemaphore(ctx->device, &semInfo, nil, &ctx->imageAvailable[i]), "vkCreateSemaphore") ||
		   !vkOk(vkCreateSemaphore(ctx->device, &semInfo, nil, &ctx->renderFinished[i]), "vkCreateSemaphore") ||
		   !vkOk(vkCreateFence(ctx->device, &fenceInfo, nil, &ctx->inFlight[i]), "vkCreateFence"))
			return 0;
	}
	return 1;
}

static void
destroySyncObjects(void)
{
	Context *ctx = &vkGlobals.context;
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
		if(ctx->imageAvailable[i] != VK_NULL_HANDLE)
			vkDestroySemaphore(ctx->device, ctx->imageAvailable[i], nil);
		if(ctx->renderFinished[i] != VK_NULL_HANDLE)
			vkDestroySemaphore(ctx->device, ctx->renderFinished[i], nil);
		if(ctx->inFlight[i] != VK_NULL_HANDLE)
			vkDestroyFence(ctx->device, ctx->inFlight[i], nil);
		ctx->imageAvailable[i] = VK_NULL_HANDLE;
		ctx->renderFinished[i] = VK_NULL_HANDLE;
		ctx->inFlight[i] = VK_NULL_HANDLE;
	}
}

static bool32
recreateSwapchain(void)
{
	Context *ctx = &vkGlobals.context;
	vkDeviceWaitIdle(ctx->device);
	destroyDrawPipelines();
	destroySwapchain();
	return createSwapchain() && createDrawPipelines();
}

static void
buildRaySceneForCurrentFrame(VkCommandBuffer commandBuffer)
{
	Context *ctx = &vkGlobals.context;
	if(!vkGlobals.rayResourcesReady || commandBuffer == VK_NULL_HANDLE){
		vkGlobals.numRayPendingInstances = 0;
		vkGlobals.raySceneHasInstances = 0;
		return;
	}

	uint32 frame = ctx->currentFrame;
	if(vkGlobals.numRayPendingInstances == 0){
		updateRayDescriptorSet(frame, vkGlobals.dummyTlas.as);
		vkGlobals.raySceneHasInstances = 0;
		return;
	}

	VkAccelerationStructureInstanceKHR *instances =
		rwNewT(VkAccelerationStructureInstanceKHR, vkGlobals.numRayPendingInstances,
			MEMDUR_FUNCTION | ID_DRIVER);
	if(instances == nil){
		vkGlobals.numRayPendingInstances = 0;
		updateRayDescriptorSet(frame, vkGlobals.dummyTlas.as);
		vkGlobals.raySceneHasInstances = 0;
		return;
	}

	uint32 count = 0;
	for(uint32 i = 0; i < vkGlobals.numRayPendingInstances; i++){
		RayInstanceRef *src = &vkGlobals.rayPendingInstances[i];
		if(src->geometry == nil || src->geometry->blas.as == VK_NULL_HANDLE)
			continue;
		VkDeviceAddress blasAddress = getAccelerationStructureDeviceAddress(src->geometry->blas.as);
		if(blasAddress == 0)
			continue;
		VkAccelerationStructureInstanceKHR *dst = &instances[count++];
		memset(dst, 0, sizeof(*dst));
		dst->transform = makeRayTransform(&src->transform);
		dst->instanceCustomIndex = count & 0xFFFFFF;
		dst->mask = 0xFF;
		dst->instanceShaderBindingTableRecordOffset = 0;
		dst->flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		dst->accelerationStructureReference = blasAddress;
	}
	vkGlobals.numRayPendingInstances = 0;

	if(count == 0 ||
	   !buildTopLevelAccelerationStructure(&vkGlobals.rayFrames[frame].tlas,
		   &vkGlobals.rayFrames[frame].instances, &vkGlobals.rayFrames[frame].scratch,
		   &vkGlobals.rayFrames[frame].capacity, instances, count, commandBuffer)){
		rwFree(instances);
		updateRayDescriptorSet(frame, vkGlobals.dummyTlas.as);
		vkGlobals.rayFrames[frame].valid = 0;
		vkGlobals.raySceneHasInstances = 0;
		return;
	}

	rwFree(instances);
	vkGlobals.rayFrames[frame].valid = 1;
	vkGlobals.raySceneHasInstances = 1;
	updateRayDescriptorSet(frame, vkGlobals.rayFrames[frame].tlas.as);
	if(!vkGlobals.raySceneLogged){
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, "librw-vulkan",
			"Vulkan ray TLAS built (%u instances)", count);
#else
		SDL_Log("Vulkan ray TLAS built (%u instances)", count);
#endif
		vkGlobals.raySceneLogged = 1;
	}
}

static void
resetCommandBindings(void)
{
	vkGlobals.currentPipeline = VK_NULL_HANDLE;
	vkGlobals.currentDescriptorSet = VK_NULL_HANDLE;
	vkGlobals.currentDescriptorSetKind = PIPE_COUNT;
}

static void
setSwapchainViewportAndScissor(void)
{
	Context *ctx = &vkGlobals.context;
	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)ctx->swapchainExtent.width;
	viewport.height = (float)ctx->swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(ctx->commandBuffers[ctx->currentImage], 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = ctx->swapchainExtent;
	vkCmdSetScissor(ctx->commandBuffers[ctx->currentImage], 0, 1, &scissor);
}

static void
beginSwapchainRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, bool32 clear)
{
	Context *ctx = &vkGlobals.context;
	VkClearValue clearValues[2];
	memset(clearValues, 0, sizeof(clearValues));
	if(clear){
		RGBAf clearColor;
		convColor(&clearColor, &vkGlobals.clearColor);
		clearValues[0].color.float32[0] = clearColor.red;
		clearValues[0].color.float32[1] = clearColor.green;
		clearValues[0].color.float32[2] = clearColor.blue;
		clearValues[0].color.float32[3] = clearColor.alpha;
		clearValues[1].depthStencil.depth = 1.0f;
		clearValues[1].depthStencil.stencil = 0;
	}

	VkRenderPassBeginInfo rpInfo;
	memset(&rpInfo, 0, sizeof(rpInfo));
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.renderPass = renderPass;
	rpInfo.framebuffer = framebuffer;
	rpInfo.renderArea.offset.x = 0;
	rpInfo.renderArea.offset.y = 0;
	rpInfo.renderArea.extent = ctx->swapchainExtent;
	rpInfo.clearValueCount = clear ? nelem(clearValues) : 0;
	rpInfo.pClearValues = clear ? clearValues : nil;
	vkCmdBeginRenderPass(ctx->commandBuffers[ctx->currentImage], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
	setSwapchainViewportAndScissor();
	resetCommandBindings();
}

static int
startSDL3(void)
{
	Context *ctx = &vkGlobals.context;
	DisplayMode *mode = &vkGlobals.modes[vkGlobals.currentMode];
	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;

	if(mode->flags & VIDEOMODEEXCLUSIVE) {
		flags |= SDL_WINDOW_FULLSCREEN;
		ctx->window = SDL_CreateWindow(vkGlobals.winTitle, mode->mode.w, mode->mode.h, flags);
		if(ctx->window)
			SDL_SetWindowFullscreenMode(ctx->window, &mode->mode);
	}else{
		ctx->window = SDL_CreateWindow(vkGlobals.winTitle, vkGlobals.winWidth, vkGlobals.winHeight, flags);
		if(ctx->window)
			SDL_SetWindowFullscreenMode(ctx->window, nil);
	}
	if(ctx->window == nil){
		RWERROR((ERR_GENERAL, SDL_GetError()));
		return 0;
	}

	if(!createInstance() || !createSurface() || !selectPhysicalDevice() ||
	   !createLogicalDevice() || !createSwapchain() || !createSyncObjects() || !createDrawResources())
		return 0;

	resetRenderState();
	*vkGlobals.pWindow = ctx->window;
	return 1;
}

static int
stopSDL3(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(ctx->device);
	destroyDrawResources();
	destroySyncObjects();
	destroySwapchain();
	if(ctx->commandPool != VK_NULL_HANDLE)
		vkDestroyCommandPool(ctx->device, ctx->commandPool, nil);
	if(ctx->device != VK_NULL_HANDLE)
		vkDestroyDevice(ctx->device, nil);
	if(ctx->surface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(ctx->instance, ctx->surface, nil);
	if(ctx->instance != VK_NULL_HANDLE)
		vkDestroyInstance(ctx->instance, nil);
	if(ctx->window)
		SDL_DestroyWindow(ctx->window);
	resetContext(ctx);
	return 1;
}

static bool32
beginFrame(void)
{
	Context *ctx = &vkGlobals.context;
	if(ctx->frameStarted)
		return 1;

	if(vkGlobals.swapchainRecreateRequested){
		vkGlobals.swapchainRecreateRequested = 0;
		if(!recreateSwapchain())
			return 0;
	}

	resetCommandBindings();

	vkWaitForFences(ctx->device, 1, &ctx->inFlight[ctx->currentFrame], VK_TRUE, UINT64_MAX);
	retireTextureUploadGarbage(ctx->currentFrame);

	VkResult result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX,
		ctx->imageAvailable[ctx->currentFrame], VK_NULL_HANDLE, &ctx->currentImage);
	if(result == VK_ERROR_OUT_OF_DATE_KHR)
		return recreateSwapchain() && beginFrame();
	if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		return vkOk(result, "vkAcquireNextImageKHR");

	if(ctx->imagesInFlight && ctx->imagesInFlight[ctx->currentImage] != VK_NULL_HANDLE)
		vkWaitForFences(ctx->device, 1, &ctx->imagesInFlight[ctx->currentImage], VK_TRUE, UINT64_MAX);
	if(ctx->imagesInFlight)
		ctx->imagesInFlight[ctx->currentImage] = ctx->inFlight[ctx->currentFrame];

	vkResetFences(ctx->device, 1, &ctx->inFlight[ctx->currentFrame]);
	vkResetCommandBuffer(ctx->commandBuffers[ctx->currentImage], 0);

	VkCommandBufferBeginInfo beginInfo;
	memset(&beginInfo, 0, sizeof(beginInfo));
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if(!vkOk(vkBeginCommandBuffer(ctx->commandBuffers[ctx->currentImage], &beginInfo), "vkBeginCommandBuffer"))
		return 0;

	flushPendingTextureUploads(ctx->commandBuffers[ctx->currentImage]);
	buildRaySceneForCurrentFrame(ctx->commandBuffers[ctx->currentImage]);
	beginSwapchainRenderPass(ctx->renderPass, ctx->framebuffers[ctx->currentImage], 1);

	ctx->frameStarted = 1;
	vkGlobals.commandReady = 0;
	vkGlobals.dynamicVertexOffset = 0;
	vkGlobals.dynamicIndexOffset = 0;
	return 1;
}

static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  = -inv.at.x;
	view[9]  =  inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, &view, sizeof(RawMatrix));
	memcpy(vkGlobals.view, view, sizeof(view));

	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);
	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;
	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;
	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = cam->farPlane*invz;
		proj[11] = 1.0f;
		proj[15] = 0.0f;
	}else{
		proj[10] = invz;
		proj[11] = 0.0f;
		proj[15] = 1.0f;
	}
	proj[14] = -cam->nearPlane*proj[10];
	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));
	memcpy(vkGlobals.proj, proj, sizeof(proj));

	engine->currentCamera = cam;
	if(cam->frameBuffer && cam->frameBuffer->type == Raster::CAMERATEXTURE)
		return;
	beginFrame();
}

static void
endUpdate(Camera *cam)
{
	if(cam && cam->frameBuffer && cam->frameBuffer->type == Raster::CAMERATEXTURE)
		return;
	Context *ctx = &vkGlobals.context;
	if(!ctx->frameStarted || vkGlobals.commandReady)
		return;
	vkCmdEndRenderPass(ctx->commandBuffers[ctx->currentImage]);
	vkOk(vkEndCommandBuffer(ctx->commandBuffers[ctx->currentImage]), "vkEndCommandBuffer");
	vkGlobals.commandReady = 1;
}

static void
clearCamera(Camera*, RGBA *col, uint32 mode)
{
	vkGlobals.clearColor = *col;
	vkGlobals.clearMode = mode;
}

static void
showRaster(Raster*, uint32 flags)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->frameStarted)
		return;
	if(!vkGlobals.commandReady)
		endUpdate(nil);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(submitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &ctx->imageAvailable[ctx->currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &ctx->commandBuffers[ctx->currentImage];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &ctx->renderFinished[ctx->currentFrame];

	if(!vkOk(vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, ctx->inFlight[ctx->currentFrame]), "vkQueueSubmit"))
		return;

	VkPresentInfoKHR presentInfo;
	memset(&presentInfo, 0, sizeof(presentInfo));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &ctx->renderFinished[ctx->currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &ctx->swapchain;
	presentInfo.pImageIndices = &ctx->currentImage;

	VkResult result = vkQueuePresentKHR(ctx->presentQueue, &presentInfo);
	if(result == VK_ERROR_OUT_OF_DATE_KHR)
		recreateSwapchain();
	else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		vkOk(result, "vkQueuePresentKHR");

	ctx->currentFrame = (ctx->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	ctx->frameStarted = 0;
	vkGlobals.commandReady = 0;
}

static bool32
rasterCopyTargetReady(Raster *raster)
{
	if(raster == nil || nativeRasterOffset == 0)
		return 0;
	if(raster->type != Raster::NORMAL &&
	   raster->type != Raster::TEXTURE &&
	   raster->type != Raster::CAMERATEXTURE)
		return 0;

	Context *ctx = &vkGlobals.context;
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	if(natras->image != VK_NULL_HANDLE &&
	   natras->descriptorSet != VK_NULL_HANDLE &&
	   natras->imageFormat == ctx->swapchainFormat)
		return 1;

	destroyRasterTexture(raster);
	if(!createEmptySampledImage((uint32)raster->width, (uint32)raster->height, ctx->swapchainFormat,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT, &natras->image, &natras->imageMemory,
		&natras->imageView, &natras->sampler, &natras->descriptorSet)){
		destroyRasterTexture(raster);
		return 0;
	}

	natras->imageFormat = ctx->swapchainFormat;
	natras->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	natras->gpuReady = 1;
	natras->gpuDirty = 0;
	return 1;
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	Context *ctx = &vkGlobals.context;
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	if(src == nil || dst == nil || src->type != Raster::CAMERA)
		return 0;
	if(!ctx->frameStarted || vkGlobals.commandReady || !vkGlobals.canCopyFromSwapchain)
		return 0;
	if(x < 0 || y < 0 || x >= dst->width || y >= dst->height)
		return 0;
	if(!rasterCopyTargetReady(dst))
		return 0;

	VulkanRaster *natdst = GETVULKANRASTEREXT(dst);
	uint32 copyWidth = (uint32)src->width;
	uint32 copyHeight = (uint32)src->height;
	if(copyWidth > ctx->swapchainExtent.width)
		copyWidth = ctx->swapchainExtent.width;
	if(copyHeight > ctx->swapchainExtent.height)
		copyHeight = ctx->swapchainExtent.height;
	if(copyWidth > (uint32)(dst->width - x))
		copyWidth = (uint32)(dst->width - x);
	if(copyHeight > (uint32)(dst->height - y))
		copyHeight = (uint32)(dst->height - y);
	if(copyWidth == 0 || copyHeight == 0)
		return 0;

	VkCommandBuffer commandBuffer = ctx->commandBuffers[ctx->currentImage];
	vkCmdEndRenderPass(commandBuffer);
	resetCommandBindings();

	transitionImageLayout(commandBuffer, ctx->swapchainImages[ctx->currentImage],
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	transitionImageLayout(commandBuffer, natdst->image, natdst->imageLayout,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageCopy region;
	memset(&region, 0, sizeof(region));
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.mipLevel = 0;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.mipLevel = 0;
	region.dstSubresource.baseArrayLayer = 0;
	region.dstSubresource.layerCount = 1;
	region.dstOffset.x = x;
	region.dstOffset.y = y;
	region.extent.width = copyWidth;
	region.extent.height = copyHeight;
	region.extent.depth = 1;
	vkCmdCopyImage(commandBuffer,
		ctx->swapchainImages[ctx->currentImage], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		natdst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	transitionImageLayout(commandBuffer, natdst->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	natdst->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	transitionImageLayout(commandBuffer, ctx->swapchainImages[ctx->currentImage],
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	beginSwapchainRenderPass(ctx->loadRenderPass, ctx->loadFramebuffers[ctx->currentImage], 0);
	return 1;
}

static void setRenderState(int32 state, void *value)
{
	if(state >= 0 && state <= GSALPHATESTREF){
		vkGlobals.renderStates[state] = value;
		vkGlobals.renderStateSet[state] = 1;
		if(state == TEXTURERASTER && value && !ensureTextureUploaded((Raster*)value))
			queueTextureUpload((Raster*)value);
	}
}

static void *getRenderState(int32 state)
{
	if(state >= 0 && state <= GSALPHATESTREF)
		return vkGlobals.renderStates[state];
	return nil;
}

static bool32
validDrawState(PipelineKind kind, PrimitiveType primType)
{
	Context *ctx = &vkGlobals.context;
	if(!ctx->frameStarted || primType <= PRIMTYPENONE || primType >= MAX_PRIM_PIPELINES)
		return 0;
	return vkGlobals.pipelineLayouts[kind] != VK_NULL_HANDLE;
}

static VkPipeline
getDrawPipeline(PipelineKind kind, PrimitiveType primType, bool32 alpha)
{
	return vkGlobals.pipelines[kind][selectBlendMode(alpha)][primType];
}

static void
bindTextureSet(PipelineKind kind, VkDescriptorSet set)
{
	Context *ctx = &vkGlobals.context;
	if(set == VK_NULL_HANDLE)
		set = vkGlobals.whiteDescriptorSet;
	if(vkGlobals.currentDescriptorSet != set || vkGlobals.currentDescriptorSetKind != kind){
		vkCmdBindDescriptorSets(ctx->commandBuffers[ctx->currentImage],
			VK_PIPELINE_BIND_POINT_GRAPHICS, vkGlobals.pipelineLayouts[kind],
			0, 1, &set, 0, nil);
		vkGlobals.currentDescriptorSet = set;
		vkGlobals.currentDescriptorSetKind = kind;
	}
}

static bool32
isColor3DPipeline(PipelineKind kind)
{
	return kind == PIPE_COLOR3D || kind == PIPE_COLOR3D_NOZWRITE ||
	       kind == PIPE_COLOR3D_NOZTEST || kind == PIPE_COLOR3D_NOZTEST_NOZWRITE;
}

static void
bindRaySet(PipelineKind kind)
{
	Context *ctx = &vkGlobals.context;
	if(!vkGlobals.rayTracingUserEnabled || !vkGlobals.rayResourcesReady || !isColor3DPipeline(kind))
		return;
	VkDescriptorSet set = vkGlobals.rayDescriptorSets[ctx->currentFrame];
	if(set == VK_NULL_HANDLE)
		return;
	vkCmdBindDescriptorSets(ctx->commandBuffers[ctx->currentImage],
		VK_PIPELINE_BIND_POINT_GRAPHICS, vkGlobals.pipelineLayouts[kind],
		1, 1, &set, 0, nil);
}

static void
makeRtParams(float *dst, bool32 enable)
{
	dst[0] = enable && vkGlobals.rayTracingUserEnabled && vkGlobals.rayResourcesReady && vkGlobals.raySceneHasInstances ? 1.0f : 0.0f;
	dst[1] = 0.65f;
	dst[2] = 0.0f;
	dst[3] = 0.0f;
}

static void
makeRtLight(float *dst, const V3d *dir)
{
	if(dir){
		dst[0] = dir->x;
		dst[1] = dir->y;
		dst[2] = dir->z;
	}else{
		dst[0] = -0.45f;
		dst[1] = -0.25f;
		dst[2] = 0.86f;
	}
	dst[3] = 85.0f;
}

static void im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices);
static void im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices);

static void
im2DRenderLine(void *vertices, int32, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex tmp[2] = { verts[vert1], verts[vert2] };
	im2DRenderPrimitive(PRIMTYPELINELIST, tmp, 2);
}

static void
im2DRenderTriangle(void *vertices, int32, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex tmp[3] = { verts[vert1], verts[vert2], verts[vert3] };
	im2DRenderPrimitive(PRIMTYPETRILIST, tmp, 3);
}

static void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	PipelineKind k = selectIm2DPipelineKind();
	if(!validDrawState(k, primType) || numVertices <= 0)
		return;

	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)numVertices * sizeof(Im2DVertex);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset))
		return;

	Raster *textureRaster = (Raster*)vkGlobals.renderStates[TEXTURERASTER];
	bool32 vertexAlpha = 0;
	Im2DVertex *verts = (Im2DVertex*)vertices;
	for(int32 i = 0; i < numVertices; i++)
		if(verts[i].a != 0xFF)
			vertexAlpha = 1;
	bool32 alpha = vertexAlpha || rasterHasAlpha(textureRaster) || getRenderStateUInt(VERTEXALPHA, 0);

	VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
	if(vkGlobals.currentPipeline != pipeline){
		vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
			VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkGlobals.currentPipeline = pipeline;
	}
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);

	Im2DPushConstants pc;
	Camera *cam = (Camera*)engine->currentCamera;
	float w = cam && cam->frameBuffer ? (float)cam->frameBuffer->width : (float)ctx->swapchainExtent.width;
	float h = cam && cam->frameBuffer ? (float)cam->frameBuffer->height : (float)ctx->swapchainExtent.height;
	pc.xform[0] = 2.0f/w;
	pc.xform[1] = 2.0f/h;
	pc.xform[2] = -1.0f;
	pc.xform[3] = -1.0f;
	RGBA white = { 255, 255, 255, 255 };
	colorToFloat(pc.matColor, white);
	makeAlphaRef(pc.alphaRef, alpha);
	vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	bindTextureSet(k, getTextureDescriptor(textureRaster));
	vkCmdDraw(ctx->commandBuffers[ctx->currentImage], numVertices, 1, 0, 0);
}

static void
im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices)
{
	PipelineKind k = selectIm2DPipelineKind();
	if(!validDrawState(k, primType) || numVertices <= 0 || numIndices <= 0)
		return;

	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)numVertices * sizeof(Im2DVertex);
	VkDeviceSize indexSize = (VkDeviceSize)numIndices * sizeof(uint16);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VulkanBuffer *indexBuffer = &vkGlobals.dynamicIndexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset, indexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset) ||
	   !uploadDynamicBuffer(indexBuffer, &vkGlobals.dynamicIndexOffset, indices,
		indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 2, &indexOffset))
		return;

	Raster *textureRaster = (Raster*)vkGlobals.renderStates[TEXTURERASTER];
	bool32 vertexAlpha = 0;
	Im2DVertex *verts = (Im2DVertex*)vertices;
	for(int32 i = 0; i < numVertices; i++)
		if(verts[i].a != 0xFF)
			vertexAlpha = 1;
	bool32 alpha = vertexAlpha || rasterHasAlpha(textureRaster) || getRenderStateUInt(VERTEXALPHA, 0);

	VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
	if(vkGlobals.currentPipeline != pipeline){
		vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
			VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkGlobals.currentPipeline = pipeline;
	}
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(ctx->commandBuffers[ctx->currentImage], indexBuffer->buffer, indexOffset, VK_INDEX_TYPE_UINT16);

	Im2DPushConstants pc;
	Camera *cam = (Camera*)engine->currentCamera;
	float w = cam && cam->frameBuffer ? (float)cam->frameBuffer->width : (float)ctx->swapchainExtent.width;
	float h = cam && cam->frameBuffer ? (float)cam->frameBuffer->height : (float)ctx->swapchainExtent.height;
	pc.xform[0] = 2.0f/w;
	pc.xform[1] = 2.0f/h;
	pc.xform[2] = -1.0f;
	pc.xform[3] = -1.0f;
	RGBA white = { 255, 255, 255, 255 };
	colorToFloat(pc.matColor, white);
	makeAlphaRef(pc.alphaRef, alpha);
	vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	bindTextureSet(k, getTextureDescriptor(textureRaster));
	vkCmdDrawIndexed(ctx->commandBuffers[ctx->currentImage], numIndices, 1, 0, 0, 0);
}

static void
drawColor3DPrimitive(PrimitiveType primType, Im3DVertex *vertices, int32 numVertices,
	Matrix *world, const RGBA &matColor, VkDescriptorSet descriptorSet, bool32 alpha, bool32 rayTrace)
{
	PipelineKind k = selectColor3DPipelineKind();
	if(!validDrawState(k, primType) || numVertices <= 0)
		return;
	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)numVertices * sizeof(Im3DVertex);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset))
		return;

	VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
	if(vkGlobals.currentPipeline != pipeline){
		vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
			VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkGlobals.currentPipeline = pipeline;
	}
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);

	Color3DPushConstants pc;
	makeMVP(pc.mvp, world);
	colorToFloat(pc.matColor, matColor);
	makeAlphaRef(pc.alphaRef, alpha);
	makeRtParams(pc.rtParams, rayTrace);
	makeRtLight(pc.rtLight, nil);
	vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	bindTextureSet(k, descriptorSet);
	bindRaySet(k);
	vkCmdDraw(ctx->commandBuffers[ctx->currentImage], numVertices, 1, 0, 0);
}

static void
drawColor3DIndexed(PrimitiveType primType, Im3DVertex *vertices, int32 numVertices,
	void *indices, int32 numIndices, Matrix *world, const RGBA &matColor, VkDescriptorSet descriptorSet, bool32 alpha, bool32 rayTrace)
{
	PipelineKind k = selectColor3DPipelineKind();
	if(!validDrawState(k, primType) || numVertices <= 0 || numIndices <= 0)
		return;
	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)numVertices * sizeof(Im3DVertex);
	VkDeviceSize indexSize = (VkDeviceSize)numIndices * sizeof(uint16);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VulkanBuffer *indexBuffer = &vkGlobals.dynamicIndexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset, indexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset) ||
	   !uploadDynamicBuffer(indexBuffer, &vkGlobals.dynamicIndexOffset, indices,
		indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 2, &indexOffset))
		return;

	VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
	if(vkGlobals.currentPipeline != pipeline){
		vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
			VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkGlobals.currentPipeline = pipeline;
	}
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(ctx->commandBuffers[ctx->currentImage], indexBuffer->buffer, indexOffset, VK_INDEX_TYPE_UINT16);

	Color3DPushConstants pc;
	makeMVP(pc.mvp, world);
	colorToFloat(pc.matColor, matColor);
	makeAlphaRef(pc.alphaRef, alpha);
	makeRtParams(pc.rtParams, rayTrace);
	makeRtLight(pc.rtLight, nil);
	vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	bindTextureSet(k, descriptorSet);
	bindRaySet(k);
	vkCmdDrawIndexed(ctx->commandBuffers[ctx->currentImage], numIndices, 1, 0, 0, 0);
}

static void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	if(world == nil){
		vkGlobals.im3dWorld.setIdentity();
	}else
		vkGlobals.im3dWorld = *world;
	vkGlobals.im3dFlags = flags;
	vkGlobals.im3dVertices = (Im3DVertex*)vertices;
	vkGlobals.im3dNumVertices = numVertices;
}

static void
im3DRenderPrimitive(PrimitiveType primType)
{
	RGBA white = { 255, 255, 255, 255 };
	VkDescriptorSet set = (vkGlobals.im3dFlags & im3d::VERTEXUV) ?
		getTextureDescriptor((Raster*)vkGlobals.renderStates[TEXTURERASTER]) :
		vkGlobals.whiteDescriptorSet;
	Raster *textureRaster = (vkGlobals.im3dFlags & im3d::VERTEXUV) ?
		(Raster*)vkGlobals.renderStates[TEXTURERASTER] : nil;
	bool32 alpha = rasterHasAlpha(textureRaster) || getRenderStateUInt(VERTEXALPHA, 0) ||
		(vkGlobals.im3dFlags & im3d::ALLOPAQUE) == 0;
	drawColor3DPrimitive(primType, vkGlobals.im3dVertices, vkGlobals.im3dNumVertices,
		&vkGlobals.im3dWorld, white, set, alpha, 0);
}

static void
im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices)
{
	RGBA white = { 255, 255, 255, 255 };
	VkDescriptorSet set = (vkGlobals.im3dFlags & im3d::VERTEXUV) ?
		getTextureDescriptor((Raster*)vkGlobals.renderStates[TEXTURERASTER]) :
		vkGlobals.whiteDescriptorSet;
	Raster *textureRaster = (vkGlobals.im3dFlags & im3d::VERTEXUV) ?
		(Raster*)vkGlobals.renderStates[TEXTURERASTER] : nil;
	bool32 alpha = rasterHasAlpha(textureRaster) || getRenderStateUInt(VERTEXALPHA, 0) ||
		(vkGlobals.im3dFlags & im3d::ALLOPAQUE) == 0;
	drawColor3DIndexed(primType, vkGlobals.im3dVertices, vkGlobals.im3dNumVertices,
		indices, numIndices, &vkGlobals.im3dWorld, white, set, alpha, 0);
}

static void im3DEnd(void) {}

static V3d
transformVector(Matrix *m, const V3d &v)
{
	V3d out;
	out.x = m->right.x*v.x + m->up.x*v.y + m->at.x*v.z;
	out.y = m->right.y*v.x + m->up.y*v.y + m->at.y*v.z;
	out.z = m->right.z*v.x + m->up.z*v.y + m->at.z*v.z;
	return out;
}

static void
normalizeVector(V3d *v)
{
	float len = sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
	if(len > 0.0001f){
		v->x /= len;
		v->y /= len;
		v->z /= len;
	}
}

static float
dotVector(const V3d &a, const V3d &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

struct SimpleLightSet
{
	bool32 enabled;
	bool32 hasNormals;
	RGBAf ambient;
	Light *directionals[8];
	V3d directionalsNormalized[8];
	RGBAf directionalsColor[8];
	int32 numDirectionals;
};

static void
collectSimpleLights(Atomic *atomic, SimpleLightSet *lights)
{
	memset(lights, 0, sizeof(*lights));
	Geometry *geo = atomic->geometry;
	if(engine->currentWorld == nil)
		return;

	lights->hasNormals = (geo->flags & Geometry::NORMALS) != 0;
	if((geo->flags & Geometry::LIGHT) == 0){
		lights->enabled = 0;
		return;
	}

	WorldLights lightData;
	Light *locals[8];
	lightData.directionals = lights->directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;
	((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
	lights->ambient = lightData.ambient;
	lights->numDirectionals = lightData.numDirectionals;
	lights->enabled = 1;

	// Pre-calculate normalized directions and colors of all directionals
	for(int32 i = 0; i < lights->numDirectionals; i++){
		Light *l = lights->directionals[i];
		V3d dir = l->getFrame()->getLTM()->at;
		dir.x = -dir.x;
		dir.y = -dir.y;
		dir.z = -dir.z;
		normalizeVector(&dir);
		lights->directionalsNormalized[i] = dir;
		lights->directionalsColor[i] = l->color;
	}
}

static RGBA
applySimpleLighting(SimpleLightSet *lights, Matrix *world, const V3d &normal, const RGBA &base)
{
	if(!lights->enabled)
		return base;

	if(!lights->hasNormals){
		float r = base.red / 255.0f + lights->ambient.red;
		float g = base.green / 255.0f + lights->ambient.green;
		float b = base.blue / 255.0f + lights->ambient.blue;
		if(r > 1.0f) r = 1.0f;
		if(g > 1.0f) g = 1.0f;
		if(b > 1.0f) b = 1.0f;
		RGBA out = base;
		out.red = (uint8)(r*255.0f);
		out.green = (uint8)(g*255.0f);
		out.blue = (uint8)(b*255.0f);
		return out;
	}

	V3d n = transformVector(world, normal);
	normalizeVector(&n);

	float r = base.red / 255.0f + lights->ambient.red;
	float g = base.green / 255.0f + lights->ambient.green;
	float b = base.blue / 255.0f + lights->ambient.blue;

	for(int32 i = 0; i < lights->numDirectionals; i++){
		float f = dotVector(n, lights->directionalsNormalized[i]);
		if(f > 0.0f){
			r += f*lights->directionalsColor[i].red;
			g += f*lights->directionalsColor[i].green;
			b += f*lights->directionalsColor[i].blue;
		}
	}

	if(r > 1.0f) r = 1.0f;
	if(g > 1.0f) g = 1.0f;
	if(b > 1.0f) b = 1.0f;
	RGBA out = base;
	out.red = (uint8)(r*255.0f);
	out.green = (uint8)(g*255.0f);
	out.blue = (uint8)(b*255.0f);
	return out;
}


static bool32
ensureTempGeometry(uint32 numVertices, uint32 numIndices)
{
	if(vkGlobals.tempVertexCapacity < numVertices){
		rwFree(vkGlobals.tempVertices);
		vkGlobals.tempVertices = rwNewT(Im3DVertex, numVertices, MEMDUR_EVENT | ID_DRIVER);
		vkGlobals.tempVertexCapacity = numVertices;
	}
	if(vkGlobals.tempIndexCapacity < numIndices){
		rwFree(vkGlobals.tempIndices);
		vkGlobals.tempIndices = rwNewT(uint16, numIndices, MEMDUR_EVENT | ID_DRIVER);
		vkGlobals.tempIndexCapacity = numIndices;
	}
	return vkGlobals.tempVertices != nil && vkGlobals.tempIndices != nil;
}

void
defaultRenderCB(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo == nil || geo->numVertices <= 0 || geo->meshHeader == nil)
		return;
	MeshHeader *meshh = geo->meshHeader;
	if(!ensureTempGeometry(geo->numVertices, meshh->totalIndices))
		return;

	bool32 hasNormals = !!(geo->flags & Geometry::NORMALS);
	bool32 hasPrelit = !!(geo->flags & Geometry::PRELIT);
	bool32 hasTex = geo->numTexCoordSets > 0 && geo->texCoords[0] != nil;
	MorphTarget *morph = &geo->morphTargets[0];
	RGBA white = { 255, 255, 255, 255 };
	RGBA black = { 0, 0, 0, 255 };
	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	SimpleLightSet lights;
	collectSimpleLights(atomic, &lights);
	registerRayInstance(geo, world);
	for(int32 i = 0; i < geo->numVertices; i++){
		Im3DVertex &v = vkGlobals.tempVertices[i];
		V3d::transformPoints(&v.position, &morph->vertices[i], 1, world);
		V3d localNormal;
		if(hasNormals)
			localNormal = morph->normals[i];
		else{
			localNormal.x = 0.0f;
			localNormal.y = 0.0f;
			localNormal.z = 1.0f;
		}
		V3d::transformVectors(&v.normal, &localNormal, 1, world);
		RGBA c = hasPrelit ? geo->colors[i] :
			((geo->flags & Geometry::LIGHT) ? black : white);
		c = applySimpleLighting(&lights, world, localNormal, c);
		v.r = c.red;
		v.g = c.green;
		v.b = c.blue;
		v.a = c.alpha;
		if(hasTex){
			v.u = geo->texCoords[0][i].u;
			v.v = geo->texCoords[0][i].v;
		}else{
			v.u = 0.0f;
			v.v = 0.0f;
		}
	}

	PrimitiveType primType = meshh->flags == MeshHeader::TRISTRIP ? PRIMTYPETRISTRIP : PRIMTYPETRILIST;
	PipelineKind k = selectColor3DPipelineKind();
	if(!validDrawState(k, primType))
		return;

	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)geo->numVertices * sizeof(Im3DVertex);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vkGlobals.tempVertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset))
		return;

	bool32 vertexAlpha = geometryHasVertexAlpha(geo);
	Mesh *mesh = meshh->getMeshes();
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);

	for(uint32 i = 0; i < meshh->numMeshes; i++){
		if(mesh[i].numIndices <= 0)
			continue;
		Material *mat = mesh[i].material;
		RGBA matColor = white;
		VkDescriptorSet tex = vkGlobals.whiteDescriptorSet;
		Raster *texRaster = nil;
		if(mat){
			if(geo->flags & Geometry::MODULATE)
				matColor = mat->color;
			if(mat->texture && mat->texture->raster){
				texRaster = mat->texture->raster;
				tex = getTextureDescriptor(texRaster);
			}
		}

		VulkanBuffer *indexBuffer = &vkGlobals.dynamicIndexBuffers[ctx->currentFrame];
		VkDeviceSize indexSize = (VkDeviceSize)mesh[i].numIndices * sizeof(uint16);
		VkDeviceSize indexOffset;
		if(!uploadDynamicBuffer(indexBuffer, &vkGlobals.dynamicIndexOffset, mesh[i].indices,
			indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 2, &indexOffset))
			continue;

		bool32 alpha = vertexAlpha || matColor.alpha != 0xFF ||
			rasterHasAlpha(texRaster) || getRenderStateUInt(VERTEXALPHA, 0);
		VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
		if(vkGlobals.currentPipeline != pipeline){
			vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
				VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkGlobals.currentPipeline = pipeline;
		}
		vkCmdBindIndexBuffer(ctx->commandBuffers[ctx->currentImage], indexBuffer->buffer, indexOffset, VK_INDEX_TYPE_UINT16);

		Color3DPushConstants pc;
		makeMVP(pc.mvp, &ident);
		colorToFloat(pc.matColor, matColor);
		makeAlphaRef(pc.alphaRef, alpha);
		makeRtParams(pc.rtParams, !alpha);
		makeRtLight(pc.rtLight, lights.numDirectionals > 0 ? &lights.directionalsNormalized[0] : nil);
		vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		bindTextureSet(k, tex);
		bindRaySet(k);
		vkCmdDrawIndexed(ctx->commandBuffers[ctx->currentImage], mesh[i].numIndices, 1, 0, 0, 0);
	}
}

static void vulkanPipeInstance(rw::ObjPipeline*, Atomic*) {}
static void vulkanPipeUninstance(rw::ObjPipeline*, Atomic*) {}
static void
vulkanPipeRender(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	if(pipe->renderCB)
		pipe->renderCB(atomic);
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_VULKAN);
	this->impl.instance = vulkanPipeInstance;
	this->impl.uninstance = vulkanPipeUninstance;
	this->impl.render = vulkanPipeRender;
	this->renderCB = nil;
}

ObjPipeline*
ObjPipeline::create(void)
{
	ObjPipeline *pipe = rwNewT(ObjPipeline, 1, MEMDUR_GLOBAL);
	pipe->init();
	return pipe;
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

static void
skinRenderCB(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo == nil || geo->numVertices <= 0 || geo->meshHeader == nil)
		return;
	MeshHeader *meshh = geo->meshHeader;
	if(!ensureTempGeometry(geo->numVertices, meshh->totalIndices))
		return;

	Skin *skin = Skin::get(geo);
	HAnimHierarchy *hier = skin ? Skin::getHierarchy(atomic) : nil;

	// Compute skin matrices
	static Matrix skinMats[256];
	int32 numBones = skin ? skin->numBones : 0;
	if(numBones > 256) numBones = 256;

	if(skin && hier){
		Matrix *invMats = (Matrix*)skin->inverseMatrices;
		if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
			for(int32 i = 0; i < numBones; i++){
				invMats[i].flags = 0;
				Matrix::mult(&skinMats[i], &invMats[i], &hier->matrices[i]);
			}
		}else{
			Matrix invAtmMat;
			Matrix::invert(&invAtmMat, atomic->getFrame()->getLTM());
			Matrix tmp;
			for(int32 i = 0; i < numBones; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
				Matrix::mult(&skinMats[i], &invMats[i], &tmp);
			}
		}
	}else{
		for(int32 i = 0; i < numBones; i++)
			skinMats[i].setIdentity();
	}

	bool32 hasNormals = !!(geo->flags & Geometry::NORMALS);
	bool32 hasPrelit = !!(geo->flags & Geometry::PRELIT);
	bool32 hasTex = geo->numTexCoordSets > 0 && geo->texCoords[0] != nil;
	MorphTarget *morph = &geo->morphTargets[0];
	RGBA white = { 255, 255, 255, 255 };
	RGBA black = { 0, 0, 0, 255 };
	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	SimpleLightSet lights;
	collectSimpleLights(atomic, &lights);

	for(int32 i = 0; i < geo->numVertices; i++){
		Im3DVertex &v = vkGlobals.tempVertices[i];
		V3d srcPos = morph->vertices[i];
		V3d srcNrm;
		if(hasNormals)
			srcNrm = morph->normals[i];
		else{
			srcNrm.x = 0.0f;
			srcNrm.y = 0.0f;
			srcNrm.z = 1.0f;
		}

		// CPU skinning: blend by bone weights
		if(skin && skin->weights && skin->indices){
			V3d pos = {0,0,0};
			V3d nrm = {0,0,0};
			uint8 *idx = &skin->indices[i*4];
			float *wgt = &skin->weights[i*4];
			for(int32 j = 0; j < 4; j++){
				float w = wgt[j];
				if(w == 0.0f) continue;
				Matrix *m = &skinMats[idx[j]];
				// Transform point: pos += w * (m * srcPos)
				V3d tp;
				tp.x = m->right.x*srcPos.x + m->up.x*srcPos.y + m->at.x*srcPos.z + m->pos.x;
				tp.y = m->right.y*srcPos.x + m->up.y*srcPos.y + m->at.y*srcPos.z + m->pos.y;
				tp.z = m->right.z*srcPos.x + m->up.z*srcPos.y + m->at.z*srcPos.z + m->pos.z;
				pos.x += w*tp.x;
				pos.y += w*tp.y;
				pos.z += w*tp.z;
				// Transform vector: nrm += w * (m * srcNrm)
				V3d tn;
				tn.x = m->right.x*srcNrm.x + m->up.x*srcNrm.y + m->at.x*srcNrm.z;
				tn.y = m->right.y*srcNrm.x + m->up.y*srcNrm.y + m->at.y*srcNrm.z;
				tn.z = m->right.z*srcNrm.x + m->up.z*srcNrm.y + m->at.z*srcNrm.z;
				nrm.x += w*tn.x;
				nrm.y += w*tn.y;
				nrm.z += w*tn.z;
			}
			v.position = pos;
			// Normalize
			float len = sqrtf(nrm.x*nrm.x + nrm.y*nrm.y + nrm.z*nrm.z);
			if(len > 0.0001f){
				nrm.x /= len;
				nrm.y /= len;
				nrm.z /= len;
			}
			v.normal = nrm;
		}else{
			v.position = srcPos;
			v.normal = srcNrm;
		}

		RGBA c = hasPrelit ? geo->colors[i] :
			((geo->flags & Geometry::LIGHT) ? black : white);
		c = applySimpleLighting(&lights, world, v.normal, c);
		v.r = c.red;
		v.g = c.green;
		v.b = c.blue;
		v.a = c.alpha;
		if(hasTex){
			v.u = geo->texCoords[0][i].u;
			v.v = geo->texCoords[0][i].v;
		}else{
			v.u = 0.0f;
			v.v = 0.0f;
		}
	}

	PrimitiveType primType = meshh->flags == MeshHeader::TRISTRIP ? PRIMTYPETRISTRIP : PRIMTYPETRILIST;
	PipelineKind k = selectColor3DPipelineKind();
	if(!validDrawState(k, primType))
		return;

	Context *ctx = &vkGlobals.context;
	VkDeviceSize vertexSize = (VkDeviceSize)geo->numVertices * sizeof(Im3DVertex);
	VulkanBuffer *vertexBuffer = &vkGlobals.dynamicVertexBuffers[ctx->currentFrame];
	VkDeviceSize vertexOffset;
	if(!uploadDynamicBuffer(vertexBuffer, &vkGlobals.dynamicVertexOffset, vkGlobals.tempVertices,
		vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 16, &vertexOffset))
		return;

	bool32 vertexAlpha = geometryHasVertexAlpha(geo);
	Mesh *mesh = meshh->getMeshes();
	VkBuffer vertexBuffers[] = { vertexBuffer->buffer };
	VkDeviceSize offsets[] = { vertexOffset };
	vkCmdBindVertexBuffers(ctx->commandBuffers[ctx->currentImage], 0, 1, vertexBuffers, offsets);

	for(uint32 i = 0; i < meshh->numMeshes; i++){
		if(mesh[i].numIndices <= 0)
			continue;
		Material *mat = mesh[i].material;
		RGBA matColor = white;
		VkDescriptorSet tex = vkGlobals.whiteDescriptorSet;
		Raster *texRaster = nil;
		if(mat){
			if(geo->flags & Geometry::MODULATE)
				matColor = mat->color;
			if(mat->texture && mat->texture->raster){
				texRaster = mat->texture->raster;
				tex = getTextureDescriptor(texRaster);
			}
		}

		VulkanBuffer *indexBuffer = &vkGlobals.dynamicIndexBuffers[ctx->currentFrame];
		VkDeviceSize indexSize = (VkDeviceSize)mesh[i].numIndices * sizeof(uint16);
		VkDeviceSize indexOffset;
		if(!uploadDynamicBuffer(indexBuffer, &vkGlobals.dynamicIndexOffset, mesh[i].indices,
			indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 2, &indexOffset))
			continue;

		bool32 alpha = vertexAlpha || matColor.alpha != 0xFF ||
			rasterHasAlpha(texRaster) || getRenderStateUInt(VERTEXALPHA, 0);
		VkPipeline pipeline = getDrawPipeline(k, primType, alpha);
		if(vkGlobals.currentPipeline != pipeline){
			vkCmdBindPipeline(ctx->commandBuffers[ctx->currentImage],
				VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkGlobals.currentPipeline = pipeline;
		}
		vkCmdBindIndexBuffer(ctx->commandBuffers[ctx->currentImage], indexBuffer->buffer, indexOffset, VK_INDEX_TYPE_UINT16);

		Color3DPushConstants pc;
		makeMVP(pc.mvp, world);
		colorToFloat(pc.matColor, matColor);
		makeAlphaRef(pc.alphaRef, alpha);
		makeRtParams(pc.rtParams, 0);
		makeRtLight(pc.rtLight, nil);
		vkCmdPushConstants(ctx->commandBuffers[ctx->currentImage], vkGlobals.pipelineLayouts[k],
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		bindTextureSet(k, tex);
		bindRaySet(k);
		vkCmdDrawIndexed(ctx->commandBuffers[ctx->currentImage], mesh[i].numIndices, 1, 0, 0, 0);
	}
}

static ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

static void*
vkSkinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_VULKAN] = makeSkinPipeline();
	return o;
}

static void*
vkSkinClose(void *o, int32, int32)
{
	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_VULKAN])->destroy();
	skinGlobals.pipelines[PLATFORM_VULKAN] = nil;
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_VULKAN, 0, ID_SKIN,
	                       vkSkinOpen, vkSkinClose);
}

static ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = makeDefaultPipeline();
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 1;
	return pipe;
}

static void*
vkMatFXOpen(void *o, int32, int32)
{
	matFXGlobals.pipelines[PLATFORM_VULKAN] = makeMatFXPipeline();
	return o;
}

static void*
vkMatFXClose(void *o, int32, int32)
{
	if(matFXGlobals.pipelines[PLATFORM_VULKAN]){
		((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_VULKAN])->destroy();
		matFXGlobals.pipelines[PLATFORM_VULKAN] = nil;
	}
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_VULKAN, 0, ID_MATFX,
	                       vkMatFXOpen, vkMatFXClose);
}

static int
deviceSystemSDL3(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;
	switch(req){
	case DEVICEOPEN:
		return openSDL3((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeSDL3();
	case DEVICEINIT:
		return startSDL3();
	case DEVICETERM:
		return stopSDL3();
	case DEVICEFINALIZE:
		return 1;

	case DEVICEGETNUMSUBSYSTEMS:
		return vkGlobals.numDisplays;
	case DEVICEGETCURRENTSUBSYSTEM:
		return vkGlobals.currentDisplay;
	case DEVICESETSUBSYSTEM:
		if(n < 0 || n >= vkGlobals.numDisplays)
			return 0;
		vkGlobals.currentDisplay = n;
		makeVideoModeList(vkGlobals.currentDisplay);
		return 1;
	case DEVICEGETSUBSSYSTEMINFO: {
		if(n < 0 || n >= vkGlobals.numDisplays)
			return 0;
		const char *displayName = SDL_GetDisplayName(vkGlobals.displays[n]);
		if(displayName == nil)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, displayName, sizeof(SubSystemInfo::name));
		((SubSystemInfo*)arg)->name[sizeof(SubSystemInfo::name)-1] = '\0';
		return 1;
	}

	case DEVICEGETNUMVIDEOMODES:
		return vkGlobals.numModes;
	case DEVICEGETCURRENTVIDEOMODE:
		return vkGlobals.currentMode;
	case DEVICESETVIDEOMODE:
		if(n < 0 || n >= vkGlobals.numModes)
			return 0;
		vkGlobals.currentMode = n;
		return 1;
	case DEVICEGETVIDEOMODEINFO:
		if(n < 0 || n >= vkGlobals.numModes)
			return 0;
		rwmode = (VideoMode*)arg;
		rwmode->width = vkGlobals.modes[n].mode.w;
		rwmode->height = vkGlobals.modes[n].mode.h;
		rwmode->depth = vkGlobals.modes[n].depth;
		rwmode->flags = vkGlobals.modes[n].flags;
		return 1;

	case DEVICEGETMAXMULTISAMPLINGLEVELS:
	case DEVICEGETMULTISAMPLINGLEVELS:
		return 1;
	case DEVICESETMULTISAMPLINGLEVELS:
		return n == 1;
	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

Device renderdevice = {
	0.0f, 1.0f,
	vulkan::beginUpdate,
	vulkan::endUpdate,
	vulkan::clearCamera,
	vulkan::showRaster,
	vulkan::rasterRenderFast,
	vulkan::setRenderState,
	vulkan::getRenderState,
	vulkan::im2DRenderLine,
	vulkan::im2DRenderTriangle,
	vulkan::im2DRenderPrimitive,
	vulkan::im2DRenderIndexedPrimitive,
	vulkan::im3DTransform,
	vulkan::im3DRenderPrimitive,
	vulkan::im3DRenderIndexedPrimitive,
	vulkan::im3DEnd,
	vulkan::deviceSystemSDL3
};

}
}

#endif
