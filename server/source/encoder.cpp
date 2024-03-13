#include "encoder.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <cstring>
#ifdef __unix__
#include <dlfcn.h>
#endif

bool EncoderWorker::create(EncoderContext* context, void* nvenc_session)
{
    this->nvenc_session = nvenc_session;
    this->context = context;
    this->state = ENCODER_WORKER_STATE_ACTIVE;

    this->thread = std::thread([this]()
    {
        this->worker();
    });

    return true;
}

void EncoderWorker::destroy()
{
    this->state = ENCODER_WORKER_STATE_INACTIVE;
    this->condition.notify_one();

    this->thread.join();
}

void EncoderWorker::submit_input(const EncoderWorkerInput& input)
{
    std::unique_lock<std::mutex> lock(this->mutex);

    this->input_queue.push_back(input);
    this->condition.notify_one();
}

void EncoderWorker::receive_output(std::vector<EncoderWorkerOutput>& output)
{
    std::unique_lock<std::mutex> lock(this->mutex);

    output.insert(output.end(), this->output_queue.begin(), this->output_queue.end());
    this->output_queue.clear();
}

void EncoderWorker::worker()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        while (this->input_queue.empty())
        {
            if (this->state == ENCODER_WORKER_STATE_INACTIVE)
            {
                return;
            }

            this->condition.wait(lock);
        }

        EncoderWorkerInput input = this->input_queue.front();
        this->input_queue.erase(this->input_queue.begin());
        lock.unlock();

        NV_ENC_LOCK_BITSTREAM lock_stream;
        memset(&lock_stream, 0, sizeof(lock_stream));
        lock_stream.version = NV_ENC_LOCK_BITSTREAM_VER;
        lock_stream.doNotWait = 0;
        lock_stream.getRCStats = 0;
        lock_stream.reservedBitFields = 0;
        lock_stream.outputBitstream = input.nvenc_output_buffer;
        lock_stream.sliceOffsets = nullptr;
        memset(lock_stream.reserved1, 0, sizeof(lock_stream.reserved1));
        memset(lock_stream.reserved2, 0, sizeof(lock_stream.reserved2));
        memset(lock_stream.reservedInternal, 0, sizeof(lock_stream.reservedInternal));

        if (this->context->nvenc_functions.nvEncLockBitstream(this->nvenc_session, &lock_stream) != NV_ENC_SUCCESS)
        {
            std::cout << "Can't lock output bistream" << std::endl;
        }

        if (lock_stream.bitstreamSizeInBytes == 0)
        {
            std::cout << "Received output buffer with zero bytes" << std::endl;
        }

        EncoderWorkerOutput output;
        output.nvenc_output_buffer = input.nvenc_output_buffer;
        output.output_buffer = (uint8_t*)lock_stream.bitstreamBufferPtr;
        output.output_buffer_size = lock_stream.bitstreamSizeInBytes;

        lock.lock();
        this->output_queue.push_back(output);
        lock.unlock();
    }
}

bool EncoderContext::create()
{
    if (!this->setup_cuda())
    {
        return false;
    }

    if (!this->setup_vulkan())
    {
        return false;
    }

    if (!this->setup_nvenc())
    {
        return false;
    }

    return true;
}

void EncoderContext::destroy()
{
    this->shutdown_nvenc();
    this->shutdown_vulkan();
    this->shutdown_cuda();
}

bool EncoderContext::setup_cuda()
{
    if (cuInit(0) != CUDA_SUCCESS)
    {
        return false;
    }

    uint32_t device_count = 0;

    if (cuGLGetDevices(&device_count, &this->cuda_device, 1, CU_GL_DEVICE_LIST_ALL) != CUDA_SUCCESS)
    {
        return false;
    }

    if (cuDeviceGetUuid(&this->cuda_device_uuid, this->cuda_device) != CUDA_SUCCESS)
    {
        return false;
    }

    if (cuCtxCreate(&this->cuda_context, 0, this->cuda_device) != CUDA_SUCCESS)
    {
        return false;
    }

    return true;
}

bool EncoderContext::setup_vulkan()
{
    if (volkInitialize() != VK_SUCCESS)
    {
        return false;
    }

    VkApplicationInfo application_info;
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = nullptr;
    application_info.pApplicationName = "Depth Discontinuity Trace";
    application_info.applicationVersion = 0;
    application_info.pEngineName = "None";
    application_info.engineVersion = 0;
    application_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info;
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = nullptr;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &application_info;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = nullptr;
    instance_info.enabledExtensionCount = 0;
    instance_info.ppEnabledExtensionNames = nullptr;

    if (vkCreateInstance(&instance_info, nullptr, &this->vulkan_instance) != VK_SUCCESS)
    {
        return false;
    }

    volkLoadInstance(this->vulkan_instance);

    uint32_t physical_device_count = 0;

    if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, nullptr) != VK_SUCCESS)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    
    if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, physical_devices.data()) != VK_SUCCESS)
    {
        return false;
    }

    for (VkPhysicalDevice physical_device : physical_devices)
    {
        VkPhysicalDeviceProperties2 device_properties;
        VkPhysicalDeviceIDProperties id_properties;

        device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device_properties.pNext = &id_properties;

        id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        id_properties.pNext = nullptr;

        vkGetPhysicalDeviceProperties2(physical_device, &device_properties);

        if (memcmp(id_properties.deviceUUID, this->cuda_device_uuid.bytes, sizeof(id_properties.deviceUUID)) == 0)
        {
            this->vulkan_physical_device = physical_device;

            break;
        }
    }

    if (this->vulkan_physical_device == VK_NULL_HANDLE)
    {
        return false;
    }

#ifdef _WIN32
    std::vector<const char*> device_extensions =
    {
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    };
#elif __unix__
    std::vector<const char*> device_extensions =
    {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    };
#else
#error "Platform not supported"
#endif

    float queue_pripority = 0.0f;

    VkDeviceQueueCreateInfo queue_info;
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = nullptr;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = 0;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_pripority;

    VkDeviceCreateInfo device_info;
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = nullptr;
    device_info.flags = 0;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = nullptr;
    device_info.enabledExtensionCount = device_extensions.size();
    device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.pEnabledFeatures = nullptr;

    if (vkCreateDevice(this->vulkan_physical_device, &device_info, nullptr, &this->vulkan_device) != VK_SUCCESS)
    {
        return false;
    }

    vkGetDeviceQueue(this->vulkan_device, 0, 0, &this->vulkan_queue);

    VkCommandPoolCreateInfo command_pool_info;
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = nullptr;
    command_pool_info.flags = 0;
    command_pool_info.queueFamilyIndex = 0;

    if (vkCreateCommandPool(this->vulkan_device, &command_pool_info, nullptr, &this->vulkan_command_pool) != VK_SUCCESS)
    {
        return false;
    }

#if defined(_WIN32)
    this->vulkan_vkGetMemoryWin32HandleKHR = (vkGetMemoryWin32HandleKHRType)vkGetInstanceProcAddr(vulkan_instance, "vkGetMemoryWin32HandleKHR");

    if (this->vulkan_vkGetMemoryWin32HandleKHR == nullptr)
    {
        return false;
    }

    this->vulkan_vkGetSemaphoreWin32HandleKHR = (vkGetSemaphoreWin32HandleKHRType)vkGetInstanceProcAddr(vulkan_instance, "vkGetSemaphoreWin32HandleKHR");

    if (this->vulkan_vkGetSemaphoreWin32HandleKHR == nullptr)
    {
        return false;
    }
