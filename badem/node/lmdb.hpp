#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <lmdb/libraries/liblmdb/lmdb.h>

#include <badem/lib/numbers.hpp>
#include <badem/node/logging.hpp>
#include <badem/secure/blockstore.hpp>
#include <badem/secure/common.hpp>

#include <thread>

namespace badem
{
class mdb_env;
class mdb_txn : public transaction_impl
{
public:
	mdb_txn (badem::mdb_env const &, bool = false);
	mdb_txn (badem::mdb_txn const &) = delete;
	mdb_txn (badem::mdb_txn &&) = default;
	~mdb_txn ();
	badem::mdb_txn & operator= (badem::mdb_txn const &) = delete;
	badem::mdb_txn & operator= (badem::mdb_txn &&) = default;
	operator MDB_txn * () const;
	MDB_txn * handle;
};
/**
 * RAII wrapper for MDB_env
 */
class mdb_env
{
public:
	mdb_env (bool &, boost::filesystem::path const &, int max_dbs = 128, size_t map_size = 128ULL * 1024 * 1024 * 1024);
	~mdb_env ();
	operator MDB_env * () const;
	badem::transaction tx_begin (bool = false) const;
	MDB_txn * tx (badem::transaction const &) const;
	MDB_env * environment;
};

/**
 * Encapsulates MDB_val and provides uint256_union conversion of the data.
 */
class mdb_val
{
public:
	mdb_val (badem::epoch = badem::epoch::unspecified);
	mdb_val (badem::account_info const &);
	mdb_val (badem::block_info const &);
	mdb_val (MDB_val const &, badem::epoch = badem::epoch::unspecified);
	mdb_val (badem::pending_info const &);
	mdb_val (badem::pending_key const &);
	mdb_val (badem::unchecked_info const &);
	mdb_val (size_t, void *);
	mdb_val (badem::uint128_union const &);
	mdb_val (badem::uint256_union const &);
	mdb_val (badem::endpoint_key const &);
	mdb_val (std::shared_ptr<badem::block> const &);
	mdb_val (std::shared_ptr<badem::vote> const &);
	mdb_val (uint64_t);
	void * data () const;
	size_t size () const;
	explicit operator badem::account_info () const;
	explicit operator badem::block_info () const;
	explicit operator badem::pending_info () const;
	explicit operator badem::pending_key () const;
	explicit operator badem::unchecked_info () const;
	explicit operator badem::uint128_union () const;
	explicit operator badem::uint256_union () const;
	explicit operator std::array<char, 64> () const;
	explicit operator badem::endpoint_key () const;
	explicit operator badem::no_value () const;
	explicit operator std::shared_ptr<badem::block> () const;
	explicit operator std::shared_ptr<badem::send_block> () const;
	explicit operator std::shared_ptr<badem::receive_block> () const;
	explicit operator std::shared_ptr<badem::open_block> () const;
	explicit operator std::shared_ptr<badem::change_block> () const;
	explicit operator std::shared_ptr<badem::state_block> () const;
	explicit operator std::shared_ptr<badem::vote> () const;
	explicit operator uint64_t () const;
	operator MDB_val * () const;
	operator MDB_val const & () const;
	MDB_val value;
	std::shared_ptr<std::vector<uint8_t>> buffer;
	badem::epoch epoch{ badem::epoch::unspecified };
};
class block_store;

template <typename T, typename U>
class mdb_iterator : public store_iterator_impl<T, U>
{
public:
	mdb_iterator (badem::transaction const & transaction_a, MDB_dbi db_a, badem::epoch = badem::epoch::unspecified);
	mdb_iterator (std::nullptr_t, badem::epoch = badem::epoch::unspecified);
	mdb_iterator (badem::transaction const & transaction_a, MDB_dbi db_a, MDB_val const & val_a, badem::epoch = badem::epoch::unspecified);
	mdb_iterator (badem::mdb_iterator<T, U> && other_a);
	mdb_iterator (badem::mdb_iterator<T, U> const &) = delete;
	~mdb_iterator ();
	badem::store_iterator_impl<T, U> & operator++ () override;
	std::pair<badem::mdb_val, badem::mdb_val> * operator-> ();
	bool operator== (badem::store_iterator_impl<T, U> const & other_a) const override;
	bool is_end_sentinal () const override;
	void fill (std::pair<T, U> &) const override;
	void clear ();
	badem::mdb_iterator<T, U> & operator= (badem::mdb_iterator<T, U> && other_a);
	badem::store_iterator_impl<T, U> & operator= (badem::store_iterator_impl<T, U> const &) = delete;
	MDB_cursor * cursor;
	std::pair<badem::mdb_val, badem::mdb_val> current;

private:
	MDB_txn * tx (badem::transaction const &) const;
};

/**
 * Iterates the key/value pairs of two stores merged together
 */
template <typename T, typename U>
class mdb_merge_iterator : public store_iterator_impl<T, U>
{
public:
	mdb_merge_iterator (badem::transaction const &, MDB_dbi, MDB_dbi);
	mdb_merge_iterator (std::nullptr_t);
	mdb_merge_iterator (badem::transaction const &, MDB_dbi, MDB_dbi, MDB_val const &);
	mdb_merge_iterator (badem::mdb_merge_iterator<T, U> &&);
	mdb_merge_iterator (badem::mdb_merge_iterator<T, U> const &) = delete;
	~mdb_merge_iterator ();
	badem::store_iterator_impl<T, U> & operator++ () override;
	std::pair<badem::mdb_val, badem::mdb_val> * operator-> ();
	bool operator== (badem::store_iterator_impl<T, U> const &) const override;
	bool is_end_sentinal () const override;
	void fill (std::pair<T, U> &) const override;
	void clear ();
	badem::mdb_merge_iterator<T, U> & operator= (badem::mdb_merge_iterator<T, U> &&) = default;
	badem::mdb_merge_iterator<T, U> & operator= (badem::mdb_merge_iterator<T, U> const &) = delete;

private:
	badem::mdb_iterator<T, U> & least_iterator () const;
	std::unique_ptr<badem::mdb_iterator<T, U>> impl1;
	std::unique_ptr<badem::mdb_iterator<T, U>> impl2;
};

class logging;
/**
 * mdb implementation of the block store
 */
class mdb_store : public block_store
{
	friend class badem::block_predecessor_set;

public:
	mdb_store (bool &, badem::logging &, boost::filesystem::path const &, int lmdb_max_dbs = 128, bool drop_unchecked = false, size_t batch_size = 512);
	~mdb_store ();

