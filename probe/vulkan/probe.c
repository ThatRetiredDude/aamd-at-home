/*
 * AAMD@home RADV saxpy probe.
 *
 * Copied from the working PS5 homebrew GPU path: Porpoise (PPSA99764) and
 * RetroArch (PPSA99169) statically link Mesa 26.2 RADV; PS5_Vulkan's
 * radv/radv_smoke.c is the smallest program that already ran a compute
 * dispatch on the console. This file is that smoke test cut down to saxpy:
 * vk_icdGetInstanceProcAddr, host-visible storage buffers, vkCmdDispatch,
 * CPU check, timing. No Vulkan loader. GPL-3.0 applies to a binary linked
 * with PS5_Vulkan / RADV (see NOTICE.md); this source is MIT as part of
 * aamd-at-home.
 */

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "aamd_probe.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "saxpy_spv.h"

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance,
                                                                   const char *name);

#define INSTANCE_COMMANDS(X)                                                                       \
    X(DestroyInstance)                                                                             \
    X(EnumeratePhysicalDevices)                                                                    \
    X(GetPhysicalDeviceProperties2)                                                                \
    X(GetPhysicalDeviceQueueFamilyProperties)                                                      \
    X(GetPhysicalDeviceMemoryProperties)                                                           \
    X(CreateDevice)                                                                                \
    X(GetDeviceProcAddr)

#define DEVICE_COMMANDS(X)                                                                         \
    X(DestroyDevice)                                                                               \
    X(GetDeviceQueue)                                                                              \
    X(QueueSubmit)                                                                                 \
    X(QueueWaitIdle)                                                                               \
    X(DeviceWaitIdle)                                                                              \
    X(CreateFence)                                                                                 \
    X(DestroyFence)                                                                                \
    X(WaitForFences)                                                                               \
    X(ResetFences)                                                                                 \
    X(CreateCommandPool)                                                                           \
    X(DestroyCommandPool)                                                                          \
    X(AllocateCommandBuffers)                                                                      \
    X(BeginCommandBuffer)                                                                          \
    X(EndCommandBuffer)                                                                            \
    X(ResetCommandBuffer)                                                                          \
    X(CreateBuffer)                                                                                \
    X(DestroyBuffer)                                                                               \
    X(GetBufferMemoryRequirements)                                                                 \
    X(AllocateMemory)                                                                              \
    X(FreeMemory)                                                                                  \
    X(BindBufferMemory)                                                                            \
    X(MapMemory)                                                                                   \
    X(CmdPipelineBarrier2)                                                                         \
    X(CmdBindPipeline)                                                                             \
    X(CmdBindDescriptorSets)                                                                       \
    X(CmdDispatch)                                                                                 \
    X(CmdPushConstants)                                                                            \
    X(CreateShaderModule)                                                                          \
    X(DestroyShaderModule)                                                                         \
    X(CreateDescriptorSetLayout)                                                                   \
    X(DestroyDescriptorSetLayout)                                                                  \
    X(CreatePipelineLayout)                                                                        \
    X(DestroyPipelineLayout)                                                                       \
    X(CreateComputePipelines)                                                                      \
    X(DestroyPipeline)                                                                             \
    X(CreateDescriptorPool)                                                                        \
    X(DestroyDescriptorPool)                                                                       \
    X(AllocateDescriptorSets)                                                                      \
    X(UpdateDescriptorSets)

#define DECLARE(name) static PFN_vk##name vk##name;
INSTANCE_COMMANDS(DECLARE)
DEVICE_COMMANDS(DECLARE)
static PFN_vkCreateInstance vkCreateInstance;

struct context {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    VkPhysicalDeviceMemoryProperties memory;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkFence fence;
};

struct buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *map;
    VkDeviceSize size;
};