#endif

    return true;
}

bool EncoderContext::setup_nvenc()
{
#if defined(_WIN32)
#if defined(_WIN64)
    this->nvenc_library = LoadLibrary("nvEncodeAPI64.dll");
#else
    this->nvenc_library = LoadLibrary("nvEncodeAPI.dll");
#endif
#elif defined(__unix__)
    this->nvenc_library = dlopen("libnvidia-encode.so.1", RTLD_LAZY);
#else 
#error "Not implemented for this platform!"
#endif

    if (this->nvenc_library == nullptr)
    {
        return false;
    }

#if defined(_WIN32)
    this->nvenc_NvEncodeAPIGetMaxSupportedVersion = (NvEncodeAPIGetMaxSupportedVersionType)GetProcAddress((HMODULE)nvenc_library, "NvEncodeAPIGetMaxSupportedVersion");
    this->nvenc_NvEncodeAPICreateInstance = (NvEncodeAPICreateInstanceType)GetProcAddress((HMODULE)nvenc_library, "NvEncodeAPICreateInstance");
#elif defined(__unix__)
    this->nvenc_NvEncodeAPIGetMaxSupportedVersion = (NvEncodeAPIGetMaxSupportedVersionType)dlsym(nvenc_library, "NvEncodeAPIGetMaxSupportedVersion");
    this->nvenc_NvEncodeAPICreateInstance = (NvEncodeAPICreateInstanceType)dlsym(nvenc_library, "NvEncodeAPICreateInstance");
#else
#error "Not implemented for this platform!"
#endif

    if (this->nvenc_NvEncodeAPIGetMaxSupportedVersion == nullptr)
    {
        return false;
    }

    if (this->nvenc_NvEncodeAPICreateInstance == nullptr)
    {
        return false;
    }

    uint32_t current_version = (NVENCAPI_MAJOR_VERSION << 4) | (NVENCAPI_MINOR_VERSION & 0xF);
    uint32_t max_version = 0;

    if (this->nvenc_NvEncodeAPIGetMaxSupportedVersion(&max_version) != NV_ENC_SUCCESS)
    {
        return false;
    }

    if (max_version < current_version)
    {
        return false;
    }

    this->nvenc_functions.version = NV_ENCODE_API_FUNCTION_LIST_VER;

    if (this->nvenc_NvEncodeAPICreateInstance(&this->nvenc_functions) != NV_ENC_SUCCESS)
    {
        return false;
    }

    return true;
}

void EncoderContext::shutdown_cuda()
{
    if (this->cuda_context != nullptr)
    {
        cuCtxDestroy(this->cuda_context);
    }

    this->cuda_context = nullptr;
    this->cuda_device = 0;
}

void EncoderContext::shutdown_vulkan()
{
    if (this->vulkan_command_pool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(this->vulkan_device, this->vulkan_command_pool, nullptr);
    }

    if (this->vulkan_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(this->vulkan_device, nullptr);
    }

    if (this->vulkan_instance != nullptr)
    {
        vkDestroyInstance(this->vulkan_instance, nullptr);
    }

    this->vulkan_instance = VK_NULL_HANDLE;
    this->vulkan_physical_device = VK_NULL_HANDLE;
    this->vulkan_device = VK_NULL_HANDLE;
    this->vulkan_queue = VK_NULL_HANDLE;
    this->vulkan_command_pool = VK_NULL_HANDLE;

#if defined(_WIN32)
    this->vulkan_vkGetMemoryWin32HandleKHR = nullptr;
    this->vulkan_vkGetSemaphoreWin32HandleKHR = nullptr;
#endif
}

void EncoderContext::shutdown_nvenc()
{
    if (this->nvenc_library != nullptr)
    {
#if defined(_WIN32)
        FreeLibrary((HMODULE)this->nvenc_library);
#elif defined(__unix__)
        dlclose(this->nvenc_library);
#else
#error "Not implemented for this platform!"
#endif
    }

    this->nvenc_library = nullptr;
    this->nvenc_NvEncodeAPIGetMaxSupportedVersion = nullptr;
    this->nvenc_NvEncodeAPICreateInstance = nullptr;
}

bool Encoder::create(EncoderContext* context, EncoderCodec codec, const glm::uvec2& resolution, bool chroma_subsampling)
{
    this->context = context;
    this->codec = codec;
    this->resolution = resolution;
    this->chroma_subsampling = chroma_subsampling;

    if(!this->create_session())
    {
        return false;
    }

    if (!this->worker.create(this->context, this->nvenc_session))
    {
        return false;
    }

    return true;
}

void Encoder::destroy()
{
    this->worker.destroy();

    this->destroy_session();

    this->context = nullptr;
}

EncoderFrame* Encoder::create_frame()
{
    EncoderFrame* frame = new EncoderFrame;

    if (!this->create_color_buffer(frame))
    {
        this->destroy_frame(frame);

        return nullptr;
    }

    if (!this->create_input_buffer(frame))
    {
        this->destroy_frame(frame);

        return nullptr;
    }

    if (!this->create_output_buffer(frame))
    {
        this->destroy_frame(frame);

        return nullptr;
    }

    if (!this->create_semaphores(frame))
    {
        this->destroy_frame(frame);

        return nullptr;
    }

    if (!this->create_copy_command(frame))
    {
        this->destroy_frame(frame);

        return nullptr;
    }

    return frame;
}

