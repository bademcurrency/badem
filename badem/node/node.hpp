#pragma once

#include <badem/lib/work.hpp>
#include <badem/node/blockprocessor.hpp>
#include <badem/node/bootstrap.hpp>
#include <badem/node/logging.hpp>
#include <badem/node/nodeconfig.hpp>
#include <badem/node/peers.hpp>
#include <badem/node/portmapping.hpp>
#include <badem/node/signatures.hpp>
#include <badem/node/stats.hpp>
#include <badem/node/wallet.hpp>
#include <badem/secure/ledger.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <queue>

#include <boost/asio/thread_pool.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/thread.hpp>

#define xstr(a) ver_str (a)
#define ver_str(a) #a

/**
* Returns build version information
*/
static const char * BADEM_MAJOR_MINOR_VERSION = xstr (BADEM_VERSION_MAJOR) "." xstr (BADEM_VERSION_MINOR);
static const char * BADEM_MAJOR_MINOR_RC_VERSION = xstr (BADEM_VERSION_MAJOR) "." xstr (BADEM_VERSION_MINOR) "RC" xstr (BADEM_VERSION_PATCH);

namespace badem
{
class node;
class election_status
{
public:
	std::shared_ptr<badem::block> winner;
	badem::amount tally;
	std::chrono::milliseconds election_end;
	std::chrono::milliseconds election_duration;
};
class vote_info
{
public:
	std::chrono::steady_clock::time_point time;
	uint64_t sequence;
	badem::block_hash hash;
};
class election_vote_result
{
public:
	election_vote_result ();
	election_vote_result (bool, bool);
	bool replay;
	bool processed;
};
class election : public std::enable_shared_from_this<badem::election>
{
	std::function<void(std::shared_ptr<badem::block>)> confirmation_action;
	void confirm_once (badem::transaction const &, bool = false);
	void confirm_back (badem::transaction const &);

public:
	election (badem::node &, std::shared_ptr<badem::block>, std::function<void(std::shared_ptr<badem::block>)> const &);
	badem::election_vote_result vote (badem::account, uint64_t, badem::block_hash);
	badem::tally_t tally (badem::transaction const &);
	// Check if we have vote quorum
	bool have_quorum (badem::tally_t const &, badem::uint128_t);
	// Change our winner to agree with the network
	void compute_rep_votes (badem::transaction const &);
	// Confirm this block if quorum is met
	void confirm_if_quorum (badem::transaction const &);
	void log_votes (badem::tally_t const &);
	bool publish (std::shared_ptr<badem::block> block_a);
	size_t last_votes_size ();
	void stop ();
	badem::node & node;
	std::unordered_map<badem::account, badem::vote_info> last_votes;
	std::unordered_map<badem::block_hash, std::shared_ptr<badem::block>> blocks;
	std::chrono::steady_clock::time_point election_start;
	badem::election_status status;
	std::atomic<bool> confirmed;
	bool stopped;
	std::unordered_map<badem::block_hash, badem::uint128_t> last_tally;
	unsigned announcements;
};
class conflict_info
{
public:
	badem::uint512_union root;
	uint64_t difficulty;
	std::shared_ptr<badem::election> election;
};
// Core class for determining consensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions
{
public:
	active_transactions (badem::node &);
	~active_transactions ();
	// Start an election for a block
	// Call action with confirmed block, may be different than what we started with
	// clang-format off
	bool start (std::shared_ptr<badem::block>, std::function<void(std::shared_ptr<badem::block>)> const & = [](std::shared_ptr<badem::block>) {});
	// clang-format on
	// If this returns true, the vote is a replay
	// If this returns false, the vote may or may not be a replay
	bool vote (std::shared_ptr<badem::vote>, bool = false);
	// Is the root of this block in the roots container
	bool active (badem::block const &);
	void update_difficulty (badem::block const &);
	std::deque<std::shared_ptr<badem::block>> list_blocks (bool = false);
	void erase (badem::block const &);
	bool empty ();
	size_t size ();
	void stop ();
	bool publish (std::shared_ptr<badem::block> block_a);
	boost::multi_index_container<
	badem::conflict_info,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<
	boost::multi_index::member<badem::conflict_info, badem::uint512_union, &badem::conflict_info::root>>,
	boost::multi_index::ordered_non_unique<
	boost::multi_index::member<badem::conflict_info, uint64_t, &badem::conflict_info::difficulty>,
	std::greater<uint64_t>>>>
	roots;
	std::unordered_map<badem::block_hash, std::shared_ptr<badem::election>> blocks;
	std::deque<badem::election_status> list_confirmed ();
	std::deque<badem::election_status> confirmed;
	badem::node & node;
	std::mutex mutex;
	// Maximum number of conflicts to vote on per interval, lowest root hash first
	static unsigned constexpr announcements_per_interval = 32;
	// Minimum number of block announcements
	static unsigned constexpr announcement_min = 2;
	// Threshold to start logging blocks haven't yet been confirmed
	static unsigned constexpr announcement_long = 20;
	static unsigned constexpr request_interval_ms = badem::is_test_network ? 10 : 16000;
	static size_t constexpr election_history_size = 2048;
	static size_t constexpr max_broadcast_queue = 1000;

private:
	// Call action with confirmed block, may be different than what we started with
	// clang-format off
	bool add (std::shared_ptr<badem::block>, std::function<void(std::shared_ptr<badem::block>)> const & = [](std::shared_ptr<badem::block>) {});
	// clang-format on
	void request_loop ();
	void request_confirm (std::unique_lock<std::mutex> &);
	std::condition_variable condition;
	bool started;
	bool stopped;
	boost::thread thread;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (active_transactions & active_transactions, const std::string & name);

class operation
{
public:
	bool operator> (badem::operation const &) const;
	std::chrono::steady_clock::time_point wakeup;
	std::function<void()> function;
};
class alarm
{
public:
	alarm (boost::asio::io_context &);
	~alarm ();
	void add (std::chrono::steady_clock::time_point const &, std::function<void()> const &);
	void run ();
	boost::asio::io_context & io_ctx;
	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<operation, std::vector<operation>, std::greater<operation>> operations;
	boost::thread thread;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (alarm & alarm, const std::string & name);

class gap_information
{
public:
	std::chrono::steady_clock::time_point arrival;
	badem::block_hash hash;
	std::unordered_set<badem::account> voters;
};
class gap_cache
{
public:
	gap_cache (badem::node &);
	void add (badem::transaction const &, badem::block_hash const &, std::chrono::steady_clock::time_point = std::chrono::steady_clock::now ());
	void vote (std::shared_ptr<badem::vote>);
	badem::uint128_t bootstrap_threshold (badem::transaction const &);
	size_t size ();
	boost::multi_index_container<
	badem::gap_information,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<gap_information, badem::block_hash, &gap_information::hash>>>>
	blocks;
	size_t const max = 256;
	std::mutex mutex;
	badem::node & node;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (gap_cache & gap_cache, const std::string & name);

class work_pool;
class send_info
{
public:
	uint8_t const * data;
	size_t size;
	badem::endpoint endpoint;
	std::function<void(boost::system::error_code const &, size_t)> callback;
};
class block_arrival_info
{
public:
	std::chrono::steady_clock::time_point arrival;
	badem::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival
{
public:
	// Return `true' to indicated an error if the block has already been inserted
	bool add (badem::block_hash const &);
	bool recent (badem::block_hash const &);
	boost::multi_index_container<
	badem::block_arrival_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<badem::block_arrival_info, std::chrono::steady_clock::time_point, &badem::block_arrival_info::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<badem::block_arrival_info, badem::block_hash, &badem::block_arrival_info::hash>>>>
	arrival;
	std::mutex mutex;
	static size_t constexpr arrival_size_min = 8 * 1024;
	static std::chrono::seconds constexpr arrival_time_min = std::chrono::seconds (300);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_arrival & block_arrival, const std::string & name);

class online_reps
{
public:
	online_reps (badem::ledger &, badem::uint128_t);
	void observe (badem::account const &);
	void sample ();
	badem::uint128_t online_stake ();
	std::vector<badem::account> list ();
	static uint64_t constexpr weight_period = 5 * 60; // 5 minutes
	// The maximum amount of samples for a 2 week period on live or 3 days on beta
	static uint64_t constexpr weight_samples = badem::is_live_network ? 4032 : 864;

private:
	badem::uint128_t trend (badem::transaction &);
	std::mutex mutex;
	badem::ledger & ledger;
	std::unordered_set<badem::account> reps;
	badem::uint128_t online;
	badem::uint128_t minimum;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (online_reps & online_reps, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (online_reps & online_reps, const std::string & name);

class udp_data
{
public:
	uint8_t * buffer;
	size_t size;
	badem::endpoint endpoint;
};
/**
  * A circular buffer for servicing UDP datagrams. This container follows a producer/consumer model where the operating system is producing data in to buffers which are serviced by internal threads.
  * If buffers are not serviced fast enough they're internally dropped.
  * This container has a maximum space to hold N buffers of M size and will allocate them in round-robin order.
  * All public methods are thread-safe
*/
class udp_buffer
{
public:
	// Stats - Statistics
	// Size - Size of each individual buffer
	// Count - Number of buffers to allocate
	udp_buffer (badem::stat & stats, size_t, size_t);
	// Return a buffer where UDP data can be put
	// Method will attempt to return the first free buffer
	// If there are no free buffers, an unserviced buffer will be dequeued and returned
	// Function will block if there are no free or unserviced buffers
	// Return nullptr if the container has stopped
	badem::udp_data * allocate ();
	// Queue a buffer that has been filled with UDP data and notify servicing threads
	void enqueue (badem::udp_data *);
	// Return a buffer that has been filled with UDP data
	// Function will block until a buffer has been added
	// Return nullptr if the container has stopped
	badem::udp_data * dequeue ();
	// Return a buffer to the freelist after is has been serviced
	void release (badem::udp_data *);
	// Stop container and notify waiting threads
	void stop ();

private:
	badem::stat & stats;
	std::mutex mutex;
	std::condition_variable condition;
	boost::circular_buffer<badem::udp_data *> free;
	boost::circular_buffer<badem::udp_data *> full;
	std::vector<uint8_t> slab;
	std::vector<badem::udp_data> entries;
	bool stopped;
};
class network
{
public:
	network (badem::node &, uint16_t);
	~network ();
	void receive ();
	void process_packets ();
	void start ();
	void stop ();
	void receive_action (badem::udp_data *, badem::endpoint const &);
	void rpc_action (boost::system::error_code const &, size_t);
	void republish_vote (std::shared_ptr<badem::vote>);
	void republish_block (std::shared_ptr<badem::block>);
	void republish_block (std::shared_ptr<badem::block>, badem::endpoint const &);
	static unsigned const broadcast_interval_ms = 10;
	void republish_block_batch (std::deque<std::shared_ptr<badem::block>>, unsigned = broadcast_interval_ms);
	void republish (badem::block_hash const &, std::shared_ptr<std::vector<uint8_t>>, badem::endpoint);
	void confirm_send (badem::confirm_ack const &, std::shared_ptr<std::vector<uint8_t>>, badem::endpoint const &);
	void merge_peers (std::array<badem::endpoint, 8> const &);
	void send_keepalive (badem::endpoint const &);
	void send_node_id_handshake (badem::endpoint const &, boost::optional<badem::uint256_union> const & query, boost::optional<badem::uint256_union> const & respond_to);
	void broadcast_confirm_req (std::shared_ptr<badem::block>);
	void broadcast_confirm_req_base (std::shared_ptr<badem::block>, std::shared_ptr<std::vector<badem::peer_information>>, unsigned, bool = false);
	void broadcast_confirm_req_batch (std::unordered_map<badem::endpoint, std::vector<std::pair<badem::block_hash, badem::block_hash>>>, unsigned = broadcast_interval_ms, bool = false);
	void broadcast_confirm_req_batch (std::deque<std::pair<std::shared_ptr<badem::block>, std::shared_ptr<std::vector<badem::peer_information>>>>, unsigned = broadcast_interval_ms);
	void send_confirm_req (badem::endpoint const &, std::shared_ptr<badem::block>);
	void send_confirm_req_hashes (badem::endpoint const &, std::vector<std::pair<badem::block_hash, badem::block_hash>> const &);
	void confirm_hashes (badem::transaction const &, badem::endpoint const &, std::vector<badem::block_hash>);
	bool send_votes_cache (badem::block_hash const &, badem::endpoint const &);
	void send_buffer (uint8_t const *, size_t, badem::endpoint const &, std::function<void(boost::system::error_code const &, size_t)>);
	badem::endpoint endpoint ();
	badem::udp_buffer buffer_container;
	boost::asio::ip::udp::socket socket;
	std::mutex socket_mutex;
	boost::asio::ip::udp::resolver resolver;
	std::vector<boost::thread> packet_processing_threads;
	badem::node & node;
	std::atomic<bool> on;
	static uint16_t const node_port = badem::is_live_network ? 2224 : 54000;
	static size_t const buffer_size = 512;
	static size_t const confirm_req_hashes_max = 6;
};

class node_init
{
public:
	node_init ();
	bool error ();
	bool block_store_init;
	bool wallets_store_init;
	bool wallet_init;
};
class node_observers
{
public:
	badem::observer_set<std::shared_ptr<badem::block>, badem::account const &, badem::uint128_t const &, bool> blocks;
	badem::observer_set<bool> wallet;
	badem::observer_set<badem::transaction const &, std::shared_ptr<badem::vote>, badem::endpoint const &> vote;
	badem::observer_set<badem::account const &, bool> account_balance;
	badem::observer_set<badem::endpoint const &> endpoint;
	badem::observer_set<> disconnect;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (node_observers & node_observers, const std::string & name);

class vote_processor
{
public:
	vote_processor (badem::node &);
	void vote (std::shared_ptr<badem::vote>, badem::endpoint);
	// node.active.mutex lock required
	badem::vote_code vote_blocking (badem::transaction const &, std::shared_ptr<badem::vote>, badem::endpoint, bool = false);
	void verify_votes (std::deque<std::pair<std::shared_ptr<badem::vote>, badem::endpoint>> &);
	void flush ();
	void calculate_weights ();
	badem::node & node;
	void stop ();

private:
	void process_loop ();
	std::deque<std::pair<std::shared_ptr<badem::vote>, badem::endpoint>> votes;
	// Representatives levels for random early detection
	std::unordered_set<badem::account> representatives_1;
	std::unordered_set<badem::account> representatives_2;
	std::unordered_set<badem::account> representatives_3;
	std::condition_variable condition;
	std::mutex mutex;
	bool started;
	bool stopped;
	bool active;
	boost::thread thread;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_processor & vote_processor, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_processor & vote_processor, const std::string & name);

// The network is crawled for representatives by occasionally sending a unicast confirm_req for a specific block and watching to see if it's acknowledged with a vote.
class rep_crawler
{
public:
	void add (badem::block_hash const &);
	void remove (badem::block_hash const &);
	bool exists (badem::block_hash const &);
	std::mutex mutex;
	std::unordered_set<badem::block_hash> active;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (rep_crawler & rep_crawler, const std::string & name);
std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_processor & block_processor, const std::string & name);

class node : public std::enable_shared_from_this<badem::node>
{
public:
	node (badem::node_init &, boost::asio::io_context &, uint16_t, boost::filesystem::path const &, badem::alarm &, badem::logging const &, badem::work_pool &);
	node (badem::node_init &, boost::asio::io_context &, boost::filesystem::path const &, badem::alarm &, badem::node_config const &, badem::work_pool &, badem::node_flags = badem::node_flags ());
	~node ();
	template <typename T>
	void background (T action_a)
	{
		alarm.io_ctx.post (action_a);
	}
	void send_keepalive (badem::endpoint const &);
	bool copy_with_compaction (boost::filesystem::path const &);
	void keepalive (std::string const &, uint16_t, bool = false);
	void start ();
	void stop ();
	std::shared_ptr<badem::node> shared ();
	int store_version ();
	void process_confirmed (std::shared_ptr<badem::block>, uint8_t = 0);
	void process_message (badem::message &, badem::endpoint const &);
	void process_active (std::shared_ptr<badem::block>);
	badem::process_return process (badem::block const &);
	void keepalive_preconfigured (std::vector<std::string> const &);
	badem::block_hash latest (badem::account const &);
	badem::uint128_t balance (badem::account const &);
	std::shared_ptr<badem::block> block (badem::block_hash const &);
	std::pair<badem::uint128_t, badem::uint128_t> balance_pending (badem::account const &);
	badem::uint128_t weight (badem::account const &);
	badem::account representative (badem::account const &);
	void ongoing_keepalive ();
	void ongoing_syn_cookie_cleanup ();
	void ongoing_rep_crawl ();
	void ongoing_rep_calculation ();
	void ongoing_bootstrap ();
	void ongoing_store_flush ();
	void ongoing_peer_store ();
	void ongoing_unchecked_cleanup ();
	void backup_wallet ();
	void search_pending ();
	void bootstrap_wallet ();
	void unchecked_cleanup ();
	int price (badem::uint128_t const &, int);
	void work_generate_blocking (badem::block &, uint64_t = badem::work_pool::publish_threshold);
	uint64_t work_generate_blocking (badem::uint256_union const &, uint64_t = badem::work_pool::publish_threshold);
	void work_generate (badem::uint256_union const &, std::function<void(uint64_t)>, uint64_t = badem::work_pool::publish_threshold);
	void add_initial_peers ();
	void block_confirm (std::shared_ptr<badem::block>);
	void process_fork (badem::transaction const &, std::shared_ptr<badem::block>);
	bool validate_block_by_previous (badem::transaction const &, std::shared_ptr<badem::block>);
	void do_rpc_callback (boost::asio::ip::tcp::resolver::iterator i_a, std::string const &, uint16_t, std::shared_ptr<std::string>, std::shared_ptr<std::string>, std::shared_ptr<boost::asio::ip::tcp::resolver>);
	badem::uint128_t delta ();
	void ongoing_online_weight_calculation ();
	void ongoing_online_weight_calculation_queue ();
	boost::asio::io_context & io_ctx;
	badem::node_config config;
	badem::node_flags flags;
	badem::alarm & alarm;
	badem::work_pool & work;
	boost::log::sources::logger_mt log;
	std::unique_ptr<badem::block_store> store_impl;
	badem::block_store & store;
	std::unique_ptr<badem::wallets_store> wallets_store_impl;
	badem::wallets_store & wallets_store;
	badem::gap_cache gap_cache;
	badem::ledger ledger;
	badem::active_transactions active;
	badem::network network;
	badem::bootstrap_initiator bootstrap_initiator;
	badem::bootstrap_listener bootstrap;
	badem::peer_container peers;
	boost::filesystem::path application_path;
	badem::node_observers observers;
	badem::wallets wallets;
	badem::port_mapping port_mapping;
	badem::signature_checker checker;
	badem::vote_processor vote_processor;
	badem::rep_crawler rep_crawler;
	unsigned warmed_up;
	badem::block_processor block_processor;
	boost::thread block_processor_thread;
	badem::block_arrival block_arrival;
	badem::online_reps online_reps;
	badem::votes_cache votes_cache;
	badem::stat stats;
	badem::keypair node_id;
	badem::block_uniquer block_uniquer;
	badem::vote_uniquer vote_uniquer;
	const std::chrono::steady_clock::time_point startup_time;
	static double constexpr price_max = 16.0;
	static double constexpr free_cutoff = 1024.0;
	static std::chrono::seconds constexpr period = badem::is_test_network ? std::chrono::seconds (1) : std::chrono::seconds (60);
	static std::chrono::seconds constexpr cutoff = period * 5;
	static std::chrono::seconds constexpr syn_cookie_cutoff = std::chrono::seconds (5);
	static std::chrono::minutes constexpr backup_interval = std::chrono::minutes (5);
	static std::chrono::seconds constexpr search_pending_interval = badem::is_test_network ? std::chrono::seconds (1) : std::chrono::seconds (5 * 60);
	static std::chrono::seconds constexpr peer_interval = search_pending_interval;
	static std::chrono::hours constexpr unchecked_cleanup_interval = std::chrono::hours (1);
	static std::chrono::milliseconds constexpr process_confirmed_interval = badem::is_test_network ? std::chrono::milliseconds (50) : std::chrono::milliseconds (500);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (node & node, const std::string & name);

class thread_runner
{
public:
	thread_runner (boost::asio::io_context &, unsigned);
	~thread_runner ();
	void join ();
	std::vector<boost::thread> threads;
};
class inactive_node
{
public:
	inactive_node (boost::filesystem::path const & path = badem::working_path (), uint16_t = 24000);
	~inactive_node ();
	boost::filesystem::path path;
	std::shared_ptr<boost::asio::io_context> io_context;
	badem::alarm alarm;
	badem::logging logging;
	badem::node_init init;
	badem::work_pool work;
	uint16_t peering_port;
	std::shared_ptr<badem::node> node;
};
}
