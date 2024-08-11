#pragma once

#include <algorithm>
#include <deque>

#include "engine/easy.h"
#include "engine/easy_drawing.h"
#include "engine/easy_sprite.h"

namespace queue_sim {

using namespace arctic;  // NOLINT

static constexpr double Usec = 0.000001;
static constexpr double Msec = 0.001;

// ----------------------------
// out global time

static double CurrentTime = 0; // seconds

double Now() {
    return CurrentTime;
}

void AdvanceTime(double dt) {
    CurrentTime += dt;
}

// ----------------------------
// helpers

Font& GetFont() {
    static Font font;
    static bool loaded = false;
    if (!loaded) {
        font.Load("data/arctic_one_bmf.fnt");
        loaded = true;
    }

    return font;
}

std::string NumToStrWithSuffix(size_t num) {
    if (num < 1000) {
        return std::to_string(num);
    } else if (num < 1000000) {
        return std::to_string(num / 1000) + "K";
    } else if (num < 1000000000) {
        return std::to_string(num / 1000000) + "M";
    } else {
        return std::to_string(num / 1000000000) + "G";
    }
}

// ----------------------------
// Histogram

class Histogram {
private:
    std::vector<int> Buckets;
    std::vector<int> Counts;

public:
    Histogram(const std::vector<int>& bucketThresholds)
        : Buckets(bucketThresholds)
        , Counts(bucketThresholds.size() + 1, 0)
    {
        for (size_t i = 1; i < Buckets.size(); ++i) {
            if (Buckets[i] < Buckets[i - 1]) {
                throw std::runtime_error("Buckets must be sorted.");
            }
        }
    }

    void AddDuration(int duration) {
        for (size_t i = 0; i < Buckets.size(); ++i) {
            if (duration < Buckets[i]) {
                ++Counts[i];
                return;
            }
        }
        ++Counts.back();
    }

    int GetPercentile(int percentile) {
        if (percentile < 0 || percentile > 100) {
            throw std::runtime_error("Percentile must be between 0 and 100.");
        }

        int totalCounts = 0;
        for (int count : Counts) {
            totalCounts += count;
        }

        int threshold = (int)((percentile / 100.0) * totalCounts);
        int cumulativeCount = 0;

        for (size_t i = 0; i < Counts.size(); ++i) {
            cumulativeCount += Counts[i];
            if (cumulativeCount >= threshold) {
                return Buckets[i];
            }
        }

        throw std::runtime_error("Something went wrong while calculating the percentile.");
    }
};

// ----------------------------
// Event

struct Event {
    Event()
        : StartTime(Now())
    {
        StartStage();
    }

    Event(const Event& other) = default;

    double GetDuration() const {
        return Now() - StartTime;
    }

    double GetStageDuration() const {
        return Now() - StageStarted;
    }

    void StartStage() {
        StageStarted = Now();
    }

private:
    double StartTime = 0;
    double StageStarted = 0;
};

// ----------------------------
// IPipeLineItem

class IPipeLineItem {
public:
    virtual ~IPipeLineItem() = default;

    virtual void Tick(double dt) = 0;

    virtual bool IsReadyToPushEvent() const = 0;
    virtual void PushEvent(Event event) = 0;

    virtual bool IsReadyToPopEvent() const = 0;
    virtual Event PopEvent() = 0;

public:
    virtual void Draw(Sprite toSprite) = 0;
};

using PipeLineItemPtr = std::unique_ptr<IPipeLineItem>;

// ----------------------------
// Queue

class Queue : public IPipeLineItem {
public:
    Queue(const char* name, size_t initialEvents = 0)
        : Name(name)
        , QueueTimeUs({ 1, 2, 3, 4, 5, 8, 10, 20, 40, 50, 60, 70, 80,
                        100, 200, 250, 500, 750,
                        1000, 1500, 2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000, 512000
                      })
    {
        for (size_t i = 0; i < initialEvents; ++i) {
            Events.emplace_back();
        }
    }