bool Encoder::destroy_frame(EncoderFrame* frame)
{
    if (frame->nvenc_mapped_buffer != nullptr)
    {
        this->context->nvenc_functions.nvEncUnmapInputResource(this->nvenc_session, frame->nvenc_mapped_buffer);
    }

    if (frame->nvenc_input_buffer != nullptr)
    {
        this->context->nvenc_functions.nvEncUnregisterResource(this->nvenc_session, frame->nvenc_input_buffer);
    }

    if (frame->nvenc_output_buffer != nullptr)
    {
        this->context->nvenc_functions.nvEncDestroyBitstreamBuffer(this->nvenc_session, frame->nvenc_output_buffer);
    }

    frame->nvenc_mapped_buffer = nullptr;
    frame->nvenc_input_buffer = nullptr;
    frame->nvenc_output_buffer = nullptr;

    if (frame->cuda_input_buffer != 0)
    {
        cuMemFree(frame->cuda_input_buffer);
    }

    if (frame->cuda_input_memory != nullptr)
    {
        cuDestroyExternalMemory(frame->cuda_input_memory);
    }

    if (frame->cuda_copy_signal_semaphore != nullptr)
    {
        cuDestroyExternalSemaphore(frame->cuda_copy_signal_semaphore);
    }

    frame->cuda_input_buffer = 0;
    frame->cuda_input_memory = nullptr;
    frame->cuda_copy_signal_semaphore = nullptr;

    if (frame->color_buffer != 0)
    {
        glDeleteTextures(1, &frame->color_buffer);
    }

    if (frame->color_memory != 0)
    {
        glDeleteMemoryObjectsEXT(1, &frame->color_memory);
    }

    if (frame->copy_wait_semaphore != 0)
    {
        glDeleteSemaphoresEXT(1, &frame->copy_wait_semaphore);
    }

    frame->color_buffer = 0;
    frame->color_memory = 0;
    frame->copy_wait_semaphore = 0;

#if defined(_WIN32)
    auto close_handle = [](PlatformHandle handle) { CloseHandle(handle); };
#elif defined(__unix__)
    auto close_handle = [](PlatformHandle handle) { close(handle); };
#else
#error "Unsupported Platform"
#endif

    if (frame->platform_color_memory_handle != INVALID_HANDLE)
    {
        close_handle(frame->platform_color_memory_handle);
        frame->platform_color_memory_handle = INVALID_HANDLE;
    }

    if (frame->platform_input_memory_handle != INVALID_HANDLE)
    {
        close_handle(frame->platform_input_memory_handle );
        frame->platform_input_memory_handle = INVALID_HANDLE;
    }

    if (frame->platform_copy_wait_semaphore_handle != INVALID_HANDLE)
    {
        close_handle(frame->platform_copy_wait_semaphore_handle );
        frame->platform_copy_wait_semaphore_handle = INVALID_HANDLE;
    }

    if (frame->platform_copy_signal_semaphore_handle != INVALID_HANDLE)
    {
        close_handle(frame->platform_copy_signal_semaphore_handle );
        frame->platform_copy_signal_semaphore_handle = INVALID_HANDLE;
    }

    if (vkQueueWaitIdle(this->context->vulkan_queue) != VK_SUCCESS)
    {
        return false;
    }

    if (frame->vulkan_copy_command != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(this->context->vulkan_device, this->context->vulkan_command_pool, 1, &frame->vulkan_copy_command);
    }

    if (frame->vulkan_copy_wait_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(this->context->vulkan_device, frame->vulkan_copy_wait_semaphore, nullptr);
    }

    if (frame->vulkan_copy_signal_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(this->context->vulkan_device, frame->vulkan_copy_signal_semaphore, nullptr);
    }

    if (frame->vulkan_color_image !=  VK_NULL_HANDLE)
    {
        vkDestroyImage(this->context->vulkan_device, frame->vulkan_color_image, nullptr);
    }

    if (frame->vulkan_input_image != VK_NULL_HANDLE)
    {
        vkDestroyImage(this->context->vulkan_device, frame->vulkan_input_image, nullptr);
    }

    if (frame->vulkan_color_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(this->context->vulkan_device, frame->vulkan_color_memory, nullptr);
    }

    if (frame->vulkan_input_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(this->context->vulkan_device, frame->vulkan_input_memory, nullptr);
    }

    frame->vulkan_color_image = VK_NULL_HANDLE;
    frame->vulkan_input_image = VK_NULL_HANDLE;
    frame->vulkan_color_memory = VK_NULL_HANDLE;
    frame->vulkan_input_memory = VK_NULL_HANDLE;
    frame->vulkan_copy_command = VK_NULL_HANDLE;
    frame->vulkan_copy_wait_semaphore = VK_NULL_HANDLE;
    frame->vulkan_copy_signal_semaphore = VK_NULL_HANDLE;

    delete frame;

    return true;
}

