#include "domain/chains/rpc_client.hpp"

#include <stdexcept>

#include "domain/chains/jsonrpc.hpp"
#include "platform/net/http_client.hpp"

namespace izan::chains {

RpcClient::RpcClient(ChainSpec spec)
    : m_spec(std::move(spec))
{
    m_clients.resize(m_spec.rpc.size());
}

RpcClient::~RpcClient() = default;

std::string RpcClient::round_robin(
    std::string_view method, std::string_view params_json, bool string_result)
{
    const uint64_t id = m_next_id++;
    const std::string request = make_request(method, params_json, id);

    std::string failures;
    for (std::size_t i = 0; i < m_spec.rpc.size(); ++i) {
        const std::string& url = m_spec.rpc[i];
        try {
            // http:// endpoints are legal in a dev config but this
            // transport is TLS-only; parse_https_url refuses them and
            // the failure is recorded like any other.
            const net::HttpsUrl parsed = net::parse_https_url(url);
            if (!m_clients[i])
                m_clients[i] = std::make_unique<net::HttpsClient>(
                    parsed.host, parsed.port);
            const net::HttpResponse resp
                = m_clients[i]->post(parsed.target, request);
            if (resp.status != 200)
                throw std::runtime_error(
                    "http status " + std::to_string(resp.status));
            return string_result ? result_string(resp.body, id)
                                 : result_of(resp.body, id);
        } catch (const RpcError&) {
            throw;
        } catch (const std::exception& e) {
            failures += "  " + url + ": " + e.what() + "\n";
            m_clients[i].reset();
        }
    }
    throw std::runtime_error("rpc: chain " + std::to_string(m_spec.chain_id)
        + " all endpoints failed for " + std::string(method) + ":\n"
        + failures);
}

std::string RpcClient::call(
    std::string_view method, std::string_view params_json)
{
    return round_robin(method, params_json, false);
}

std::string RpcClient::call_string(
    std::string_view method, std::string_view params_json)
{
    return round_robin(method, params_json, true);
}

}
