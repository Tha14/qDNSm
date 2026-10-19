#pragma once
#include <stdint.h>
#include <unistd.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <sys/time_util.h>
#include <vsh/paf.hpp>

uint64_t GetTimeNow();
void Sleep(uint64_t ms);

namespace Ease
{
    static float Linear(float t) 
    {
        return t;
    }
}

class TimerData
{
public:
    TimerData() = default;
    /* Returns true once the tween has finished, so the caller can erase safely. */
    bool Process();

    bool IsValueBoolean();

public:
    float* m_From{};
    float m_StartValue{};
    float m_EndValue{};

    float(*m_Interpolate)(float timeFactor){};
    void(*m_Callback)(float* pFrom){};

    uint64_t m_StartTime{};
    uint64_t m_Duration{};
};

class Timer
{
public:
    Timer() = default;

    bool IsAlreadyPresent(float* pFrom);
    TimerData* FindTimer(float* pFrom);

    void Add(float* from, float to, uint64_t duration, uint64_t startDelay = 0, float(*interpolation)(float) = Ease::Linear, void(*callback)(float*) = nullptr);

    void OnUpdate();
public:
    std::vector<TimerData> m_TimerStack{};
};

extern Timer g_Timer;