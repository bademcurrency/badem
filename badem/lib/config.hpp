#pragma once

#include <chrono>
#include <cstddef>

namespace badem
{
/**
 * Network variants with different genesis blocks and network parameters
 * @warning Enum values are used for comparison; do not change.
 */
enum class badem_networks
{
	// Low work parameters, publicly known genesis key, test IP ports
	badem_test_network = 0,
	bdm_test_network = 0,
	// Normal work parameters, secret beta genesis key, beta IP ports
	badem_beta_network = 1,
	bdm_beta_network = 1,
	// Normal work parameters, secret live key, live IP ports
	badem_live_network = 2,
	bdm_live_network = 2,
};
badem::badem_networks constexpr badem_network = badem_networks::ACTIVE_NETWORK;
bool constexpr is_live_network = badem_network == badem_networks::badem_live_network;
bool constexpr is_beta_network = badem_network == badem_networks::badem_beta_network;
bool constexpr is_test_network = badem_network == badem_networks::badem_test_network;

std::chrono::milliseconds const transaction_timeout = std::chrono::milliseconds (1000);
}
