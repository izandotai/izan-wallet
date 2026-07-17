#include "platform/proc/child_process.hpp"

#include <stdexcept>
#include <vector>

#include <windows.h>

namespace izan::proc {

ChildProcess::~ChildProcess()
{
    close();
}

ChildProcess::ChildProcess(ChildProcess&& other) noexcept
    : m_process(other.m_process)
    , m_thread(other.m_thread)
{
    other.m_process = nullptr;
    other.m_thread = nullptr;
}

ChildProcess& ChildProcess::operator=(ChildProcess&& other) noexcept
{
    if (this != &other) {
        close();
        m_process = other.m_process;
        m_thread = other.m_thread;
        other.m_process = nullptr;
        other.m_thread = nullptr;
    }
    return *this;
}

void ChildProcess::close() noexcept
{
    if (m_thread) {
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    if (m_process) {
        CloseHandle(m_process);
        m_process = nullptr;
    }
}

ChildProcess ChildProcess::spawn(
    const std::string& command_line, std::span<void* const> inherit)
{
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<uint8_t> attrBuf(attrSize);
    auto* attrs
        = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize))
        throw std::runtime_error("InitializeProcThreadAttributeList failed");
    std::vector<HANDLE> handles(inherit.begin(), inherit.end());
    if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handles.data(), handles.size() * sizeof(HANDLE), nullptr,
            nullptr)) {
        DeleteProcThreadAttributeList(attrs);
        throw std::runtime_error("UpdateProcThreadAttribute failed");
    }

    STARTUPINFOEXA si {};
    si.StartupInfo.cb = sizeof(si);
    si.lpAttributeList = attrs;
    PROCESS_INFORMATION pi {};
    std::string cmd = command_line; // CreateProcess wants writable
    const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr,
        &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(attrs);
    if (!ok)
        throw std::runtime_error(
            "CreateProcess failed: " + std::to_string(GetLastError()));

    ChildProcess child;
    child.m_process = pi.hProcess;
    child.m_thread = pi.hThread;
    return child;
}

std::optional<uint32_t> ChildProcess::wait(uint32_t timeout_ms)
{
    if (WaitForSingleObject(m_process, timeout_ms) != WAIT_OBJECT_0)
        return std::nullopt;
    DWORD code = 0;
    GetExitCodeProcess(m_process, &code);
    return code;
}

void ChildProcess::kill() noexcept
{
    if (m_process)
        TerminateProcess(m_process, 1);
}

}
