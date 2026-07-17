#include "domain/assets/balances.hpp"

#include <stdexcept>
#include <string>

#include "core/codec/abi.hpp"
#include "core/crypto/eth.hpp"

namespace izan::assets {

namespace {

    // Checksummed 0x form, or a throw — never let a typo'd address
    // reach the wire and come back as a plausible zero balance.
    std::string require_address(std::string_view address)
    {
        std::string checked = crypto::eth_checksum_address(address);
        if (checked.empty())
            throw std::invalid_argument(
                "not an address: " + std::string(address));
        return checked;
    }

}

units::U256 native_balance(chains::RpcClient& rpc, std::string_view address)
{
    const std::string addr = require_address(address);
    return units::U256::from_hex(
        rpc.call_string("eth_getBalance", "[\"" + addr + "\",\"latest\"]"));
}

units::U256 erc20_balance(
    chains::RpcClient& rpc, std::string_view token, std::string_view holder)
{
    const std::string contract = require_address(token);
    const std::string data = codec::CallData("balanceOf(address)")
                                 .add_address(require_address(holder))
                                 .to_hex();
    return codec::decode_u256(rpc.call_string("eth_call",
        "[{\"to\":\"" + contract + "\",\"data\":\"" + data
            + "\"},\"latest\"]"));
}

}