	badem::transaction tx_begin_write () override;
	badem::transaction tx_begin_read () override;
	badem::transaction tx_begin (bool write = false) override;

	void initialize (badem::transaction const &, badem::genesis const &) override;
	void block_put (badem::transaction const &, badem::block_hash const &, badem::block const &, badem::block_sideband const &, badem::epoch version = badem::epoch::epoch_0) override;
	size_t block_successor_offset (badem::transaction const &, MDB_val, badem::block_type);
	badem::block_hash block_successor (badem::transaction const &, badem::block_hash const &) override;
	void block_successor_clear (badem::transaction const &, badem::block_hash const &) override;
	std::shared_ptr<badem::block> block_get (badem::transaction const &, badem::block_hash const &, badem::block_sideband * = nullptr) override;
	std::shared_ptr<badem::block> block_random (badem::transaction const &) override;
	void block_del (badem::transaction const &, badem::block_hash const &) override;
	bool block_exists (badem::transaction const &, badem::block_hash const &) override;
	bool block_exists (badem::transaction const &, badem::block_type, badem::block_hash const &) override;
	badem::block_counts block_count (badem::transaction const &) override;
	bool root_exists (badem::transaction const &, badem::uint256_union const &) override;
	bool source_exists (badem::transaction const &, badem::block_hash const &) override;
	badem::account block_account (badem::transaction const &, badem::block_hash const &) override;

	void frontier_put (badem::transaction const &, badem::block_hash const &, badem::account const &) override;
	badem::account frontier_get (badem::transaction const &, badem::block_hash const &) override;
	void frontier_del (badem::transaction const &, badem::block_hash const &) override;