bool Encoder::submit_frame(EncoderFrame* frame)
{
    frame->config_changed = this->config_changed;

    if (this->config_changed)
    {
        if (!this->apply_config())
        {
            return false;
        }

        this->config_changed = false;
    }

    GLenum color_buffer_layout = GL_LAYOUT_TRANSFER_SRC_EXT;
    glSignalSemaphoreEXT(frame->copy_wait_semaphore, 0, nullptr, 1, &frame->color_buffer, &color_buffer_layout);

    GLenum error = glGetError();

    if (error != 0)
    {
        return false;
    }

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame->vulkan_copy_wait_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame->vulkan_copy_command;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame->vulkan_copy_signal_semaphore;

    if (vkQueueSubmit(this->context->vulkan_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        return false;
    }

    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS wait_parameters;
    memset(&wait_parameters.params, 0, sizeof(wait_parameters.params));
    wait_parameters.flags = 0;
    memset(wait_parameters.reserved, 0, sizeof(wait_parameters.reserved));

    if (cuWaitExternalSemaphoresAsync(&frame->cuda_copy_signal_semaphore, &wait_parameters, 1, 0) != CUDA_SUCCESS)
    {
        return false;
    }

    NV_ENC_MAP_INPUT_RESOURCE map_info;
    map_info.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    map_info.subResourceIndex = 0;
    map_info.inputResource = 0;
    map_info.registeredResource = frame->nvenc_input_buffer;
    map_info.mappedResource = nullptr;
    map_info.mappedBufferFmt = (NV_ENC_BUFFER_FORMAT)0;
    memset(map_info.reserved1, 0, sizeof(map_info.reserved1));
    memset(map_info.reserved2, 0, sizeof(map_info.reserved2));

    if (this->context->nvenc_functions.nvEncMapInputResource(this->nvenc_session, &map_info) != NV_ENC_SUCCESS)
    {
        return false;
    }

    frame->nvenc_mapped_buffer = map_info.mappedResource;

    NV_ENC_PIC_PARAMS encode_parameters;
    encode_parameters.version = NV_ENC_PIC_PARAMS_VER;
    encode_parameters.inputWidth = this->resolution.x;
    encode_parameters.inputHeight = this->resolution.y;
    encode_parameters.inputPitch = frame->vulkan_input_memory_layout.rowPitch;
    encode_parameters.encodePicFlags = 0;
    encode_parameters.frameIdx = 0;
    encode_parameters.inputTimeStamp = 0;
    encode_parameters.inputDuration = 0;
    encode_parameters.inputBuffer = frame->nvenc_mapped_buffer;
    encode_parameters.outputBitstream = frame->nvenc_output_buffer;
    encode_parameters.completionEvent = nullptr;
    encode_parameters.bufferFmt = map_info.mappedBufferFmt;
    encode_parameters.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    encode_parameters.pictureType = (NV_ENC_PIC_TYPE)0;
    memset(encode_parameters.meHintCountsPerBlock, 0, sizeof(encode_parameters.meHintCountsPerBlock));
    encode_parameters.meExternalHints = nullptr;
    memset(encode_parameters.reserved1, 0, sizeof(encode_parameters.reserved1));
    memset(encode_parameters.reserved2, 0, sizeof(encode_parameters.reserved2));
    encode_parameters.qpDeltaMap = nullptr;
    encode_parameters.qpDeltaMapSize = 0;
    encode_parameters.reservedBitFields = 0;
    memset(encode_parameters.meHintRefPicDist, 0, sizeof(encode_parameters.meHintRefPicDist));
    encode_parameters.alphaBuffer = nullptr;
    encode_parameters.meExternalSbHints = nullptr;
    encode_parameters.meSbHintsCount = 0;
    encode_parameters.stateBufferIdx = 0;
    encode_parameters.outputReconBuffer = nullptr;
    memset(encode_parameters.reserved3, 0, sizeof(encode_parameters.reserved3));
    memset(encode_parameters.reserved4, 0, sizeof(encode_parameters.reserved4));

    if (this->codec == ENCODER_CODEC_H264)
    {
        NV_ENC_PIC_PARAMS_MVC motion_vector_parameters;
        motion_vector_parameters.version = NV_ENC_PIC_PARAMS_MVC_VER;
        motion_vector_parameters.viewID = 0;
        motion_vector_parameters.temporalID = 0;
        motion_vector_parameters.priorityID = 0;
        memset(motion_vector_parameters.reserved1, 0, sizeof(motion_vector_parameters.reserved1));
        memset(motion_vector_parameters.reserved2, 0, sizeof(motion_vector_parameters.reserved2));

        NV_ENC_PIC_PARAMS_H264_EXT codec_extention_parameters;
        codec_extention_parameters.mvcPicParams = motion_vector_parameters;
        memset(codec_extention_parameters.reserved1, 0, sizeof(codec_extention_parameters.reserved1));

        NV_ENC_PIC_PARAMS_H264 codec_parameters;
        codec_parameters.displayPOCSyntax = 0;
        codec_parameters.reserved3 = 0;
        codec_parameters.refPicFlag = 0;
        codec_parameters.colourPlaneId = 0;
        codec_parameters.forceIntraRefreshWithFrameCnt = 0;
        codec_parameters.constrainedFrame = 1;
        codec_parameters.sliceModeDataUpdate = 1;
        codec_parameters.ltrMarkFrame = 0;
        codec_parameters.ltrUseFrames = 0;
        codec_parameters.reservedBitFields = 0;
        codec_parameters.sliceTypeData = 0;
        codec_parameters.sliceTypeArrayCnt = 0;
        codec_parameters.seiPayloadArrayCnt = 0;
        codec_parameters.seiPayloadArray = nullptr;
        codec_parameters.sliceMode = 3;
        codec_parameters.sliceModeData = 10;
        codec_parameters.ltrMarkFrameIdx = 0;
        codec_parameters.ltrUseFrameBitmap = 0;
        codec_parameters.ltrUsageMode = 0;
        codec_parameters.forceIntraSliceCount = 0;
        codec_parameters.forceIntraSliceIdx = 0;
        codec_parameters.h264ExtPicParams = codec_extention_parameters;
        memset(&codec_parameters.timeCode, 0, sizeof(codec_parameters.timeCode));
        memset(codec_parameters.reserved, 0, sizeof(codec_parameters.reserved));
        memset(codec_parameters.reserved2, 0, sizeof(codec_parameters.reserved2));

        encode_parameters.codecPicParams.h264PicParams = codec_parameters;
    }

    else if (this->codec == ENCODER_CODEC_H265)
    {
        NV_ENC_PIC_PARAMS_HEVC codec_parameters;
        codec_parameters.displayPOCSyntax = 0;
        codec_parameters.refPicFlag = 0;
        codec_parameters.temporalId = 0;
        codec_parameters.forceIntraRefreshWithFrameCnt = 0;
        codec_parameters.constrainedFrame = 1;
        codec_parameters.sliceModeDataUpdate = 1;
        codec_parameters.ltrMarkFrame = 0;
        codec_parameters.ltrUseFrames = 0;
        codec_parameters.reservedBitFields = 0;
        codec_parameters.sliceTypeData = 0;
        codec_parameters.sliceTypeArrayCnt = 0;
        codec_parameters.sliceMode = 3;
        codec_parameters.sliceModeData = 10;
        codec_parameters.ltrMarkFrameIdx = 0;
        codec_parameters.ltrUseFrameBitmap = 0;
        codec_parameters.ltrUsageMode = 0;
        codec_parameters.seiPayloadArrayCnt = 0;
        codec_parameters.reserved = 0;
        codec_parameters.seiPayloadArray = nullptr;
        memset(&codec_parameters.timeCode, 0, sizeof(codec_parameters.timeCode));
        memset(codec_parameters.reserved2, 0, sizeof(codec_parameters.reserved2));
        memset(codec_parameters.reserved3, 0, sizeof(codec_parameters.reserved3));

        encode_parameters.codecPicParams.hevcPicParams = codec_parameters;
    }

    else if (this->codec == ENCODER_CODEC_AV1)
    {
        NV_ENC_PIC_PARAMS_AV1 codec_parameters;
        codec_parameters.displayPOCSyntax = 0;
        codec_parameters.refPicFlag = 0;
        codec_parameters.temporalId = 0;
        codec_parameters.forceIntraRefreshWithFrameCnt = 0;
        codec_parameters.goldenFrameFlag = 0;
        codec_parameters.arfFrameFlag = 0;
        codec_parameters.arf2FrameFlag = 0;
        codec_parameters.bwdFrameFlag = 0;
        codec_parameters.overlayFrameFlag = 0;
        codec_parameters.showExistingFrameFlag = 0;
        codec_parameters.errorResilientModeFlag = 0;
        codec_parameters.tileConfigUpdate = 0;
        codec_parameters.enableCustomTileConfig = 0;
        codec_parameters.filmGrainParamsUpdate = 0;
        codec_parameters.reservedBitFields = 0;
        codec_parameters.numTileColumns = 0;
        codec_parameters.numTileRows = 0;
        codec_parameters.tileWidths = nullptr;
        codec_parameters.tileHeights  = nullptr;
        codec_parameters.obuPayloadArrayCnt = 0;
        codec_parameters.reserved = 0;
        codec_parameters.obuPayloadArray = nullptr;
        codec_parameters.filmGrainParams = nullptr;
        memset(codec_parameters.reserved2, 0, sizeof(codec_parameters.reserved2));
        memset(codec_parameters.reserved3, 0, sizeof(codec_parameters.reserved3));

        encode_parameters.codecPicParams.av1PicParams = codec_parameters;
    }

    else
    {
        return false;
    }

    if (this->context->nvenc_functions.nvEncEncodePicture(this->nvenc_session, &encode_parameters) != NV_ENC_SUCCESS) //The encoder should only output p frames. Therefore there should be no NV_ENC_ERR_NEED_MORE_INPUT
    {
        return false;
    }

    EncoderWorkerInput input;
    input.nvenc_output_buffer = frame->nvenc_output_buffer;

    this->worker.submit_input(input);

    return true;
}

bool Encoder::map_frame(EncoderFrame* frame)
{
    this->worker.receive_output(this->worker_output);

    bool complete = false;

    for (uint32_t index = 0; index < this->worker_output.size(); index++)
    {
        const EncoderWorkerOutput& output = this->worker_output[index];

        if (output.nvenc_output_buffer == frame->nvenc_output_buffer)
        {
            frame->output_buffer = output.output_buffer;
            frame->output_buffer_size = output.output_buffer_size;

            this->worker_output.erase(this->worker_output.begin() + index);
            complete = true;

            break;
        }
    }

    if (!complete)
    {
        return false;
    }

    frame->output_parameter_buffer.clear();

    if (frame->config_changed)
    {
        frame->output_parameter_buffer.resize(512);
        uint32_t output_parameter_buffer_size = 0;

        NV_ENC_SEQUENCE_PARAM_PAYLOAD parameter_info;
        parameter_info.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;
        parameter_info.inBufferSize = frame->output_parameter_buffer.size();
        parameter_info.spsId = 0;
        parameter_info.ppsId = 0;
        parameter_info.spsppsBuffer = frame->output_parameter_buffer.data();
        parameter_info.outSPSPPSPayloadSize = &output_parameter_buffer_size;
        memset(parameter_info.reserved, 0, sizeof(parameter_info.reserved));
        memset(parameter_info.reserved2, 0, sizeof(parameter_info.reserved2));

        if (this->context->nvenc_functions.nvEncGetSequenceParams(this->nvenc_session, &parameter_info) != NV_ENC_SUCCESS)
        {
            return false;
        }

        frame->output_parameter_buffer.resize(output_parameter_buffer_size);
    }

    return true;
}

bool Encoder::unmap_frame(EncoderFrame* frame)
{
    if (this->context->nvenc_functions.nvEncUnlockBitstream(this->nvenc_session, frame->nvenc_output_buffer) != NV_ENC_SUCCESS)
    {
        return false;
    }

    if (this->context->nvenc_functions.nvEncUnmapInputResource(this->nvenc_session, frame->nvenc_mapped_buffer) != NV_ENC_SUCCESS)
    {
        return false;
    }

    frame->nvenc_mapped_buffer = nullptr;

    return true;
}

void Encoder::set_mode(EncoderMode mode)
{
    this->mode = mode;
    this->config_changed = true;
}

void Encoder::set_frame_rate(uint32_t frame_rate)
{
    this->frame_rate = frame_rate;
    this->config_changed = true;
}

void Encoder::set_bitrate(double bitrate)
{
    this->bitrate = bitrate;
    this->config_changed = true;
}

void Encoder::set_quality(double quality)
{
    this->quality = quality;
    this->config_changed = true;
}

bool Encoder::create_session()
{
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_parameters;
    session_parameters.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    session_parameters.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
    session_parameters.device = (void*)this->context->cuda_context;
    session_parameters.reserved = 0;
    session_parameters.apiVersion = NVENCAPI_VERSION;
    memset(session_parameters.reserved1, 0, sizeof(session_parameters.reserved1));
    memset(session_parameters.reserved2, 0, sizeof(session_parameters.reserved2));

    if (this->context->nvenc_functions.nvEncOpenEncodeSessionEx(&session_parameters, &this->nvenc_session) != NV_ENC_SUCCESS)
    {
        return false;
    }

    GUID codec_guid;
    GUID profile_guid;

    switch (codec)
    {
    case ENCODER_CODEC_H264:
        codec_guid = NV_ENC_CODEC_H264_GUID;

        if (chroma_subsampling)
        {
            profile_guid = NV_ENC_H264_PROFILE_HIGH_GUID;
        }

        else
        {
            profile_guid = NV_ENC_H264_PROFILE_HIGH_444_GUID;
        }

        break;
    case ENCODER_CODEC_H265:
        codec_guid = NV_ENC_CODEC_HEVC_GUID;

        if (chroma_subsampling)
        {
            profile_guid = NV_ENC_HEVC_PROFILE_MAIN_GUID;
        }

        else
        {
            profile_guid = NV_ENC_HEVC_PROFILE_FREXT_GUID;
        }

        break;
    case ENCODER_CODEC_AV1:
        codec_guid = NV_ENC_CODEC_AV1_GUID;
        profile_guid = NV_ENC_AV1_PROFILE_MAIN_GUID;

        if (!chroma_subsampling)
        {
            std::cout << "Encoder only supports AV1 encoding with chroma subsampling!" << std::endl;

            return false;
        }

        break;
    default:
        return false;
    }

    if (!this->check_encode_support(codec_guid))
    {
        return false;
    }

    if (!this->check_profile_support(codec_guid, profile_guid))
    {
        return false;
    }

    if (!this->check_preset_support(codec_guid, NV_ENC_PRESET_P1_GUID))
    {
        return false;
    }

    if (!this->check_format_support(codec_guid, NV_ENC_BUFFER_FORMAT_ABGR))
    {
        return false;
    }

    if (!chroma_subsampling)
    {
        if (!this->check_feature_support(codec_guid, NV_ENC_CAPS_SUPPORT_YUV444_ENCODE))
        {
            return false;
        }
    }

    NV_ENC_PRESET_CONFIG preset_config;
    preset_config.version = NV_ENC_PRESET_CONFIG_VER;
    memset(&preset_config.presetCfg, 0, sizeof(preset_config.presetCfg));
    preset_config.presetCfg.version = NV_ENC_CONFIG_VER;
    preset_config.presetCfg.rcParams.version = NV_ENC_RC_PARAMS_VER;
    memset(preset_config.reserved1, 0, sizeof(preset_config.reserved1));
    memset(preset_config.reserved2, 0, sizeof(preset_config.reserved2));

    if (this->context->nvenc_functions.nvEncGetEncodePresetConfigEx(this->nvenc_session, codec_guid, NV_ENC_PRESET_P1_GUID, NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY, &preset_config) != NV_ENC_SUCCESS)
    {
        return false;
    }

    this->nvenc_encode_config = preset_config.presetCfg;
    this->nvenc_encode_config.profileGUID = profile_guid;
    this->nvenc_encode_config.rcParams.version = NV_ENC_RC_PARAMS_VER;
    this->nvenc_encode_config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
    this->nvenc_encode_config.rcParams.enableAQ = 1;
    this->nvenc_encode_config.rcParams.aqStrength = 0;

    switch (codec)
    {
    case ENCODER_CODEC_H264:
        this->nvenc_encode_config.encodeCodecConfig.h264Config.disableSPSPPS = 1;
        this->nvenc_encode_config.encodeCodecConfig.h264Config.enableIntraRefresh = 1;
        this->nvenc_encode_config.encodeCodecConfig.h264Config.intraRefreshPeriod = std::max(this->frame_rate * 2, 20u);
        this->nvenc_encode_config.encodeCodecConfig.h264Config.intraRefreshCnt = 10;

        if (chroma_subsampling)
        {
            this->nvenc_encode_config.encodeCodecConfig.h264Config.chromaFormatIDC = 1;
        }

        else
        {
            this->nvenc_encode_config.encodeCodecConfig.h264Config.chromaFormatIDC = 3;
        }

        break;
    case ENCODER_CODEC_H265:
        this->nvenc_encode_config.encodeCodecConfig.hevcConfig.disableSPSPPS = 1;
        this->nvenc_encode_config.encodeCodecConfig.hevcConfig.enableIntraRefresh = 1;
        this->nvenc_encode_config.encodeCodecConfig.hevcConfig.intraRefreshPeriod = std::max(this->frame_rate * 2, 20u);
        this->nvenc_encode_config.encodeCodecConfig.hevcConfig.intraRefreshCnt = 10;

        if (chroma_subsampling)
        {
            this->nvenc_encode_config.encodeCodecConfig.hevcConfig.chromaFormatIDC = 1;
        }

        else
        {
            this->nvenc_encode_config.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
        }

        break;
    case ENCODER_CODEC_AV1:
        this->nvenc_encode_config.encodeCodecConfig.av1Config.disableSeqHdr = 1;
        this->nvenc_encode_config.encodeCodecConfig.av1Config.enableIntraRefresh = 1;
        this->nvenc_encode_config.encodeCodecConfig.av1Config.intraRefreshPeriod = std::max(this->frame_rate * 2, 20u);
        this->nvenc_encode_config.encodeCodecConfig.av1Config.intraRefreshCnt = 10;
        break;
    default:
        return false;
    }

    this->nvenc_session_config.version = NV_ENC_INITIALIZE_PARAMS_VER;
    this->nvenc_session_config.encodeGUID = codec_guid;
    this->nvenc_session_config.presetGUID = NV_ENC_PRESET_P1_GUID;
    this->nvenc_session_config.encodeWidth = resolution.x;
    this->nvenc_session_config.encodeHeight = resolution.y;
    this->nvenc_session_config.darWidth = resolution.x;
    this->nvenc_session_config.darHeight = resolution.y;
    this->nvenc_session_config.frameRateNum = this->frame_rate;
    this->nvenc_session_config.frameRateDen = 1;
    this->nvenc_session_config.enableEncodeAsync = 0;
    this->nvenc_session_config.enablePTD = 1;
    this->nvenc_session_config.reportSliceOffsets = 0;
    this->nvenc_session_config.enableSubFrameWrite = 0;
    this->nvenc_session_config.enableExternalMEHints = 0;
    this->nvenc_session_config.enableMEOnlyMode = 0;
    this->nvenc_session_config.enableWeightedPrediction = 0;
    this->nvenc_session_config.splitEncodeMode = NV_ENC_SPLIT_DISABLE_MODE;
    this->nvenc_session_config.enableOutputInVidmem = 0;
    this->nvenc_session_config.enableReconFrameOutput = 0;
    this->nvenc_session_config.enableOutputStats = 0;
    this->nvenc_session_config.reservedBitFields = 0;
    this->nvenc_session_config.privDataSize = 0;
    this->nvenc_session_config.privData = nullptr;
    this->nvenc_session_config.encodeConfig = &this->nvenc_encode_config;
    this->nvenc_session_config.maxEncodeWidth = resolution.x;
    this->nvenc_session_config.maxEncodeHeight = resolution.y;
    memset(this->nvenc_session_config.maxMEHintCountsPerBlock, 0, sizeof(this->nvenc_session_config.maxMEHintCountsPerBlock));
    this->nvenc_session_config.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
    this->nvenc_session_config.bufferFormat = (NV_ENC_BUFFER_FORMAT)0;
    this->nvenc_session_config.numStateBuffers = 0;
    this->nvenc_session_config.outputStatsLevel = NV_ENC_OUTPUT_STATS_NONE;
    memset(this->nvenc_session_config.reserved, 0, sizeof(this->nvenc_session_config.reserved));
    memset(this->nvenc_session_config.reserved2, 0, sizeof(this->nvenc_session_config.reserved2));

    if (this->context->nvenc_functions.nvEncInitializeEncoder(this->nvenc_session, &this->nvenc_session_config) != NV_ENC_SUCCESS)
    {
        return false;
    }

    return true;
}

void Encoder::destroy_session()
{
    if (this->nvenc_session != nullptr)
    {
        this->context->nvenc_functions.nvEncDestroyEncoder(this->nvenc_session);
    }

    this->nvenc_session = nullptr;
}

bool Encoder::create_external_image(VkImageTiling tiling, VkImageUsageFlags usage, VkImage& vulkan_image, VkDeviceMemory& vulkan_device_memory, uint32_t& vulkan_memory_size, PlatformHandle& platform_memory_handle)
{
    VkExternalMemoryImageCreateInfo export_image_info;
    export_image_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    export_image_info.pNext = nullptr;
#if defined(_WIN32)
    export_image_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__unix__)
    export_image_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
#error "Not implemented for this platform!"
#endif

    VkImageCreateInfo image_info;
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = &export_image_info;
    image_info.flags = 0;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = this->resolution.x;
    image_info.extent.height = this->resolution.y;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = tiling;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = nullptr;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(this->context->vulkan_device, &image_info, nullptr, &vulkan_image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(this->context->vulkan_device, vulkan_image, &memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(this->context->vulkan_physical_device, &memory_properties);

    bool memory_found = false;
    uint32_t memory_index = 0;
    VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; index++)
    {
        if ((memory_requirements.memoryTypeBits & (1 << index)) == 0)
        {
            continue;
        }

        if ((memory_properties.memoryTypes[index].propertyFlags & memory_flags) == 0)
        {
            continue;
        }

        memory_found = true;
        memory_index = index;

        break;
    }

    if (!memory_found)
    {
        return false;
    }

    VkExportMemoryAllocateInfo export_info;
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.pNext = nullptr;
#if defined(_WIN32)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__unix__)
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
#error "Not implemented for this platform!"
#endif

    VkMemoryAllocateInfo allocation_info;
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.pNext = &export_info;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_index;

    if (vkAllocateMemory(this->context->vulkan_device, &allocation_info, nullptr, &vulkan_device_memory) != VK_SUCCESS)
    {
        return false;
    }

    if (vkBindImageMemory(this->context->vulkan_device, vulkan_image, vulkan_device_memory, 0) != VK_SUCCESS)
    {
        return false;
    }

#if defined(_WIN32)
    VkMemoryGetWin32HandleInfoKHR memory_info;
    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    memory_info.pNext = nullptr;
    memory_info.memory = vulkan_device_memory;
    memory_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    if (this->context->vulkan_vkGetMemoryWin32HandleKHR(this->context->vulkan_device, &memory_info, &platform_memory_handle) != VK_SUCCESS)
    {
        return false;
    }
#elif defined(__unix__)
    VkMemoryGetFdInfoKHR memory_info;
    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    memory_info.pNext = nullptr;
    memory_info.memory = vulkan_device_memory;
    memory_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    if (vkGetMemoryFdKHR(this->context->vulkan_device, &memory_info, (int*)&platform_memory_handle) != VK_SUCCESS)
    {
        return false;
    }
#else
#error "Not implemented for this platform!"
#endif

    vulkan_memory_size = memory_requirements.size;

    return true;
}

bool Encoder::create_external_semaphore(VkSemaphore& vulkan_semaphore, PlatformHandle& platform_semaphore_handle)
{
    VkExportSemaphoreCreateInfo semaphore_export_info;
    semaphore_export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    semaphore_export_info.pNext = nullptr;
#if defined(_WIN32)
    semaphore_export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#elif defined(__unix__)
    semaphore_export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#else
#error "Not implemented for this platform!"
#endif

    VkSemaphoreCreateInfo semaphore_info;
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = &semaphore_export_info;
    semaphore_info.flags = 0;

    if (vkCreateSemaphore(this->context->vulkan_device, &semaphore_info, nullptr, &vulkan_semaphore) != VK_SUCCESS)
    {
        return false;
    }

#if defined(_WIN32)
    VkSemaphoreGetWin32HandleInfoKHR export_info;
    export_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
    export_info.pNext = nullptr;
    export_info.semaphore = vulkan_semaphore;
    export_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    if (this->context->vulkan_vkGetSemaphoreWin32HandleKHR(this->context->vulkan_device, &export_info, (HANDLE*)&platform_semaphore_handle) != VK_SUCCESS)
    {
        return false;
    }
#elif defined(__unix__)
    VkSemaphoreGetFdInfoKHR export_info;
    export_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    export_info.pNext = nullptr;
    export_info.semaphore = vulkan_semaphore;
    export_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    if (vkGetSemaphoreFdKHR(this->context->vulkan_device, &export_info, &platform_semaphore_handle))
    {
        return false;
    }
#else
#error "Not implemented for this platform!"
#endif

    return true;
}

bool Encoder::create_color_buffer(EncoderFrame* frame)
{
    if (!this->create_external_image(VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, frame->vulkan_color_image, frame->vulkan_color_memory, frame->vulkan_color_memory_size, frame->platform_color_memory_handle))
    {
        return false;
    }

    glCreateMemoryObjectsEXT(1, &frame->color_memory);
#ifdef _WIN32
    glImportMemoryWin32HandleEXT(frame->color_memory, frame->vulkan_color_memory_size, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, frame->platform_color_memory_handle);
#elif __unix__
    glImportMemoryFdEXT(frame->color_memory, frame->vulkan_color_memory_size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, frame->platform_color_memory_handle);
#else
#error "Unsupported platform"
#endif

    glGenTextures(1, &frame->color_buffer);
    glBindTexture(GL_TEXTURE_2D, frame->color_buffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);

    glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, this->resolution.x, this->resolution.y, frame->color_memory, 0);

    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum error = glGetError();

    if (error != 0)
    {
        return false;
    }

    return true;
}