static bool load_instance(VkInstance instance) {
    bool ok = true;
#define LOAD(name)                                                                                 \
    vk##name = (PFN_vk##name)vk_icdGetInstanceProcAddr(instance, "vk" #name);                      \
    if (!vk##name) {                                                                               \
        aamd_log("missing vk" #name);                                                              \
        ok = false;                                                                                \
    }
    INSTANCE_COMMANDS(LOAD)
#undef LOAD
    return ok;
}

static bool load_device(VkDevice device) {
    bool ok = true;
#define LOAD(name)                                                                                 \
    vk##name = (PFN_vk##name)vkGetDeviceProcAddr(device, "vk" #name);                              \
    if (!vk##name) {                                                                               \
        aamd_log("missing vk" #name);                                                              \
        ok = false;                                                                                \
    }
    DEVICE_COMMANDS(LOAD)
#undef LOAD
    return ok;
}

static uint32_t memory_type(const struct context *c, uint32_t bits, VkMemoryPropertyFlags wanted) {
    for (uint32_t i = 0; i < c->memory.memoryTypeCount; i++) {
        if ((bits & (1u << i)) && (c->memory.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    }
    return UINT32_MAX;
}

static int buffer_create(struct context *c, VkDeviceSize size, VkBufferUsageFlags usage,
                         struct buffer *out) {
    *out = (struct buffer){.size = size};
    const VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (vkCreateBuffer(c->device, &info, NULL, &out->buffer) != VK_SUCCESS)
        return 0;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(c->device, out->buffer, &req);
    const uint32_t type =
        memory_type(c, req.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    const VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = type,
    };
    if (type == UINT32_MAX || vkAllocateMemory(c->device, &alloc, NULL, &out->memory) != VK_SUCCESS ||
        vkBindBufferMemory(c->device, out->buffer, out->memory, 0) != VK_SUCCESS ||
        vkMapMemory(c->device, out->memory, 0, VK_WHOLE_SIZE, 0, &out->map) != VK_SUCCESS)
        return 0;
    return 1;
}

static void buffer_destroy(struct context *c, struct buffer *b) {
    vkDestroyBuffer(c->device, b->buffer, NULL);
    vkFreeMemory(c->device, b->memory, NULL);
}

static int begin(struct context *c) {
    const VkCommandBufferBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    return vkResetCommandBuffer(c->cmd, 0) == VK_SUCCESS &&
           vkBeginCommandBuffer(c->cmd, &info) == VK_SUCCESS;
}

static int submit_and_wait(struct context *c, const char *what) {
    const VkMemoryBarrier2 host = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
    };
    const VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &host,
    };
    vkCmdPipelineBarrier2(c->cmd, &dependency);
    VkResult result = vkEndCommandBuffer(c->cmd);
    if (result != VK_SUCCESS) {
        aamd_log("%s: vkEndCommandBuffer %d", what, (int)result);
        return 0;
    }
    const VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &c->cmd,
    };
    vkResetFences(c->device, 1, &c->fence);
    result = vkQueueSubmit(c->queue, 1, &submit, c->fence);
    if (result != VK_SUCCESS) {
        aamd_log("%s: vkQueueSubmit %d", what, (int)result);
        return 0;
    }
    result = vkWaitForFences(c->device, 1, &c->fence, VK_TRUE, UINT64_C(20000000000));
    if (result != VK_SUCCESS) {
        aamd_log("%s: vkWaitForFences %d", what, (int)result);
        return 0;
    }
    return 1;
}

static int context_create(struct context *c) {
    vkCreateInstance = (PFN_vkCreateInstance)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    if (!vkCreateInstance) {
        aamd_log("vk_icdGetInstanceProcAddr has no vkCreateInstance");
        return 0;
    }
    const VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "aamd-probe",
        .apiVersion = VK_API_VERSION_1_3,
    };
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkResult result = vkCreateInstance(&instance_info, NULL, &c->instance);
    if (result != VK_SUCCESS || !load_instance(c->instance)) {
        aamd_log("vkCreateInstance failed (%d)", (int)result);
        return 0;
    }

    uint32_t count = 1;
    result = vkEnumeratePhysicalDevices(c->instance, &count, &c->physical);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0) {
        aamd_log("no physical device");
        return 0;
    }

    VkPhysicalDeviceDriverProperties driver = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 props = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
    vkGetPhysicalDeviceProperties2(c->physical, &props);
    aamd_log("device: %s  Vulkan %u.%u.%u  %s %s", props.properties.deviceName,
             VK_API_VERSION_MAJOR(props.properties.apiVersion),
             VK_API_VERSION_MINOR(props.properties.apiVersion),
             VK_API_VERSION_PATCH(props.properties.apiVersion), driver.driverName,
             driver.driverInfo);

    VkQueueFamilyProperties families[8];
    count = 8;
    vkGetPhysicalDeviceQueueFamilyProperties(c->physical, &count, families);
    c->family = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            c->family = i;
            break;
        }
    }
    if (c->family == UINT32_MAX) {
        aamd_log("no compute queue family");
        return 0;
    }
    vkGetPhysicalDeviceMemoryProperties(c->physical, &c->memory);

    const float priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = c->family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
    };
    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    result = vkCreateDevice(c->physical, &device_info, NULL, &c->device);
    if (result != VK_SUCCESS || !load_device(c->device)) {
        aamd_log("vkCreateDevice failed (%d)", (int)result);
        return 0;
    }
    vkGetDeviceQueue(c->device, c->family, 0, &c->queue);

    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = c->family,
    };
    const VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateCommandPool(c->device, &pool_info, NULL, &c->pool) != VK_SUCCESS ||
        vkCreateFence(c->device, &fence_info, NULL, &c->fence) != VK_SUCCESS)
        return 0;
    const VkCommandBufferAllocateInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = c->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    return vkAllocateCommandBuffers(c->device, &cmd_info, &c->cmd) == VK_SUCCESS;
}

