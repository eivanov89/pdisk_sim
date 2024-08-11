#include <deque>

#include "engine/easy.h"

#include "queue.h"

using namespace arctic;  // NOLINT
using namespace queue_sim;  // NOLINT

constexpr double updateScreenInterval = 0.8;
constexpr double tickInterval = 1 * Usec;

void EasyMain() {
    ResizeScreen(1024, 768);

    ClosedPipeLine pipeline(GetEngine()->GetBackbuffer());

    pipeline.AddQueue("InputQ", 16);
    pipeline.AddFixedTimeExecutor("PDisk", 1, 10 * Usec);
    pipeline.AddQueue("SubmitQ", 0);
    pipeline.AddFixedTimeExecutor("Smb", 1, 5 * Usec);

    PercentileTimeProcessor::Percentiles diskPercentiles;
    diskPercentiles.P10 = 10 * Usec;
    diskPercentiles.P50 = 20 * Usec;
    diskPercentiles.P90 = 70 * Usec;
    diskPercentiles.P99 = 100 * Usec;
    diskPercentiles.P999 = 200 * Usec;
    diskPercentiles.P9999 = 300 * Usec;
    diskPercentiles.P99999 = 500 * Usec;
    diskPercentiles.P100 = 1;

    pipeline.AddPercentileTimeExecutor("NVMe", 128, diskPercentiles);

    double prevTime = 0;

    while (!IsKeyDownward(kKeyEscape)) {
        // IsKeyDown -> hold

        double currentTime = Now();

        for (size_t i = 0; i < 10000000; ++i) {
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
}
