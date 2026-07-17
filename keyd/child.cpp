#include "keyd/child.hpp"

#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/secure/secure_bytes.hpp"
#include "core/secure/vault.hpp"
#include "keyd/hardening.hpp"
#include "keyd/protocol.hpp"
#include "platform/ipc/secret_channel.hpp"

namespace izan::keyd {

namespace {

    using secure::SecureBytes;

    uint64_t token_arg(std::string_view arg)
    {
        uint64_t value = 0;
        std::from_chars(arg.data(), arg.data() + arg.size(), value);
        return value;
    }

    bool send_op(ipc::SecretChannel& ch, Op op, const uint8_t* body = nullptr,
        std::size_t size = 0)
    {
        std::vector<uint8_t> frame(1 + size);
        frame[0] = uint8_t(op);
        if (size)
            std::memcpy(frame.data() + 1, body, size);
        return ch.send(frame.data(), frame.size());
    }

    bool send_err(ipc::SecretChannel& ch, std::string_view reason)
    {
        std::vector<uint8_t> frame(1 + reason.size());
        frame[0] = uint8_t(Op::Err);
        std::memcpy(frame.data() + 1, reason.data(), reason.size());
        return ch.send(frame.data(), frame.size());
    }

}

int child_main(int argc, char** argv)
{
    // Before anything secret can exist in this process.
    const uint8_t hardened = apply_process_hardening();

    uint64_t inTok = 0, outTok = 0, keyTok = 0;
    std::string vaultPath;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with("--in="))
            inTok = token_arg(arg.substr(5));
        else if (arg.starts_with("--out="))
            outTok = token_arg(arg.substr(6));
        else if (arg.starts_with("--key="))
            keyTok = token_arg(arg.substr(6));
        else if (arg.starts_with("--vault="))
            vaultPath = std::string(arg.substr(8));
    }
    if (!inTok || !outTok || !keyTok || vaultPath.empty())
        return 2;

    // One-shot session key, then the delivery pipe dies.
    SecureBytes key(ipc::SecretChannel::kKeyBytes);
    {
        ipc::PipeEnd keyPipe = ipc::PipeEnd::from_token(keyTok);
        if (!keyPipe.read_all(key.data(), key.size()))
            return 2;
    }

    try {
        ipc::SecretChannel channel(ipc::PipeEnd::from_token(inTok),
            ipc::PipeEnd::from_token(outTok), key);
        key.reset();

        const uint8_t hello[2] = { kProtocolVersion, hardened };
        if (!send_op(channel, Op::Hello, hello, sizeof hello))
            return 0;

        std::optional<vault::Wallet> wallet;
        for (;;) {
            std::optional<SecureBytes> frame = channel.recv();
            if (!frame)
                break; // parent gone: fall through to wipe and exit
            if (frame->empty())
                continue;
            const Op op = Op(frame->data()[0]);
            switch (op) {
            case Op::Unlock: {
                SecureBytes pass(frame->size() - 1);
                if (!pass.empty())
                    std::memcpy(pass.data(), frame->data() + 1, pass.size());
                try {
                    wallet = vault::open(vaultPath, pass);
                    send_op(channel, Op::Ok);
                } catch (const std::exception& e) {
                    wallet.reset();
                    send_err(channel, e.what());
                }
                break;
            }
            case Op::Lock:
                wallet.reset();
                send_op(channel, Op::Ok);
                break;
            case Op::Status: {
                const uint8_t state = wallet.has_value() ? 1 : 0;
                send_op(channel, Op::State, &state, 1);
                break;
            }
            case Op::Shutdown:
                wallet.reset();
                send_op(channel, Op::Ok);
                return 0;
            default:
                send_err(channel, "unknown op");
            }
        }
    } catch (...) {
        // Tampered frame or transport failure: SecureBytes members
        // wipe on unwind; never limp along.
        return 3;
    }
    return 0;
}

}