static int gpu_saxpy(struct context *c, uint32_t n, float a, const float *x_in, float *y_inout,
                     double *seconds) {
    struct buffer bx = {0}, by = {0};
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    int ok = 1;

    const VkDeviceSize bytes = (VkDeviceSize)n * sizeof(float);
    if (!buffer_create(c, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &bx) ||
        !buffer_create(c, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &by)) {
        aamd_log("gpu: buffer_create failed");
        ok = 0;
        goto cleanup;
    }
    memcpy(bx.map, x_in, (size_t)bytes);
    memcpy(by.map, y_inout, (size_t)bytes);

    const VkShaderModuleCreateInfo sh = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(saxpy_comp_spv),
        .pCode = saxpy_comp_spv,
    };
    if (vkCreateShaderModule(c->device, &sh, NULL, &module) != VK_SUCCESS) {
        aamd_log("gpu: vkCreateShaderModule failed");
        ok = 0;
        goto cleanup;
    }

    const VkDescriptorSetLayoutBinding bindings[2] = {
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
    };
    const VkDescriptorSetLayoutCreateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    const VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 8,
    };
    if (vkCreateDescriptorSetLayout(c->device, &set_info, NULL, &set_layout) != VK_SUCCESS) {
        ok = 0;
        goto cleanup;
    }
    const VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };
    if (vkCreatePipelineLayout(c->device, &layout_info, NULL, &layout) != VK_SUCCESS) {
        ok = 0;
        goto cleanup;
    }
    const VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main",
            },
        .layout = layout,
    };
    double compile0 = aamd_now_s();
    if (vkCreateComputePipelines(c->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline) !=
        VK_SUCCESS) {
        aamd_log("gpu: vkCreateComputePipelines failed");
        ok = 0;
        goto cleanup;
    }
    aamd_log("gpu: pipeline compile %.4f s", aamd_now_s() - compile0);

    const VkDescriptorPoolSize pool_size = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            .descriptorCount = 2};
    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    if (vkCreateDescriptorPool(c->device, &pool_info, NULL, &pool) != VK_SUCCESS) {
        ok = 0;
        goto cleanup;
    }
    const VkDescriptorSetAllocateInfo set_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    if (vkAllocateDescriptorSets(c->device, &set_alloc, &set) != VK_SUCCESS) {
        ok = 0;
        goto cleanup;
    }
    const VkDescriptorBufferInfo bufinfo[2] = {
        {.buffer = bx.buffer, .offset = 0, .range = VK_WHOLE_SIZE},
        {.buffer = by.buffer, .offset = 0, .range = VK_WHOLE_SIZE},
    };
    const VkWriteDescriptorSet writes[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = set,
         .dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pBufferInfo = &bufinfo[0]},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = set,
         .dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pBufferInfo = &bufinfo[1]},
    };
    vkUpdateDescriptorSets(c->device, 2, writes, 0, NULL);

    struct {
        uint32_t n;
        float a;
    } push = {n, a};
    uint32_t groups = (n + AAMD_SAXPY_WG - 1u) / AAMD_SAXPY_WG;

    /* Warmup so compile/cache is not in the timed dispatch. */
    if (!begin(c)) {
        ok = 0;
        goto cleanup;
    }
    vkCmdBindPipeline(c->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(c->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    vkCmdPushConstants(c->cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof push, &push);
    vkCmdDispatch(c->cmd, groups, 1, 1);
    if (!submit_and_wait(c, "warmup")) {
        ok = 0;
        goto cleanup;
    }
    memcpy(by.map, y_inout, (size_t)bytes);

    double t0 = aamd_now_s();
    if (!begin(c)) {
        ok = 0;
        goto cleanup;
    }
    vkCmdBindPipeline(c->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(c->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    vkCmdPushConstants(c->cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof push, &push);
    vkCmdDispatch(c->cmd, groups, 1, 1);
    if (!submit_and_wait(c, "saxpy")) {
        ok = 0;
        goto cleanup;
    }
    *seconds = aamd_now_s() - t0;
    memcpy(y_inout, by.map, (size_t)bytes);

cleanup:
    vkDestroyPipeline(c->device, pipeline, NULL);
    vkDestroyDescriptorPool(c->device, pool, NULL);
    vkDestroyPipelineLayout(c->device, layout, NULL);
    vkDestroyDescriptorSetLayout(c->device, set_layout, NULL);
    vkDestroyShaderModule(c->device, module, NULL);
    buffer_destroy(c, &bx);
    buffer_destroy(c, &by);
    return ok;
}

int main(void) {
    const uint32_t n = AAMD_SAXPY_N;
    const unsigned threads = aamd_nthreads();
    int status = 1;

    aamd_log_open();
    aamd_log("AAMD@home GPU compute probe (Vulkan / RADV)");
    aamd_log("title: PPSA99021  n=%u a=%.1f cpu_threads=%u", n, AAMD_SAXPY_A, threads);
    aamd_notify("AAMD probe: RADV saxpy");

    float *x = (float *)malloc((size_t)n * sizeof(float));
    float *y_cpu = (float *)malloc((size_t)n * sizeof(float));
    float *y_gpu = (float *)malloc((size_t)n * sizeof(float));
    if (!x || !y_cpu || !y_gpu) {
        aamd_log("FAIL: malloc");
        goto done;
    }

    aamd_saxpy_init(x, y_cpu, n);
    memcpy(y_gpu, y_cpu, (size_t)n * sizeof(float));

    double t0 = aamd_now_s();
    aamd_saxpy_cpu(AAMD_SAXPY_A, x, y_cpu, n, threads);
    double t_cpu = aamd_now_s() - t0;
    aamd_log("cpu %u-thread: %.4f s", threads, t_cpu);

    struct context c = {0};
    if (!context_create(&c)) {
        aamd_log("RESULT: FAIL (Vulkan device)");
        aamd_notify("AAMD probe FAIL (no Vulkan)");
        goto done;
    }

    double t_gpu = 0.0;
    if (!gpu_saxpy(&c, n, AAMD_SAXPY_A, x, y_gpu, &t_gpu)) {
        aamd_log("RESULT: FAIL (dispatch)");
        aamd_notify("AAMD probe FAIL (dispatch)");
        goto teardown;
    }

    float max_err = 0.0f;
    uint32_t mismatches = aamd_saxpy_compare(y_gpu, y_cpu, n, &max_err);
    double gb = (double)n * sizeof(float) * 3.0 / (1024.0 * 1024.0 * 1024.0);
    aamd_log("gpu saxpy: %.4f s  (%.2f GiB/s effective)", t_gpu, gb / t_gpu);
    aamd_log("gpu vs cpu: mismatches=%u max_abs_err=%g  speedup=%.2fx", mismatches,
             (double)max_err, t_cpu / t_gpu);
    aamd_log("sample gpu y[0]=%g y[n-1]=%g", (double)y_gpu[0], (double)y_gpu[n - 1]);

    if (mismatches == 0) {
        aamd_log("RESULT: PASS (Vulkan saxpy on RADV)");
        aamd_notify("AAMD probe PASS (GPU)");
        status = 0;
    } else {
        aamd_log("RESULT: FAIL (GPU/CPU mismatch)");
        aamd_notify("AAMD probe FAIL (mismatch)");
    }

teardown:
    if (c.device) {
        vkDeviceWaitIdle(c.device);
        vkDestroyFence(c.device, c.fence, NULL);
        vkDestroyCommandPool(c.device, c.pool, NULL);
        vkDestroyDevice(c.device, NULL);
    }
    if (c.instance)
        vkDestroyInstance(c.instance, NULL);

done:
    free(x);
    free(y_cpu);
    free(y_gpu);
    aamd_log("log: %s (also %s on a title)", AAMD_LOG_PATH, AAMD_TITLE_LOG_PATH);
    aamd_log_close();
    return status;
}
