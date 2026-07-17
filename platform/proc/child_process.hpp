#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace izan::proc {

// A spawned child process handle. Inherited handles are restricted to
// the explicit whitelist via PROC_THREAD_ATTRIBUTE_HANDLE_LIST — the
// child must receive its pipes and nothing else that happens to be
// inheritable in this process.
class ChildProcess {
public:
    ChildProcess() = default;
    ~ChildProcess();

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept;
    ChildProcess& operator=(ChildProcess&& other) noexcept;

    static ChildProcess spawn(
        const std::string& command_line, std::span<void* const> inherit);

    // Exit code if the process ended within the timeout.
    std::optional<uint32_t> wait(uint32_t timeout_ms);
    void kill() noexcept;

private:
    void close() noexcept;

    void* m_process = nullptr;
    void* m_thread = nullptr;
};

}
