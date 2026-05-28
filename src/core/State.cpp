#include "State.hpp"

namespace cm
{
State& State::Get()
{
    static State instance;
    return instance;
}

void State::Push(const Action& a)
{
    std::scoped_lock lock(m_queueMutex);
    m_queue.push_back(a);
}

std::vector<Action> State::Drain()
{
    std::scoped_lock lock(m_queueMutex);
    std::vector<Action> out;
    out.swap(m_queue);
    return out;
}

void State::SetDisplay(const DisplayInfo& d)
{
    std::scoped_lock lock(m_dispMutex);
    m_display = d;
}

DisplayInfo State::GetDisplay()
{
    std::scoped_lock lock(m_dispMutex);
    return m_display;
}

void State::SetDiagnostics(const Diagnostics& d)
{
    std::scoped_lock lock(m_diagMutex);
    m_diag = d;
}

Diagnostics State::GetDiagnostics()
{
    std::scoped_lock lock(m_diagMutex);
    return m_diag;
}

void State::PushHistory(float health, float ram)
{
    std::scoped_lock lock(m_histMutex);
    m_history.Push(health, ram);
}

HistoryRing State::GetHistory()
{
    std::scoped_lock lock(m_histMutex);
    return m_history;
}

void State::SetLocation(int slot, const SavedLocation& loc)
{
    if (slot < 0 || slot >= kLocationSlots)
        return;
    std::scoped_lock lock(m_locMutex);
    m_locations[slot] = loc;
}

State::SavedLocation State::GetLocation(int slot)
{
    if (slot < 0 || slot >= kLocationSlots)
        return {};
    std::scoped_lock lock(m_locMutex);
    return m_locations[slot];
}
} // namespace cm