	void account_put (badem::transaction const &, badem::account const &, badem::account_info const &) override;
	bool account_get (badem::transaction const &, badem::account const &, badem::account_info &) override;
	void account_del (badem::transaction const &, badem::account const &) override;
	bool account_exists (badem::transaction const &, badem::account const &) override;
	size_t account_count (badem::transaction const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_v0_begin (badem::transaction const &, badem::account const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_v0_begin (badem::transaction const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_v0_end () override;
	badem::store_iterator<badem::account, badem::account_info> latest_v1_begin (badem::transaction const &, badem::account const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_v1_begin (badem::transaction const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_v1_end () override;
	badem::store_iterator<badem::account, badem::account_info> latest_begin (badem::transaction const &, badem::account const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_begin (badem::transaction const &) override;
	badem::store_iterator<badem::account, badem::account_info> latest_end () override;

	void pending_put (badem::transaction const &, badem::pending_key const &, badem::pending_info const &) override;
	void pending_del (badem::transaction const &, badem::pending_key const &) override;
	bool pending_get (badem::transaction const &, badem::pending_key const &, badem::pending_info &) override;
	bool pending_exists (badem::transaction const &, badem::pending_key const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_begin (badem::transaction const &, badem::pending_key const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_begin (badem::transaction const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_end () override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_begin (badem::transaction const &, badem::pending_key const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_begin (badem::transaction const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_end () override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_begin (badem::transaction const &, badem::pending_key const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_begin (badem::transaction const &) override;
	badem::store_iterator<badem::pending_key, badem::pending_info> pending_end () override;

	bool block_info_get (badem::transaction const &, badem::block_hash const &, badem::block_info &) override;
	badem::uint128_t block_balance (badem::transaction const &, badem::block_hash const &) override;
	badem::epoch block_version (badem::transaction const &, badem::block_hash const &) override;

	badem::uint128_t representation_get (badem::transaction const &, badem::account const &) override;
	void representation_put (badem::transaction const &, badem::account const &, badem::uint128_t const &) override;
	void representation_add (badem::transaction const &, badem::account const &, badem::uint128_t const &) override;
	badem::store_iterator<badem::account, badem::uint128_union> representation_begin (badem::transaction const &) override;
	badem::store_iterator<badem::account, badem::uint128_union> representation_end () override;

	void unchecked_clear (badem::transaction const &) override;
	void unchecked_put (badem::transaction const &, badem::unchecked_key const &, badem::unchecked_info const &) override;
	void unchecked_put (badem::transaction const &, badem::block_hash const &, std::shared_ptr<badem::block> const &) override;
	std::vector<badem::unchecked_info> unchecked_get (badem::transaction const &, badem::block_hash const &) override;
	bool unchecked_exists (badem::transaction const &, badem::unchecked_key const &) override;
	void unchecked_del (badem::transaction const &, badem::unchecked_key const &) override;
	badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_begin (badem::transaction const &) override;
	badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_begin (badem::transaction const &, badem::unchecked_key const &) override;
	badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_end () override;
	size_t unchecked_count (badem::transaction const &) override;

	// Return latest vote for an account from store
	std::shared_ptr<badem::vote> vote_get (badem::transaction const &, badem::account const &) override;
	// Populate vote with the next sequence number
	std::shared_ptr<badem::vote> vote_generate (badem::transaction const &, badem::account const &, badem::raw_key const &, std::shared_ptr<badem::block>) override;
	std::shared_ptr<badem::vote> vote_generate (badem::transaction const &, badem::account const &, badem::raw_key const &, std::vector<badem::block_hash>) override;
	// Return either vote or the stored vote with a higher sequence number
	std::shared_ptr<badem::vote> vote_max (badem::transaction const &, std::shared_ptr<badem::vote>) override;
	// Return latest vote for an account considering the vote cache
	std::shared_ptr<badem::vote> vote_current (badem::transaction const &, badem::account const &) override;
	void flush (badem::transaction const &) override;
	badem::store_iterator<badem::account, std::shared_ptr<badem::vote>> vote_begin (badem::transaction const &) override;
	badem::store_iterator<badem::account, std::shared_ptr<badem::vote>> vote_end () override;

	void online_weight_put (badem::transaction const &, uint64_t, badem::amount const &) override;
	void online_weight_del (badem::transaction const &, uint64_t) override;
	badem::store_iterator<uint64_t, badem::amount> online_weight_begin (badem::transaction const &) override;
	badem::store_iterator<uint64_t, badem::amount> online_weight_end () override;
	size_t online_weight_count (badem::transaction const &) const override;
	void online_weight_clear (badem::transaction const &) override;

	std::mutex cache_mutex;
	std::unordered_map<badem::account, std::shared_ptr<badem::vote>> vote_cache_l1;
	std::unordered_map<badem::account, std::shared_ptr<badem::vote>> vote_cache_l2;

	void version_put (badem::transaction const &, int) override;
	int version_get (badem::transaction const &) override;
	void do_upgrades (badem::transaction const &, bool &);
	void upgrade_v1_to_v2 (badem::transaction const &);
	void upgrade_v2_to_v3 (badem::transaction const &);
	void upgrade_v3_to_v4 (badem::transaction const &);
	void upgrade_v4_to_v5 (badem::transaction const &);
	void upgrade_v5_to_v6 (badem::transaction const &);
	void upgrade_v6_to_v7 (badem::transaction const &);
	void upgrade_v7_to_v8 (badem::transaction const &);
	void upgrade_v8_to_v9 (badem::transaction const &);
	void upgrade_v9_to_v10 (badem::transaction const &);
	void upgrade_v10_to_v11 (badem::transaction const &);
	void upgrade_v11_to_v12 (badem::transaction const &);
	void do_slow_upgrades (size_t const);
	void upgrade_v12_to_v13 (size_t const);
	bool full_sideband (badem::transaction const &);

	// Requires a write transaction
	badem::raw_key get_node_id (badem::transaction const &) override;

	/** Deletes the node ID from the store */
	void delete_node_id (badem::transaction const &) override;

	void peer_put (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) override;
	bool peer_exists (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) const override;
	void peer_del (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) override;
	size_t peer_count (badem::transaction const & transaction_a) const override;
	void peer_clear (badem::transaction const & transaction_a) override;

	badem::store_iterator<badem::endpoint_key, badem::no_value> peers_begin (badem::transaction const & transaction_a) override;
	badem::store_iterator<badem::endpoint_key, badem::no_value> peers_end () override;

	void stop ();

	badem::logging & logging;

	badem::mdb_env env;

	/**
	 * Maps head block to owning account
	 * badem::block_hash -> badem::account
	 */
	MDB_dbi frontiers{ 0 };

	/**
	 * Maps account v1 to account information, head, rep, open, balance, timestamp and block count.
	 * badem::account -> badem::block_hash, badem::block_hash, badem::block_hash, badem::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v0{ 0 };

	/**
	 * Maps account v0 to account information, head, rep, open, balance, timestamp and block count.
	 * badem::account -> badem::block_hash, badem::block_hash, badem::block_hash, badem::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v1{ 0 };

	/**
	 * Maps block hash to send block.
	 * badem::block_hash -> badem::send_block
	 */
	MDB_dbi send_blocks{ 0 };

	/**
	 * Maps block hash to receive block.
	 * badem::block_hash -> badem::receive_block
	 */
	MDB_dbi receive_blocks{ 0 };

	/**
	 * Maps block hash to open block.
	 * badem::block_hash -> badem::open_block
	 */
	MDB_dbi open_blocks{ 0 };

	/**
	 * Maps block hash to change block.
	 * badem::block_hash -> badem::change_block
	 */
	MDB_dbi change_blocks{ 0 };

	/**
	 * Maps block hash to v0 state block.
	 * badem::block_hash -> badem::state_block
	 */
	MDB_dbi state_blocks_v0{ 0 };

	/**
	 * Maps block hash to v1 state block.
	 * badem::block_hash -> badem::state_block
	 */
	MDB_dbi state_blocks_v1{ 0 };

	/**
	 * Maps min_version 0 (destination account, pending block) to (source account, amount).
	 * badem::account, badem::block_hash -> badem::account, badem::amount
	 */
	MDB_dbi pending_v0{ 0 };

	/**
	 * Maps min_version 1 (destination account, pending block) to (source account, amount).
	 * badem::account, badem::block_hash -> badem::account, badem::amount
	 */
	MDB_dbi pending_v1{ 0 };

	/**
	 * Maps block hash to account and balance.
	 * block_hash -> badem::account, badem::amount
	 */
	MDB_dbi blocks_info{ 0 };

	/**
	 * Representative weights.
	 * badem::account -> badem::uint128_t
	 */
	MDB_dbi representation{ 0 };

	/**
	 * Unchecked bootstrap blocks info.
	 * badem::block_hash -> badem::unchecked_info
	 */
	MDB_dbi unchecked{ 0 };

	/**
	 * Highest vote observed for account.
	 * badem::account -> uint64_t
	 */
	MDB_dbi vote{ 0 };

	/**
	 * Samples of online vote weight
	 * uint64_t -> badem::amount
	 */
	MDB_dbi online_weight{ 0 };

	/**
	 * Meta information about block store, such as versions.
	 * badem::uint256_union (arbitrary key) -> blob
	 */
	MDB_dbi meta{ 0 };

	/*
	 * Endpoints for peers
	 * badem::endpoint_key -> no_value
	*/
	MDB_dbi peers{ 0 };

private:
	bool entry_has_sideband (MDB_val, badem::block_type);
	badem::account block_account_computed (badem::transaction const &, badem::block_hash const &);
	badem::uint128_t block_balance_computed (badem::transaction const &, badem::block_hash const &);
	MDB_dbi block_database (badem::block_type, badem::epoch);
	template <typename T>
	std::shared_ptr<badem::block> block_random (badem::transaction const &, MDB_dbi);
	MDB_val block_raw_get (badem::transaction const &, badem::block_hash const &, badem::block_type &);
	boost::optional<MDB_val> block_raw_get_by_type (badem::transaction const &, badem::block_hash const &, badem::block_type &);
	void block_raw_put (badem::transaction const &, MDB_dbi, badem::block_hash const &, MDB_val);
	void clear (MDB_dbi);
	std::atomic<bool> stopped{ false };
	std::thread upgrades;
};
class wallet_value
{
public:
	wallet_value () = default;
	wallet_value (badem::mdb_val const &);
	wallet_value (badem::uint256_union const &, uint64_t);
	badem::mdb_val val () const;
	badem::private_key key;
	uint64_t work;
};
}
