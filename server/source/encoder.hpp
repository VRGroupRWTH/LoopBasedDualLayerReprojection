#ifndef HEADER_ENCODER
#define HEADER_ENCODER

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <volk.h>
#include <nvEncodeAPI.h>
#include <cuda.h>
#include <cudaGL.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

#if defined(_WIN32)
typedef PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHRType;
typedef PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHRType;
#endif

typedef NVENCSTATUS(NVENCAPI* NvEncodeAPICreateInstanceType)(NV_ENCODE_API_FUNCTION_LIST*);
typedef NVENCSTATUS(NVENCAPI* NvEncodeAPIGetMaxSupportedVersionType)(uint32_t* version);

#ifdef _WIN32
    using PlatformHandle = void*;
    static constexpr PlatformHandle INVALID_HANDLE = nullptr;
#elif __unix__
    using PlatformHandle = int;
    static constexpr PlatformHandle INVALID_HANDLE = 0;
#else
#error "Unsupported Platform"
#endif

class EncoderContext;

enum EncoderCodec
{
    ENCODER_CODEC_H264,
    ENCODER_CODEC_H265,
    ENCODER_CODEC_AV1
};

enum EncoderMode
{
    ENCODER_MODE_CONSTANT_BITRATE,
    ENCODER_MODE_CONSTANT_QUALITY
};

enum EncoderWorkerState
{
    ENCODER_WORKER_STATE_ACTIVE,
    ENCODER_WORKER_STATE_INACTIVE
};

struct EncoderFrame
{
    GLuint color_buffer = 0;
    GLuint color_memory = 0;
    GLuint copy_wait_semaphore = 0;

    VkImage vulkan_color_image = VK_NULL_HANDLE;
    VkImage vulkan_input_image = VK_NULL_HANDLE;

    VkDeviceMemory vulkan_color_memory = VK_NULL_HANDLE;
    VkDeviceMemory vulkan_input_memory = VK_NULL_HANDLE;
    VkSubresourceLayout vulkan_input_memory_layout;
    uint32_t vulkan_color_memory_size = 0;
    uint32_t vulkan_input_memory_size = 0;

    VkCommandBuffer vulkan_copy_command = VK_NULL_HANDLE;
    VkSemaphore vulkan_copy_wait_semaphore = VK_NULL_HANDLE;
    VkSemaphore vulkan_copy_signal_semaphore = VK_NULL_HANDLE;

    PlatformHandle platform_color_memory_handle = INVALID_HANDLE;
    PlatformHandle platform_input_memory_handle = INVALID_HANDLE;

    PlatformHandle platform_copy_wait_semaphore_handle = INVALID_HANDLE;
    PlatformHandle platform_copy_signal_semaphore_handle = INVALID_HANDLE;

    CUexternalMemory cuda_input_memory = nullptr;
    CUexternalSemaphore cuda_copy_signal_semaphore = nullptr;
    CUdeviceptr cuda_input_buffer = 0;

    void* nvenc_input_buffer = nullptr;
    void* nvenc_output_buffer = nullptr;
    void* nvenc_mapped_buffer = nullptr;

    std::vector<uint8_t> output_parameter_buffer;
    uint8_t* output_buffer = nullptr;
    uint32_t output_buffer_size = 0;

    std::chrono::high_resolution_clock::time_point encode_start;
    std::chrono::high_resolution_clock::time_point encode_end;
    double time_encode = 0.0;

    bool config_changed = false;
};

struct EncoderWorkerInput
{
    void* nvenc_output_buffer = nullptr;
};

struct EncoderWorkerOutput
{
    void* nvenc_output_buffer = nullptr;

    uint8_t* output_buffer = nullptr;
    uint32_t output_buffer_size = 0;

    std::chrono::high_resolution_clock::time_point encode_end;
};

class EncoderWorker
{
private:
    void* nvenc_session = nullptr;
    EncoderContext* context = nullptr;
    EncoderWorkerState state = ENCODER_WORKER_STATE_INACTIVE;

    std::thread thread;
    std::mutex mutex;
    std::condition_variable condition;

    std::vector<EncoderWorkerInput> input_queue;   //Protected by mutex
    std::vector<EncoderWorkerOutput> output_queue; //Protected by mutex

public:
    EncoderWorker() = default;

