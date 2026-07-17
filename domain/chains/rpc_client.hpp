#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "domain/chains/chain_spec.hpp"

namespace izan::net {
class HttpsClient;
}

namespace izan::chains {

// JSON-RPC over a chain's endpoint list. Endpoints are tried in
// priority order and connections are kept alive per endpoint; a
// transport failure (connect, TLS, non-200, garbage body) moves on to
// the next endpoint, while a node-reported RpcError is an authoritative
// answer from a healthy node and propagates as-is — retrying a revert
// elsewhere would only get the same revert slower.
class RpcClient {
public:
    explicit RpcClient(ChainSpec spec);
    ~RpcClient();

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    const ChainSpec& spec() const
    {
        return m_spec;
    }

    // Raw JSON text of "result".
    std::string call(std::string_view method, std::string_view params_json);
    // String results ("0x1a" quantities), unquoted.
    std::string call_string(
        std::string_view method, std::string_view params_json);

private:
    std::string round_robin(std::string_view method,
        std::string_view params_json, bool string_result);

    ChainSpec m_spec;
    std::vector<std::unique_ptr<net::HttpsClient>> m_clients;
    uint64_t m_next_id = 1;
};

}
