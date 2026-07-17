#pragma once

#include <cstddef>
#include <cstdint>

namespace izan::ipc {

// One end of an anonymous pipe, owning the OS handle. Anonymous pipes
// are the wallet's password line: they have no name to squat on, and
// the only way to hold an end is to have created it or inherited it.
class PipeEnd {
public:
    PipeEnd() = default;

    explicit PipeEnd(void* handle)
        : m_handle(handle)
    {
    }

    ~PipeEnd()
    {
        close();
    }

    PipeEnd(const PipeEnd&) = delete;
    PipeEnd& operator=(const PipeEnd&) = delete;

    PipeEnd(PipeEnd&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    PipeEnd& operator=(PipeEnd&& other) noexcept
    {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    void* handle() const
    {
        return m_handle;
    }

    bool valid() const
    {
        return m_handle != nullptr;
    }

    void close() noexcept;

    // Blocking, whole-buffer transfer. false means the peer is gone
    // (broken pipe) — the session-collapse signal, not an error.
    bool write_all(const uint8_t* data, std::size_t size);
    bool read_all(uint8_t* data, std::size_t size);

    // Numeric handle value for passing across CreateProcess (the
    // handle itself is made inheritable by the spawner).
    uint64_t token() const
    {
        return reinterpret_cast<uint64_t>(m_handle);
    }

    static PipeEnd from_token(uint64_t token)
    {
        return PipeEnd(reinterpret_cast<void*>(token));
    }

private:
    void* m_handle = nullptr;
};

struct Pipe {
    PipeEnd read;
    PipeEnd write;
};

// Anonymous pipe; each end is marked inheritable only when the child
// is meant to receive it.
Pipe make_pipe(bool read_inheritable, bool write_inheritable);

}
