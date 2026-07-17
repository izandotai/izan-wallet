#pragma once

#include <string_view>

#include "core/units/u256.hpp"
#include "domain/chains/rpc_client.hpp"

namespace izan::assets {

// Balance reads for watch-only mode. Addresses are validated (EIP-55
// checksum machinery, any input case) before anything touches the
// network; malformed input throws std::invalid_argument.

// Native coin balance in base units (wei and friends).
units::U256 native_balance(chains::RpcClient& rpc, std::string_view address);

// ERC-20 balanceOf(holder) on the token contract, in the token's base
// units. A non-contract token address surfaces as a decode error.
units::U256 erc20_balance(
    chains::RpcClient& rpc, std::string_view token, std::string_view holder);

}
