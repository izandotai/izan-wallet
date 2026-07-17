#include "platform/ipc/pipe.hpp"

#include <stdexcept>

#include <windows.h>

namespace izan::ipc {

void PipeEnd::close() noexcept
{
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
}

bool PipeEnd::write_all(const uint8_t* data, std::size_t size)
{
    while (size) {
        DWORD written = 0;
        if (!WriteFile(m_handle, data, DWORD(size), &written, nullptr)
            || !written)
            return false;
        data += written;
        size -= written;
    }
    return true;
}

bool PipeEnd::read_all(uint8_t* data, std::size_t size)
{
    while (size) {
        DWORD got = 0;
        if (!ReadFile(m_handle, data, DWORD(size), &got, nullptr) || !got)
            return false;
        data += got;
        size -= got;
    }
    return true;
}

Pipe make_pipe(bool read_inheritable, bool write_inheritable)
{
    SECURITY_ATTRIBUTES sa { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
        throw std::runtime_error("CreatePipe failed");
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT,
        read_inheritable ? HANDLE_FLAG_INHERIT : 0);
    SetHandleInformation(writeEnd, HANDLE_FLAG_INHERIT,
        write_inheritable ? HANDLE_FLAG_INHERIT : 0);
    return { PipeEnd(readEnd), PipeEnd(writeEnd) };
}

}
