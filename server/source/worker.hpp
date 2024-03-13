#ifndef HEADER_WORKER
#define HEADER_WORKER

#include <streaming_server.hpp>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
#include <array>

#include "camera.hpp"

struct Frame;
class Statistic;

enum WorkerState
{
    WORKER_STATE_ACTIVE,
    WORKER_STATE_INACTIVE
};

class WorkerFrame
{
public:
    Frame* frame;

    i3ds::LayerData layer_data;
    std::array<std::vector<i3ds::Index>, CAMERA_VIEW_COUNT> indices;
    std::array<std::vector<i3ds::Vertex>, CAMERA_VIEW_COUNT> vertices;

    std::array<bool, CAMERA_VIEW_COUNT> complete;

public:
    WorkerFrame() = default;
    ~WorkerFrame() = default;
};

class WorkerPool
{
private:
    std::vector<std::thread> mesh_threads;
    std::thread submit_thread;

    std::mutex input_mutex;
    std::mutex output_mutex;

    std::condition_variable input_condition;
    std::condition_variable mesh_condition;

    std::vector<WorkerFrame*> input_queue;
    std::vector<WorkerFrame*> output_queue;

    i3ds::StreamingServer* server = nullptr;

    WorkerState state = WORKER_STATE_INACTIVE;
    
public:
    WorkerPool() = default;

    bool create(i3ds::StreamingServer* server);
    void destroy(std::vector<Frame*>& frames);

    void submit(Frame* frame);
    void reclaim(std::vector<Frame*>& frames);

private:
    void worker_mesh(uint32_t view);
    void worker_submit();
};

#endif