    bool create(EncoderContext* context, void* nvenc_session);
    void destroy();

    void submit_input(const EncoderWorkerInput& input);
    void receive_output(std::vector<EncoderWorkerOutput>& output);

private:
    void worker();
};

class EncoderContext
{
public:
    CUcontext cuda_context = 0;
    CUdevice cuda_device = 0;
    CUuuid cuda_device_uuid;

    VkInstance vulkan_instance = VK_NULL_HANDLE;
    VkPhysicalDevice vulkan_physical_device = VK_NULL_HANDLE;
    VkDevice vulkan_device = VK_NULL_HANDLE;
    VkQueue vulkan_queue = VK_NULL_HANDLE;
    VkCommandPool vulkan_command_pool = VK_NULL_HANDLE;

#if defined(_WIN32)
    vkGetMemoryWin32HandleKHRType vulkan_vkGetMemoryWin32HandleKHR = nullptr;
    vkGetSemaphoreWin32HandleKHRType vulkan_vkGetSemaphoreWin32HandleKHR = nullptr;
#endif

    void* nvenc_library = nullptr;
    NV_ENCODE_API_FUNCTION_LIST nvenc_functions;
    NvEncodeAPIGetMaxSupportedVersionType nvenc_NvEncodeAPIGetMaxSupportedVersion = nullptr;
    NvEncodeAPICreateInstanceType nvenc_NvEncodeAPICreateInstance = nullptr;

public:
    EncoderContext() = default;

    bool create();
    void destroy();
    
private:
    bool setup_cuda();
    bool setup_vulkan();
    bool setup_nvenc();

    void shutdown_cuda();
    void shutdown_vulkan();
    void shutdown_nvenc();
};

class Encoder
{
private:
    EncoderContext* context = nullptr;
    EncoderCodec codec = ENCODER_CODEC_H265;
    glm::uvec2 resolution = glm::uvec2(0);
    bool chroma_subsampling = true;

    EncoderWorker worker;
    std::vector<EncoderWorkerOutput> worker_output;

    void* nvenc_session = nullptr;
    NV_ENC_INITIALIZE_PARAMS nvenc_session_config;
    NV_ENC_CONFIG nvenc_encode_config;

    EncoderMode mode = ENCODER_MODE_CONSTANT_QUALITY;
    uint32_t frame_rate = 10;
    double bitrate = 1.0;
    double quality = 0.5;
    bool config_changed = true;

public:
    Encoder() = default;

    bool create(EncoderContext* context, EncoderCodec codec, const glm::uvec2& resolution, bool chroma_subsampling);
    void destroy();

    EncoderFrame* create_frame();
    bool destroy_frame(EncoderFrame* frame);
    bool submit_frame(EncoderFrame* frame);
    bool map_frame(EncoderFrame* frame);
    bool unmap_frame(EncoderFrame* frame);

    void set_mode(EncoderMode mode);
    void set_frame_rate(uint32_t frame_rate);
    void set_bitrate(double bitrate);
    void set_quality(double quality);

private:
    bool create_session();
    void destroy_session();

    bool create_external_image(VkImageTiling tiling, VkImageUsageFlags usage, VkImage& vulkan_image, VkDeviceMemory& vulkan_device_memory, uint32_t& vulkan_memory_size, PlatformHandle& platform_memory_handle);
    bool create_external_semaphore(VkSemaphore& vulkan_semaphore, PlatformHandle& platform_semaphore_handle);

    bool create_color_buffer(EncoderFrame* frame);
    bool create_input_buffer(EncoderFrame* frame);
    bool create_output_buffer(EncoderFrame* frame);
    bool create_semaphores(EncoderFrame* frame);
    bool create_copy_command(EncoderFrame* frame);

    bool apply_config();

    bool check_encode_support(GUID required_guid) const;
    bool check_feature_support(GUID encode_guid, NV_ENC_CAPS feature) const;
    bool check_profile_support(GUID encode_guid, GUID required_guid) const;
    bool check_preset_support(GUID encode_guid, GUID required_guid) const;
    bool check_format_support(GUID encode_guid, NV_ENC_BUFFER_FORMAT required_format) const;
};

#endif
