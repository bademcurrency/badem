#pragma once

#include <boost/thread/thread.hpp>
#include <badem/node/lmdb.hpp>
#include <badem/node/openclwork.hpp>
#include <badem/secure/blockstore.hpp>
#include <badem/secure/common.hpp>

#include <mutex>
#include <unordered_set>

namespace badem
{
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan
{
public:
	fan (badem::uint256_union const &, size_t);
	void value (badem::raw_key &);
	void value_set (badem::raw_key const &);
	std::vector<std::unique_ptr<badem::uint256_union>> values;

private:
	std::mutex mutex;
	void value_get (badem::raw_key &);
};
class node_config;
class kdf
{
public:
	void phs (badem::raw_key &, std::string const &, badem::uint256_union const &);
	std::mutex mutex;
};
enum class key_type
{
	not_a_type,
	unknown,
	adhoc,
	deterministic
};
class wallet_store
{
public:
	wallet_store (bool &, badem::kdf &, badem::transaction &, badem::account, unsigned, std::string const &);
	wallet_store (bool &, badem::kdf &, badem::transaction &, badem::account, unsigned, std::string const &, std::string const &);
	std::vector<badem::account> accounts (badem::transaction const &);
	void initialize (badem::transaction const &, bool &, std::string const &);
	badem::uint256_union check (badem::transaction const &);
	bool rekey (badem::transaction const &, std::string const &);
	bool valid_password (badem::transaction const &);
	bool attempt_password (badem::transaction const &, std::string const &);
	void wallet_key (badem::raw_key &, badem::transaction const &);
	void seed (badem::raw_key &, badem::transaction const &);
	void seed_set (badem::transaction const &, badem::raw_key const &);
	badem::key_type key_type (badem::wallet_value const &);
	badem::public_key deterministic_insert (badem::transaction const &);
	badem::public_key deterministic_insert (badem::transaction const &, uint32_t const);
	void deterministic_key (badem::raw_key &, badem::transaction const &, uint32_t);
	uint32_t deterministic_index_get (badem::transaction const &);
	void deterministic_index_set (badem::transaction const &, uint32_t);
	void deterministic_clear (badem::transaction const &);
	badem::uint256_union salt (badem::transaction const &);
	bool is_representative (badem::transaction const &);
	badem::account representative (badem::transaction const &);
	void representative_set (badem::transaction const &, badem::account const &);
	badem::public_key insert_adhoc (badem::transaction const &, badem::raw_key const &);
	void insert_watch (badem::transaction const &, badem::public_key const &);
	void erase (badem::transaction const &, badem::public_key const &);
	badem::wallet_value entry_get_raw (badem::transaction const &, badem::public_key const &);
	void entry_put_raw (badem::transaction const &, badem::public_key const &, badem::wallet_value const &);
	bool fetch (badem::transaction const &, badem::public_key const &, badem::raw_key &);
	bool exists (badem::transaction const &, badem::public_key const &);
	void destroy (badem::transaction const &);
	badem::store_iterator<badem::uint256_union, badem::wallet_value> find (badem::transaction const &, badem::uint256_union const &);
	badem::store_iterator<badem::uint256_union, badem::wallet_value> begin (badem::transaction const &, badem::uint256_union const &);
	badem::store_iterator<badem::uint256_union, badem::wallet_value> begin (badem::transaction const &);
	badem::store_iterator<badem::uint256_union, badem::wallet_value> end ();
	void derive_key (badem::raw_key &, badem::transaction const &, std::string const &);
	void serialize_json (badem::transaction const &, std::string &);
	void write_backup (badem::transaction const &, boost::filesystem::path const &);
	bool move (badem::transaction const &, badem::wallet_store &, std::vector<badem::public_key> const &);
	bool import (badem::transaction const &, badem::wallet_store &);
	bool work_get (badem::transaction const &, badem::public_key const &, uint64_t &);
	void work_put (badem::transaction const &, badem::public_key const &, uint64_t);
	unsigned version (badem::transaction const &);
	void version_put (badem::transaction const &, unsigned);
	void upgrade_v1_v2 (badem::transaction const &);
	void upgrade_v2_v3 (badem::transaction const &);
	void upgrade_v3_v4 (badem::transaction const &);
	badem::fan password;
	badem::fan wallet_key_mem;
	static unsigned const version_1 = 1;
	static unsigned const version_2 = 2;
	static unsigned const version_3 = 3;
	static unsigned const version_4 = 4;
	unsigned const version_current = version_4;
	static badem::uint256_union const version_special;
	static badem::uint256_union const wallet_key_special;
	static badem::uint256_union const salt_special;
	static badem::uint256_union const check_special;
	static badem::uint256_union const representative_special;
	static badem::uint256_union const seed_special;
	static badem::uint256_union const deterministic_index_special;
	static size_t const check_iv_index;
	static size_t const seed_iv_index;
	static int const special_count;
	static unsigned const kdf_full_work = 64 * 1024;
	static unsigned const kdf_test_work = 8;
	static unsigned const kdf_work = badem::is_test_network ? kdf_test_work : kdf_full_work;
	badem::kdf & kdf;
	MDB_dbi handle;
	std::recursive_mutex mutex;

private:
	MDB_txn * tx (badem::transaction const &) const;
};
class wallets;
// A wallet is a set of account keys encrypted by a common encryption key
class wallet : public std::enable_shared_from_this<badem::wallet>
{
public:
	std::shared_ptr<badem::block> change_action (badem::account const &, badem::account const &, uint64_t = 0, bool = true);
	std::shared_ptr<badem::block> receive_action (badem::block const &, badem::account const &, badem::uint128_union const &, uint64_t = 0, bool = true);
	std::shared_ptr<badem::block> send_action (badem::account const &, badem::account const &, badem::uint128_t const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	wallet (bool &, badem::transaction &, badem::wallets &, std::string const &);
	wallet (bool &, badem::transaction &, badem::wallets &, std::string const &, std::string const &);
	void enter_initial_password ();
	bool enter_password (badem::transaction const &, std::string const &);
	badem::public_key insert_adhoc (badem::raw_key const &, bool = true);
	badem::public_key insert_adhoc (badem::transaction const &, badem::raw_key const &, bool = true);
	void insert_watch (badem::transaction const &, badem::public_key const &);
	badem::public_key deterministic_insert (badem::transaction const &, bool = true);
	badem::public_key deterministic_insert (uint32_t, bool = true);
	badem::public_key deterministic_insert (bool = true);
	bool exists (badem::public_key const &);
	bool import (std::string const &, std::string const &);
	void serialize (std::string &);
	bool change_sync (badem::account const &, badem::account const &);
	void change_async (badem::account const &, badem::account const &, std::function<void(std::shared_ptr<badem::block>)> const &, uint64_t = 0, bool = true);
	bool receive_sync (std::shared_ptr<badem::block>, badem::account const &, badem::uint128_t const &);
	void receive_async (std::shared_ptr<badem::block>, badem::account const &, badem::uint128_t const &, std::function<void(std::shared_ptr<badem::block>)> const &, uint64_t = 0, bool = true);
	badem::block_hash send_sync (badem::account const &, badem::account const &, badem::uint128_t const &);
	void send_async (badem::account const &, badem::account const &, badem::uint128_t const &, std::function<void(std::shared_ptr<badem::block>)> const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	void work_apply (badem::account const &, std::function<void(uint64_t)>);
	void work_cache_blocking (badem::account const &, badem::block_hash const &);
	void work_update (badem::transaction const &, badem::account const &, badem::block_hash const &, uint64_t);
	void work_ensure (badem::account const &, badem::block_hash const &);
	bool search_pending ();
	void init_free_accounts (badem::transaction const &);
	uint32_t deterministic_check (badem::transaction const & transaction_a, uint32_t index);
	/** Changes the wallet seed and returns the first account */
	badem::public_key change_seed (badem::transaction const & transaction_a, badem::raw_key const & prv_a, uint32_t count = 0);
	void deterministic_restore (badem::transaction const & transaction_a);
	bool live ();
	std::unordered_set<badem::account> free_accounts;
	std::function<void(bool, bool)> lock_observer;
	badem::wallet_store store;
	badem::wallets & wallets;
	std::mutex representatives_mutex;
	std::unordered_set<badem::account> representatives;
};
class node;

/**
 * The wallets set is all the wallets a node controls.
 * A node may contain multiple wallets independently encrypted and operated.
 */
class wallets
{
public:
	wallets (bool &, badem::node &);
	~wallets ();
	std::shared_ptr<badem::wallet> open (badem::uint256_union const &);
	std::shared_ptr<badem::wallet> create (badem::uint256_union const &);
	bool search_pending (badem::uint256_union const &);
	void search_pending_all ();
	void destroy (badem::uint256_union const &);
	void reload ();
	void do_wallet_actions ();
	void queue_wallet_action (badem::uint128_t const &, std::shared_ptr<badem::wallet>, std::function<void(badem::wallet &)> const &);
	void foreach_representative (badem::transaction const &, std::function<void(badem::public_key const &, badem::raw_key const &)> const &);
	bool exists (badem::transaction const &, badem::public_key const &);
	void stop ();
	void clear_send_ids (badem::transaction const &);
	void compute_reps ();
	void ongoing_compute_reps ();
	void split_if_needed (badem::transaction &, badem::block_store &);
	void move_table (std::string const &, MDB_txn *, MDB_txn *);
	std::function<void(bool)> observer;
	std::unordered_map<badem::uint256_union, std::shared_ptr<badem::wallet>> items;
	std::multimap<badem::uint128_t, std::pair<std::shared_ptr<badem::wallet>, std::function<void(badem::wallet &)>>, std::greater<badem::uint128_t>> actions;
	std::mutex mutex;
	std::mutex action_mutex;
	std::condition_variable condition;
	badem::kdf kdf;
	MDB_dbi handle;
	MDB_dbi send_action_ids;
	badem::node & node;
	badem::mdb_env & env;
	std::atomic<bool> stopped;
	boost::thread thread;
	static badem::uint128_t const generate_priority;
	static badem::uint128_t const high_priority;
	std::atomic<uint64_t> reps_count{ 0 };

	/** Start read-write transaction */
	badem::transaction tx_begin_write ();

	/** Start read-only transaction */
	badem::transaction tx_begin_read ();

	/**
	 * Start a read-only or read-write transaction
	 * @param write If true, start a read-write transaction
	 */
	badem::transaction tx_begin (bool write = false);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (wallets & wallets, const std::string & name);

class wallets_store
{
public:
	virtual ~wallets_store () = default;
};
class mdb_wallets_store : public wallets_store
{
public:
	mdb_wallets_store (bool &, boost::filesystem::path const &, int lmdb_max_dbs = 128);
	badem::mdb_env environment;
};
}
