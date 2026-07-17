#include "keyd/client.hpp"

#include <cstring>
#include <stdexcept>
#include <vector>

#include <sodium.h>

#include "keyd/protocol.hpp"
#include "platform/ipc/pipe.hpp"

namespace izan::keyd {

using secure::SecureBytes;

KeydClient KeydClient::spawn(
    const std::string& exe, const std::string& vault_path)
{
    // Child-facing ends inheritable, parent ends private.
    ipc::Pipe toChild = ipc::make_pipe(true, false);   // child reads
    ipc::Pipe fromChild = ipc::make_pipe(false, true); // child writes
    ipc::Pipe keyPipe = ipc::make_pipe(true, false);

    SecureBytes key(ipc::SecretChannel::kKeyBytes);
    randombytes_buf(key.data(), key.size());

    std::string cmd = "\"" + exe
        + "\" --keyd-child --in=" + std::to_string(toChild.read.token())
        + " --out=" + std::to_string(fromChild.write.token())
        + " --key=" + std::to_string(keyPipe.read.token()) + " --vault=\""
        + vault_path + "\"";

    void* inherit[3] = { toChild.read.handle(), fromChild.write.handle(),
        keyPipe.read.handle() };
    KeydClient client;
    client.m_child = proc::ChildProcess::spawn(cmd, inherit);

    // Drop the child-side ends in this process, hand over the key,
    // then forget it — the channel states are the only copies left.
    toChild.read.close();
    fromChild.write.close();
    keyPipe.read.close();
    if (!keyPipe.write.write_all(key.data(), key.size()))
        throw std::runtime_error("keyd: key handover failed");
    keyPipe.write.close();

    client.m_channel = std::make_unique<ipc::SecretChannel>(
        std::move(fromChild.read), std::move(toChild.write), key);
    key.reset();

    std::optional<SecureBytes> hello = client.m_channel->recv();
    if (!hello || hello->size() < 3 || hello->data()[0] != uint8_t(Op::Hello))
        throw std::runtime_error("keyd: no hello from child");
    client.m_hello = { hello->data()[1], hello->data()[2] };
    return client;
}

std::optional<SecureBytes> KeydClient::request(
    const uint8_t* frame, std::size_t size)
{
    if (!m_channel || !m_channel->send(frame, size))
        return std::nullopt;
    return m_channel->recv();
}

bool KeydClient::unlock(const SecureBytes& passphrase)
{
    std::vector<uint8_t> frame(1 + passphrase.size());
    frame[0] = uint8_t(Op::Unlock);
    if (!passphrase.empty())
        std::memcpy(frame.data() + 1, passphrase.data(), passphrase.size());
    std::optional<SecureBytes> reply = request(frame.data(), frame.size());
    sodium_memzero(frame.data(), frame.size());
    if (!reply || reply->empty()) {
        m_last_error = "channel broken";
        return false;
    }
    if (reply->data()[0] == uint8_t(Op::Ok))
        return true;
    m_last_error.assign(
        reinterpret_cast<const char*>(reply->data()) + 1, reply->size() - 1);
    return false;
}

bool KeydClient::lock()
{
    const uint8_t frame[1] = { uint8_t(Op::Lock) };
    std::optional<SecureBytes> reply = request(frame, 1);
    return reply && !reply->empty() && reply->data()[0] == uint8_t(Op::Ok);
}

std::optional<bool> KeydClient::unlocked()
{
    const uint8_t frame[1] = { uint8_t(Op::Status) };
    std::optional<SecureBytes> reply = request(frame, 1);
    if (!reply || reply->size() < 2 || reply->data()[0] != uint8_t(Op::State))
        return std::nullopt;
    return reply->data()[1] == 1;
}

bool KeydClient::shutdown()
{
    const uint8_t frame[1] = { uint8_t(Op::Shutdown) };
    std::optional<SecureBytes> reply = request(frame, 1);
    return reply && !reply->empty() && reply->data()[0] == uint8_t(Op::Ok);
}

std::optional<uint32_t> KeydClient::wait_exit(uint32_t timeout_ms)
{
    return m_child.wait(timeout_ms);
}

void KeydClient::drop_channel()
{
    m_channel.reset();
}

}
