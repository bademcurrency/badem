#pragma once

#include <chrono>
#include <badem/lib/errors.hpp>
#include <badem/lib/jsonconfig.hpp>
#include <badem/lib/numbers.hpp>
#include <badem/node/ipc.hpp>
#include <badem/node/logging.hpp>
#include <badem/node/stats.hpp>
#include <vector>

namespace badem
{
/**
 * Node configuration
 */
class node_config
{
public:
	node_config ();
	node_config (uint16_t, badem::logging const &);
	badem::error serialize_json (badem::jsonconfig &) const;
	badem::error deserialize_json (bool &, badem::jsonconfig &);
	bool upgrade_json (unsigned, badem::jsonconfig &);
	badem::account random_representative ();
	uint16_t peering_port;
	badem::logging logging;
	std::vector<std::pair<std::string, uint16_t>> work_peers;
	std::vector<std::string> preconfigured_peers;
	std::vector<badem::account> preconfigured_representatives;
	unsigned bootstrap_fraction_numerator;
	badem::amount receive_minimum;
	badem::amount vote_minimum;
	badem::amount online_weight_minimum;
	unsigned online_weight_quorum;
	unsigned password_fanout;
	unsigned io_threads;
	unsigned network_threads;
	unsigned work_threads;
	unsigned signature_checker_threads;
	bool enable_voting;
	unsigned bootstrap_connections;
	unsigned bootstrap_connections_max;
	std::string callback_address;
	uint16_t callback_port;
	std::string callback_target;
	int lmdb_max_dbs;
	bool allow_local_peers;
	badem::stat_config stat_config;
	badem::ipc::ipc_config ipc_config;
	badem::uint256_union epoch_block_link;
	badem::account epoch_block_signer;
	std::chrono::milliseconds block_processor_batch_max_time;
	std::chrono::seconds unchecked_cutoff_time;
	static std::chrono::seconds constexpr keepalive_period = std::chrono::seconds (60);
	static std::chrono::seconds constexpr keepalive_cutoff = keepalive_period * 5;
	static std::chrono::minutes constexpr wallet_backup_interval = std::chrono::minutes (5);
	static int json_version ()
	{
		return 16;
	}
};

class node_flags
{
public:
	node_flags ();
	bool disable_backup;
	bool disable_lazy_bootstrap;
	bool disable_legacy_bootstrap;
	bool disable_wallet_bootstrap;
	bool disable_bootstrap_listener;
	bool disable_unchecked_cleanup;
	bool disable_unchecked_drop;
	bool fast_bootstrap;
	size_t sideband_batch_size;
};
}
