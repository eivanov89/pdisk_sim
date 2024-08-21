#include <deque>

#include "engine/easy.h"

#include "queue.h"

using namespace arctic;  // NOLINT
using namespace queue_sim;  // NOLINT

constexpr double updateScreenInterval = 0.8;
constexpr double tickInterval = 1 * Usec;

void SetupCurrentPdiskModel(ClosedPipeLine &pipeline) {
    constexpr size_t startQueueSize = 32;

    constexpr size_t pdiskThreads = 1;
    constexpr double pdiskExecTime = 10 * Usec;

    constexpr size_t smbThreads = 1;
    constexpr double smbExecTime = 5 * Usec;

    constexpr size_t NVMeInflight = 128;
    PercentileTimeProcessor::Percentiles diskPercentilesUs = {
        {16.47, 12 * Usec},
        {87.26, 25 * Usec},
        {99.7, 50 * Usec},
        {99.992, 100 * Usec},
        {99.9968, 200 * Usec},
        {1000, 4000 * Usec},
    };

    pipeline.AddQueue("InputQ", startQueueSize);
    pipeline.AddFixedTimeExecutor("PDisk", pdiskThreads, pdiskExecTime);
    pipeline.AddQueue("SubmitQ", 0);
    pipeline.AddFixedTimeExecutor("Smb", smbThreads, smbExecTime);
    pipeline.AddPercentileTimeExecutor("NVMe", NVMeInflight, diskPercentilesUs);
    pipeline.AddFlushController("Flush");
}

void SetupCurrentPdiskModelSlowNVMe(ClosedPipeLine &pipeline) {
    constexpr size_t startQueueSize = 32;

    constexpr size_t pdiskThreads = 1;
    constexpr double pdiskExecTime = 10 * Usec;

    constexpr size_t smbThreads = 1;
    constexpr double smbExecTime = 5 * Usec;

    constexpr size_t NVMeInflight = 128;
    PercentileTimeProcessor::Percentiles diskPercentilesUs = {
        {3.813, 12 * Usec},
        {51.59, 25 * Usec},
        {98.851, 50 * Usec},
        {99.956, 100 * Usec},
        {99.983, 200 * Usec},
        {99.983, 200 * Usec},
        {1000, 4000 * Usec},
    };

    pipeline.AddQueue("InputQ", startQueueSize);
    pipeline.AddFixedTimeExecutor("PDisk", pdiskThreads, pdiskExecTime);
    pipeline.AddQueue("SubmitQ", 0);
    pipeline.AddFixedTimeExecutor("Smb", smbThreads, smbExecTime);
    pipeline.AddPercentileTimeExecutor("NVMe", NVMeInflight, diskPercentilesUs);
    pipeline.AddFlushController("Flush");
}

void EasyMain() {
    ResizeScreen(1024, 768);

    ClosedPipeLine pipeline(GetEngine()->GetBackbuffer());
    SetupCurrentPdiskModelSlowNVMe(pipeline);

    double prevTime = 0;

    double currentTime = Now();

    for (size_t i = 0; i < 10000000000000; ++i) {
        if (IsKeyDownward(kKeyEscape)) {
            break;
        }
        AdvanceTime(tickInterval);
        pipeline.Tick(tickInterval);

        auto now = Now();
        if (now - prevTime > updateScreenInterval) {
            Clear();
            prevTime = now;
            pipeline.Draw();
            ShowFrame();
        }
    }
}
