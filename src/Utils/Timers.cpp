#include "Timers.hpp"

Timer g_Timer;

uint64_t GetTimeNow()
{
    return sys_time_get_system_time() / 1000;
} 

void Sleep(uint64_t ms)
{
    sys_timer_usleep(ms * 1000);
}

bool TimerData::Process()
{
    uint64_t timeNow = GetTimeNow();
    if (timeNow < m_StartTime)
        return false;

    uint64_t endTime = m_StartTime + m_Duration;
    bool isBoolean = IsValueBoolean();

    // if it is a boolean, we aint gonna interpolate it
    if (!isBoolean && timeNow < endTime)
    {
        uint64_t timePassed = timeNow - m_StartTime;
        float deltaTime = static_cast<float>(timePassed) / static_cast<float>(m_Duration); // should be between 0.0 and 1.0
        float progress = (m_EndValue - m_StartValue) * m_Interpolate(deltaTime);

        *m_From = m_StartValue + progress;
    }

    // if is finished
    if (timeNow >= endTime)
    {
        if (!isBoolean)
            *m_From = m_EndValue;

        /*
         * Report completion and let Timer::OnUpdate erase this entry. Erasing from here
         * meant the caller's `for (auto t : m_TimerStack)` loop was iterating a container
         * that Process had just modified - iterator invalidation, undefined behaviour.
         * It only triggered on tween completion, which is why pressing the D-pad (which
         * starts a tween) set the console's fan spinning.
         */
        if (m_Callback)
            m_Callback(m_From);

        return true;
    }

    return false;
}

bool TimerData::IsValueBoolean()
{
    uint8_t* pToggle = reinterpret_cast<uint8_t*>(m_From);
    if (*pToggle < 2) // in case u dont know, a bool is 8 bits and is either a 0 or a 1
        return m_Interpolate == nullptr && m_Callback != nullptr;

    return false;
}

bool Timer::IsAlreadyPresent(float* pFrom)
{
    for (auto& t : m_TimerStack)
        if (t.m_From == pFrom)
            return true;

    return false;
}

TimerData* Timer::FindTimer(float* pFrom)
{
    for (int i = 0; i < m_TimerStack.size(); i++)
        if (m_TimerStack[i].m_From == pFrom)
            return &m_TimerStack[i];

    return nullptr;
}

void Timer::Add(float* from, float to, uint64_t duration, uint64_t startDelay, float(*interpolation)(float), void(*callback)(float*))
{
    if (!from || (*from == to))
        return;

    bool isPresent = IsAlreadyPresent(from);

    TimerData newTimer = TimerData();
    TimerData& currentTimer = isPresent ? *FindTimer(from) : newTimer;
    currentTimer.m_From = from;
    currentTimer.m_StartValue = *from;
    currentTimer.m_EndValue = to;
    currentTimer.m_Interpolate = interpolation;
    currentTimer.m_Callback = callback;
    currentTimer.m_StartTime = GetTimeNow() + startDelay;
    currentTimer.m_Duration = duration;

    if (!isPresent)
        m_TimerStack.push_back(newTimer);
}

void Timer::OnUpdate()
{
    for (int i = (int)m_TimerStack.size() - 1; i >= 0; i--)
    {
        if (m_TimerStack[i].Process())
            m_TimerStack.erase(m_TimerStack.begin() + i);
    }
}