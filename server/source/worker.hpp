#ifndef HEADER_WORKER
#define HEADER_WORKER

#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
#include <array>

#include "server.hpp"
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
    Frame* frame = nullptr;
    LayerData* layer_data = nullptr;

    std::array<bool, SHARED_VIEW_COUNT_MAX> complete;

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

    std::vector<WorkerFrame*> input_queue;  //Protected by input_mutex
    std::vector<WorkerFrame*> output_queue; //Protected by output_mutex

    Server* server = nullptr;
    WorkerState state = WORKER_STATE_INACTIVE;
    uint32_t view_count = 0;
    bool export_enabled = false;
    
public:
    WorkerPool() = default;

    bool create(Server* server, uint32_t view_count, bool export_enabled);
    void destroy(std::vector<Frame*>& frames);

    void submit(Frame* frame);
    void reclaim(std::vector<Frame*>& frames);

private:
    void worker_mesh(uint32_t view);
    void worker_submit();

    std::string get_export_file_name(const std::string& request_file_name, uint32_t layer, uint32_t view);
};

#endif