bool Encoder::create_input_buffer(EncoderFrame* frame)
{
    if (!this->create_external_image(VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, frame->vulkan_input_image, frame->vulkan_input_memory, frame->vulkan_input_memory_size, frame->platform_input_memory_handle))
    {
        return false;
    }

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC external_memory_description;
#if defined(_WIN32)
    external_memory_description.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
    external_memory_description.handle.win32.handle = frame->platform_input_memory_handle;
    external_memory_description.handle.win32.name = nullptr;
#elif defined(__unix__)
    external_memory_description.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
    external_memory_description.handle.fd = (int)frame->platform_input_memory_handle;
#else
#error "Not implemented for this platform!"
#endif
    external_memory_description.size = frame->vulkan_input_memory_size;
    external_memory_description.flags = 0;
    memset(external_memory_description.reserved, 0, sizeof(external_memory_description.reserved));

    if (cuImportExternalMemory(&frame->cuda_input_memory, &external_memory_description) != CUDA_SUCCESS)
    {
        return false;
    }

    VkImageSubresource image_subresources;
    image_subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresources.mipLevel = 0;
    image_subresources.arrayLayer = 0;

    vkGetImageSubresourceLayout(this->context->vulkan_device, frame->vulkan_input_image, &image_subresources, &frame->vulkan_input_memory_layout);

    CUDA_EXTERNAL_MEMORY_BUFFER_DESC buffer_description;
    buffer_description.offset = frame->vulkan_input_memory_layout.offset;
    buffer_description.size = frame->vulkan_input_memory_layout.size;
    buffer_description.flags = 0;
    memset(buffer_description.reserved, 0, sizeof(buffer_description.reserved));

    if (cuExternalMemoryGetMappedBuffer(&frame->cuda_input_buffer, frame->cuda_input_memory, &buffer_description) != CUDA_SUCCESS)
    {
        return false;
    }

    NV_ENC_REGISTER_RESOURCE register_info;
    register_info.version = NV_ENC_REGISTER_RESOURCE_VER;
    register_info.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
    register_info.width = this->resolution.x;
    register_info.height = this->resolution.y;
    register_info.pitch = frame->vulkan_input_memory_layout.rowPitch;
    register_info.subResourceIndex = 0;
    register_info.resourceToRegister = (void*)frame->cuda_input_buffer;
    register_info.registeredResource = nullptr;
    register_info.bufferFormat = NV_ENC_BUFFER_FORMAT_ABGR;
    register_info.bufferUsage = NV_ENC_INPUT_IMAGE;
    register_info.pInputFencePoint = nullptr;
    memset(register_info.chromaOffset, 0, sizeof(register_info.chromaOffset));
    memset(register_info.reserved1, 0, sizeof(register_info.reserved1));
    memset(register_info.reserved2, 0, sizeof(register_info.reserved2));

    if (this->context->nvenc_functions.nvEncRegisterResource(this->nvenc_session, &register_info) != NV_ENC_SUCCESS)
    {
        return false;
    }

    frame->nvenc_input_buffer = register_info.registeredResource;

    return true;
}

