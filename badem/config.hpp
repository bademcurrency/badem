#pragma once

#include <chrono>
#include <cstddef>

namespace rai
{
// Network variants with different genesis blocks and network parameters
enum class badem_networks
{
	// Low work parameters, publicly known genesis key, test IP ports
	badem_test_network,
	// Normal work parameters, secret beta genesis key, beta IP ports
	badem_beta_network,
	// Normal work parameters, secret live key, live IP ports
	badem_live_network
};
rai::badem_networks const badem_network = badem_networks::ACTIVE_NETWORK;
std::chrono::milliseconds const transaction_timeout = std::chrono::milliseconds (1000);
}