    void Tick(double dt) override {
        /* do nothing */
    }

    bool IsReadyToPushEvent() const override {
        // the queue is infinite
        return true;
    }

    void PushEvent(Event event) override {
        event.StartStage();
        Events.push_back(event);
    }

    bool IsReadyToPopEvent() const override {
        return !Events.empty();
    }

    Event PopEvent() override {
        Event event = Events.front();
        QueueTimeUs.AddDuration((int)(event.GetStageDuration() * 1000000));

        Events.pop_front();
        return event;
    }

public:
    void Draw(Sprite toSprite) override {
        auto width = toSprite.Width();
        auto height = toSprite.Height();

        auto rWidth = width;
        auto rHeight = width / 2;
        auto yPos = height / 2 - rHeight / 2;

        // draw rectangle in the middle of the sprite
        Vec2Si32 bottomLeft(0, yPos);
        Vec2Si32 topRight(rWidth, yPos + rHeight);

        DrawRectangle(toSprite, bottomLeft, topRight, Rgba(255, 255, 255, 255));

        // draw queue length in the middle

        char text[128];
        auto queueLengthS = NumToStrWithSuffix(Events.size());

        snprintf(text, sizeof(text), "%s: %s\np90: %dus",
                 Name, queueLengthS.c_str(), QueueTimeUs.GetPercentile(90));
        GetFont().Draw(toSprite, text, 10, yPos + rHeight / 2 - 20);
    }

private:
    const char* Name;
    std::deque<Event> Events;
    Histogram QueueTimeUs;
};

// ----------------------------
// ProcessorBase

class ProcessorBase {
public:
    virtual void Tick(double dt) = 0;

    virtual void StartWork(Event event) {
        _Event = event;
        _IsWorking = true;
        StartTime = Now();
    }

    bool IsBusy() const {
        return _IsWorking || _IsEventReady;
    }

    bool IsWorking() const
    {
        return _IsWorking;
    }

    bool IsEventReady() const
    {
        return _IsEventReady;
    }

    void Reset() {
        _IsWorking = false;
        _IsEventReady = false;
        StartTime = 0;
        FinishTime = 0;
        _Event = {};
    }

    Event PopEvent() {
        auto event = _Event;
        Reset();
        return event;
    }

protected:
    bool _IsWorking = false;
    bool _IsEventReady = false;

    double StartTime = 0;
    double FinishTime = 0;

    Event _Event;
};

// ----------------------------
// FixedTimeProcessor

class FixedTimeProcessor : public ProcessorBase {
public:
    FixedTimeProcessor(double executionTime)
        : ExecutionTime(executionTime)
    {
    }

    void Tick(double dt) override {
        if (_IsWorking) {
            auto now = Now();
            if (now - StartTime >= ExecutionTime) {
                _IsWorking = false;
                _IsEventReady = true;
                FinishTime = now;
            }
        }
    }
private:
    double ExecutionTime;
};

// ----------------------------
// PercentileTimeProcessor

class PercentileTimeProcessor : public ProcessorBase {
public:
    struct Percentiles {
        double P10 = 0;
        double P50 = 0;
        double P90 = 0;
        double P99 = 0;
        double P999 = 0;
        double P9999 = 0;
        double P99999 = 0;
        double P100 = 0;

        Percentiles() = default;
    };

    PercentileTimeProcessor(Percentiles percentiles)
        : _Percentiles(percentiles)
    {
    }

    void StartWork(Event event) override {
        ProcessorBase::StartWork(event);

        auto r = Random(0, 100 * 1000 - 1);
        if (r < 10000) {
            ExecutionTime = _Percentiles.P10;
        } else if (r < 50000) {
            ExecutionTime = _Percentiles.P50;
        } else if (r < 90000) {
            ExecutionTime = _Percentiles.P90;
        } else if (r < 99000) {
            ExecutionTime = _Percentiles.P99;
        } else if (r < 99900) {
            ExecutionTime = _Percentiles.P999;
        } else if (r < 99990) {
            ExecutionTime = _Percentiles.P9999;
        } else if (r < 99999) {
            ExecutionTime = _Percentiles.P99999;
        } else {
            ExecutionTime = _Percentiles.P100;
        }
    }