bool Encoder::create_output_buffer(EncoderFrame* frame)
{
    NV_ENC_CREATE_BITSTREAM_BUFFER create_info;
    create_info.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    create_info.size = 0;
    create_info.memoryHeap = (NV_ENC_MEMORY_HEAP)0;
    create_info.reserved = 0;
    create_info.bitstreamBuffer = nullptr;
    create_info.bitstreamBufferPtr = nullptr;
    memset(create_info.reserved1, 0, sizeof(create_info.reserved1));
    memset(create_info.reserved2, 0, sizeof(create_info.reserved2));

    if (this->context->nvenc_functions.nvEncCreateBitstreamBuffer(this->nvenc_session, &create_info) != NV_ENC_SUCCESS)
    {
        return false;
    }

    frame->nvenc_output_buffer = create_info.bitstreamBuffer;

    return true;
}

bool Encoder::create_semaphores(EncoderFrame* frame)
{
    if (!this->create_external_semaphore(frame->vulkan_copy_wait_semaphore, frame->platform_copy_wait_semaphore_handle))
    {
        return false;
    }

    glGenSemaphoresEXT(1, &frame->copy_wait_semaphore);
#ifdef _WIN32
    glImportSemaphoreWin32HandleEXT(frame->copy_wait_semaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, frame->platform_copy_wait_semaphore_handle);
#elif __unix__
    glImportSemaphoreFdEXT(frame->copy_wait_semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, frame->platform_copy_wait_semaphore_handle);
#else
#error "Unsupported Platform"
#endif

    GLenum error = glGetError();

    if (error != 0)
    {
        return false;
    }

    if (!this->create_external_semaphore(frame->vulkan_copy_signal_semaphore, frame->platform_copy_signal_semaphore_handle))
    {
        return false;
    }

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC semaphore_description;
#if defined(_WIN32)
    semaphore_description.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32;
    semaphore_description.handle.win32.handle = frame->platform_copy_signal_semaphore_handle;
    semaphore_description.handle.win32.name = 0;
#elif defined(__unix__)
    semaphore_description.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD;
    semaphore_description.handle.fd = frame->platform_copy_signal_semaphore_handle;
#else
#error "Not implemented for this platform!"
#endif
    semaphore_description.flags = 0;
    memset(semaphore_description.reserved, 0, sizeof(semaphore_description.reserved));

    if (cuImportExternalSemaphore(&frame->cuda_copy_signal_semaphore, &semaphore_description) != CUDA_SUCCESS)
    {
        return false;
    }

    return true;
}

