#include <deque>

#include "engine/easy.h"

#include "queue.h"

using namespace arctic;  // NOLINT
using namespace queue_sim;  // NOLINT

constexpr double updateScreenInterval = 0.8;
constexpr double tickInterval = 1 * Usec;

void SetupCurrentPdiskModel(ClosedPipeLine &pipeline) {
    constexpr size_t startQueueSize = 16;

    constexpr size_t pdiskThreads = 1;
    constexpr double pdiskExecTime = 10 * Usec;

    constexpr size_t smbThreads = 1;
    constexpr double smbExecTime = 5 * Usec;

    constexpr size_t NVMeInflight = 128;
    PercentileTimeProcessor::Percentiles diskPercentiles;
    diskPercentiles.P10 = 10 * Usec;
    diskPercentiles.P50 = 20 * Usec;
    diskPercentiles.P90 = 70 * Usec;
    diskPercentiles.P99 = 100 * Usec;
    diskPercentiles.P999 = 200 * Usec;
    diskPercentiles.P9999 = 300 * Usec;
    diskPercentiles.P99999 = 500 * Usec;
    diskPercentiles.P100 = 1;

    pipeline.AddQueue("InputQ", startQueueSize);
    pipeline.AddFixedTimeExecutor("PDisk", pdiskThreads, pdiskExecTime);
    pipeline.AddQueue("SubmitQ", 0);
    pipeline.AddFixedTimeExecutor("Smb", smbThreads, smbExecTime);
    pipeline.AddPercentileTimeExecutor("NVMe", NVMeInflight, diskPercentiles);
    pipeline.AddFlushController("Flush");
}

void EasyMain() {
    ResizeScreen(1024, 768);

    ClosedPipeLine pipeline(GetEngine()->GetBackbuffer());
    SetupCurrentPdiskModel(pipeline);

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