    void Tick(double dt) override {
        if (_IsWorking) {
            auto now = Now();
            if (now - StartTime >= ExecutionTime) {
                _IsWorking = false;
                _IsEventReady = true;
                FinishTime = now;
            }
        }
    }

private:
    Percentiles _Percentiles;
    double ExecutionTime;
};

// ----------------------------
// Executor

template <typename ProcessorType>
class Executor : public IPipeLineItem {
public:

    template<typename... Args>
    Executor(const char* name, size_t processorCount, Args&&... args)
        : Name(name)
        , Processors(processorCount, ProcessorType(std::forward<Args>(args)...))
        , BusyProcessorCount(0)
    {
    }

    void Tick(double dt) override {
        BusyProcessorCount = 0;
        ReadyEventsCount = 0;

        for (auto& processor: Processors) {
            processor.Tick(dt);
            if (processor.IsBusy()) {
                ++BusyProcessorCount;
            }
            if (processor.IsEventReady()) {
                ++ReadyEventsCount;
            }
        }
    }

    bool IsReadyToPushEvent() const override {
        return BusyProcessorCount < Processors.size();
    }

    void PushEvent(Event event) override {
        if (!IsReadyToPushEvent()) {
            throw std::runtime_error("Executor is full");
        }

        event.StartStage();

        for (auto& processor: Processors) {
            if (!processor.IsBusy()) {
                processor.StartWork(event);
                if (processor.IsBusy()) {
                    ++BusyProcessorCount;
                }
                return;
            }
        }
    }

    bool IsReadyToPopEvent() const override {
        return ReadyEventsCount > 0;
    }

    Event PopEvent() override {
        if (!IsReadyToPopEvent()) {
            throw std::runtime_error("No events ready");
        }

        for (auto& processor: Processors) {
            if (processor.IsEventReady()) {
                --ReadyEventsCount;
                --BusyProcessorCount;
                return processor.PopEvent();
            }
        }

        throw std::runtime_error("No events ready");
    }

    size_t GetProcessorCount() const {
        return Processors.size();
    }

    size_t GetBusyProcessorCount() const {
        return BusyProcessorCount;
    }

public:
    void Draw(Sprite toSprite) override {
        auto width = toSprite.Width();
        auto height = toSprite.Height();

        auto minDimension = std::min(width, height);
        auto yPos = height / 2 - minDimension / 2;

        Vec2Si32 bottomLeft(0, yPos);
        Vec2Si32 topRight(minDimension, yPos + minDimension);

        DrawRectangle(toSprite, bottomLeft, topRight, Rgba(255, 255, 255, 255));

        char text[128];
        snprintf(text, sizeof(text), "%s: %ld/%ld", Name, BusyProcessorCount, Processors.size());
        GetFont().Draw(toSprite, text, 10, yPos + minDimension / 2);
    }

private:
    const char* Name;

    std::vector<ProcessorType> Processors;
    size_t BusyProcessorCount = 0;
    size_t ReadyEventsCount = 0;
};

using FixedTimeExecutor = Executor<FixedTimeProcessor>;

// ----------------------------
// ClosedPipeLine

// assumes, that the first stage is the input queue. Finished events are pushed back to the input queue
class ClosedPipeLine {
public:
    ClosedPipeLine(Sprite sprite)
        : _Sprite(sprite)
        , EventDurationsUs(
            { 1, 2, 3, 4, 5, 8, 10, 20, 40, 50, 60, 70, 80,
              100, 200, 250, 500, 750,
              1000, 1500, 2000, 4000, 8000, 16000, 32000, 64000, 128000, 256000, 512000,
              1000000, 1500000, 2000000, 3000000, 4000000
            })
    {
    }