bool Encoder::create_copy_command(EncoderFrame* frame)
{
    VkCommandBufferAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.commandPool = this->context->vulkan_command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(this->context->vulkan_device, &allocate_info, &frame->vulkan_copy_command) != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(frame->vulkan_copy_command, &begin_info) != VK_SUCCESS)
    {
        return false;
    }

    std::array<VkImageMemoryBarrier, 2> begin_barriers;
    begin_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    begin_barriers[0].pNext = nullptr;
    begin_barriers[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    begin_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    begin_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    begin_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    begin_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    begin_barriers[0].dstQueueFamilyIndex = 0;
    begin_barriers[0].image = frame->vulkan_color_image;
    begin_barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    begin_barriers[0].subresourceRange.baseMipLevel = 0;
    begin_barriers[0].subresourceRange.levelCount = 1;
    begin_barriers[0].subresourceRange.baseArrayLayer = 0;
    begin_barriers[0].subresourceRange.layerCount = 1;

    begin_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    begin_barriers[1].pNext = nullptr;
    begin_barriers[1].srcAccessMask = 0;
    begin_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    begin_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    begin_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    begin_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    begin_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    begin_barriers[1].image = frame->vulkan_input_image;
    begin_barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    begin_barriers[1].subresourceRange.baseMipLevel = 0;
    begin_barriers[1].subresourceRange.levelCount = 1;
    begin_barriers[1].subresourceRange.baseArrayLayer = 0;
    begin_barriers[1].subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(frame->vulkan_copy_command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, begin_barriers.size(), begin_barriers.data());

    VkImageCopy image_copy;
    image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.srcSubresource.mipLevel = 0;
    image_copy.srcSubresource.baseArrayLayer = 0;
    image_copy.srcSubresource.layerCount = 1;
    image_copy.srcOffset.x = 0;
    image_copy.srcOffset.y = 0;
    image_copy.srcOffset.z = 0;
    image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.dstSubresource.mipLevel = 0;
    image_copy.dstSubresource.baseArrayLayer = 0;
    image_copy.dstSubresource.layerCount = 1;
    image_copy.dstOffset.x = 0;
    image_copy.dstOffset.y = 0;
    image_copy.dstOffset.z = 0;
    image_copy.extent.width = this->resolution.x;
    image_copy.extent.height = this->resolution.y;
    image_copy.extent.depth = 1;

    vkCmdCopyImage(frame->vulkan_copy_command, frame->vulkan_color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frame->vulkan_input_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

    VkImageMemoryBarrier end_barrier;
    end_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    end_barrier.pNext = nullptr;
    end_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    end_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    end_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    end_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    end_barrier.srcQueueFamilyIndex = 0;
    end_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    end_barrier.image = frame->vulkan_input_image;
    end_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    end_barrier.subresourceRange.baseMipLevel = 0;
    end_barrier.subresourceRange.levelCount = 1;
    end_barrier.subresourceRange.baseArrayLayer = 0;
    end_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(frame->vulkan_copy_command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &end_barrier);

    if (vkEndCommandBuffer(frame->vulkan_copy_command) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

bool Encoder::apply_config()
{
    NV_ENC_INITIALIZE_PARAMS session_config = this->nvenc_session_config;
    NV_ENC_CONFIG encode_config = this->nvenc_encode_config;
    session_config.encodeConfig = &encode_config;

    if (this->mode == ENCODER_MODE_CONSTANT_QUALITY)
    {
        encode_config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
        encode_config.rcParams.constQP.qpIntra = (uint32_t)((1.0 - this->quality) * 51);
        encode_config.rcParams.constQP.qpInterP = (uint32_t)((1.0 - this->quality) * 51);
        encode_config.rcParams.constQP.qpInterB = (uint32_t)((1.0 - this->quality) * 51);
    }

    else if (this->mode == ENCODER_MODE_CONSTANT_BITRATE)
    {
        encode_config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encode_config.rcParams.averageBitRate = (uint32_t)(this->bitrate * 1000000.0);
        encode_config.rcParams.maxBitRate = this->nvenc_encode_config.rcParams.averageBitRate;
        encode_config.rcParams.vbvBufferSize = (uint32_t)(this->nvenc_encode_config.rcParams.averageBitRate * (1.0f / this->frame_rate)) * 5;
        encode_config.rcParams.vbvInitialDelay = this->nvenc_encode_config.rcParams.vbvBufferSize;
    }

    else
    {
        return false;
    }

    NV_ENC_RECONFIGURE_PARAMS reconfig;
    reconfig.version = NV_ENC_RECONFIGURE_PARAMS_VER;
    reconfig.reInitEncodeParams = session_config;
    reconfig.resetEncoder = 1;
    reconfig.forceIDR = 1;
    reconfig.reserved = 0;

    if (this->context->nvenc_functions.nvEncReconfigureEncoder(this->nvenc_session, &reconfig) != NV_ENC_SUCCESS)
    {
        return false;
    }

    return true;
}

bool Encoder::check_encode_support(GUID required_guid) const
{
    uint32_t guid_count = 0;

    if (this->context->nvenc_functions.nvEncGetEncodeGUIDCount(this->nvenc_session, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    std::vector<GUID> guid_list;
    guid_list.resize(guid_count);

    if (this->context->nvenc_functions.nvEncGetEncodeGUIDs(this->nvenc_session, guid_list.data(), guid_count, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    for (const GUID& guid : guid_list)
    {
        if (memcmp(&guid, &required_guid, sizeof(guid)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool Encoder::check_feature_support(GUID encode_guid, NV_ENC_CAPS feature) const
{
    NV_ENC_CAPS_PARAM config;
    config.version = NV_ENC_CAPS_PARAM_VER;
    config.capsToQuery = feature;
    memset(config.reserved, 0, sizeof(config.reserved));

    int32_t value = 0;

    if (this->context->nvenc_functions.nvEncGetEncodeCaps(this->nvenc_session, encode_guid, &config, &value) != VK_SUCCESS)
    {
        return false;
    }

    if (value == 1)
    {
        return true;
    }

    return false;
}

bool Encoder::check_profile_support(GUID encode_guid, GUID required_guid) const
{
    uint32_t guid_count = 0;

    if (this->context->nvenc_functions.nvEncGetEncodeProfileGUIDCount(this->nvenc_session, encode_guid, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    std::vector<GUID> guid_list;
    guid_list.resize(guid_count);

    if (this->context->nvenc_functions.nvEncGetEncodeProfileGUIDs(this->nvenc_session, encode_guid, guid_list.data(), guid_count, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    for (const GUID& guid : guid_list)
    {
        if (memcmp(&guid, &required_guid, sizeof(guid)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool Encoder::check_preset_support(GUID encode_guid, GUID required_guid) const
{
    uint32_t guid_count = 0;

    if (this->context->nvenc_functions.nvEncGetEncodePresetCount(this->nvenc_session, encode_guid, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    std::vector<GUID> guid_list;
    guid_list.resize(guid_count);

    if (this->context->nvenc_functions.nvEncGetEncodePresetGUIDs(this->nvenc_session, encode_guid, guid_list.data(), guid_count, &guid_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    for (const GUID& guid : guid_list)
    {
        if (memcmp(&guid, &required_guid, sizeof(guid)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool Encoder::check_format_support(GUID encode_guid, NV_ENC_BUFFER_FORMAT required_format) const
{
    uint32_t format_count = 0;

    if (this->context->nvenc_functions.nvEncGetInputFormatCount(this->nvenc_session, encode_guid, &format_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    std::vector<NV_ENC_BUFFER_FORMAT> format_list;
    format_list.resize(format_count);

    if (this->context->nvenc_functions.nvEncGetInputFormats(this->nvenc_session, encode_guid, format_list.data(), format_count, &format_count) != NV_ENC_SUCCESS)
    {
        return false;
    }

    for (const NV_ENC_BUFFER_FORMAT& format : format_list)
    {
        if (memcmp(&format, &required_format, sizeof(format)) == 0)
        {
            return true;
        }
    }

    return false;
}