    void AddQueue(const char* name, size_t initialEvents = 0) {
        Stages.emplace_back(new Queue(name, initialEvents));
    }

    void AddFixedTimeExecutor(const char* name, size_t processorCount, double executionTime) {
        Stages.emplace_back(new FixedTimeExecutor(name, processorCount, executionTime));
    }

    void AddPercentileTimeExecutor(const char* name, size_t processorCount, PercentileTimeProcessor::Percentiles percentiles) {
        Stages.emplace_back(new Executor<PercentileTimeProcessor>(name, processorCount, percentiles));
    }

    void Tick(double dt) {
        TotalTimePassed += dt;

        for (auto& stage: Stages) {
            stage->Tick(dt);
        }

        auto& inputQueue = Stages.front();
        auto& lastStage = Stages.back();

        while (lastStage->IsReadyToPopEvent() && inputQueue->IsReadyToPushEvent()) {
            auto event = lastStage->PopEvent();

            ++TotalFinishedEvents;
            RPS = (size_t)(TotalFinishedEvents / TotalTimePassed);
            EventDurationsUs.AddDuration((int)(event.GetDuration() * 1000000));

            inputQueue->PushEvent(Event());
        }

        if (Stages.size() <= 2) {
            return;
        }

        for (size_t i = Stages.size() - 1; i >= 1; --i) {
            auto& stage = Stages[i - 1];
            auto& nextStage = Stages[i];

            while (stage->IsReadyToPopEvent() && nextStage->IsReadyToPushEvent()) {
                auto event = stage->PopEvent();
                nextStage->PushEvent(event);
            }
        }

        // we need second pass for the case, when there are "instant" stages,
        // user to register the event and finish it in the same tick
        for (size_t i = Stages.size() - 1; i >= 1; --i) {
            auto& stage = Stages[i - 1];
            auto& nextStage = Stages[i];

            while (stage->IsReadyToPopEvent() && nextStage->IsReadyToPushEvent()) {
                auto event = stage->PopEvent();
                nextStage->PushEvent(event);
            }
        }
    }

public:
    void Draw() {
        auto stageCount = Stages.size();
        auto width = _Sprite.Width();
        auto height = _Sprite.Height();

        const Si32 spacing = 5;
        const Si32 widthWithoutSpacing = width - spacing * 2;
        const Si32 heightWithoutSpacing = height - spacing * 2;
        const Si32 footerHeight = 100;

        size_t space_between_stages = 20;
        Si32 stage_width = ((widthWithoutSpacing - space_between_stages * (stageCount - 1))) / stageCount;
        Si32 stage_height = heightWithoutSpacing - footerHeight;

        for (size_t i = 0; i < stageCount; ++i) {
            auto& stage = Stages[i];
            Si32 x = i * (stage_width + space_between_stages) + spacing;
            Si32 y = spacing + footerHeight;
            Sprite stageSprite;
            stageSprite.Reference(_Sprite, x, y, stage_width, stage_height);
            stage->Draw(stageSprite);
        }

        char text[512];
        snprintf(text, sizeof(text),
            "TimePassed: %.2f s, RPS: %ld\np10: %dus, p50: %dus, p90: %dus, p99: %dus, p100: %dus",
            TotalTimePassed,
            RPS,
            EventDurationsUs.GetPercentile(10),
            EventDurationsUs.GetPercentile(50),
            EventDurationsUs.GetPercentile(90),
            EventDurationsUs.GetPercentile(99),
            EventDurationsUs.GetPercentile(100)
        );
        GetFont().Draw(_Sprite, text, spacing, spacing);
    }

private:
    std::deque<PipeLineItemPtr> Stages;

    size_t TotalFinishedEvents = 0;
    double TotalTimePassed = 0;

    Histogram EventDurationsUs;
    size_t RPS = 0;

private:
    Sprite _Sprite;
};

} // namespace queue_sim
