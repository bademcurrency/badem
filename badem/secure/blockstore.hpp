#pragma once

#include <badem/secure/common.hpp>
#include <stack>

namespace badem
{
class block_sideband
{
public:
	block_sideband () = default;
	block_sideband (badem::block_type, badem::account const &, badem::block_hash const &, badem::amount const &, uint64_t, uint64_t);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	static size_t size (badem::block_type);
	badem::block_type type;
	badem::block_hash successor;
	badem::account account;
	badem::amount balance;
	uint64_t height;
	uint64_t timestamp;
};
class transaction;
class block_store;

/**
 * Summation visitor for blocks, supporting amount and balance computations. These
 * computations are mutually dependant. The natural solution is to use mutual recursion
 * between balance and amount visitors, but this leads to very deep stacks. Hence, the
 * summation visitor uses an iterative approach.
 */
class summation_visitor : public badem::block_visitor
{
	enum summation_type
	{
		invalid = 0,
		balance = 1,
		amount = 2
	};

	/** Represents an invocation frame */
	class frame
	{
	public:
		frame (summation_type type_a, badem::block_hash balance_hash_a, badem::block_hash amount_hash_a) :
		type (type_a), balance_hash (balance_hash_a), amount_hash (amount_hash_a)
		{
		}

		/** The summation type guides the block visitor handlers */
		summation_type type{ invalid };
		/** Accumulated balance or amount */
		badem::uint128_t sum{ 0 };
		/** The current balance hash */
		badem::block_hash balance_hash{ 0 };
		/** The current amount hash */
		badem::block_hash amount_hash{ 0 };
		/** If true, this frame is awaiting an invocation result */
		bool awaiting_result{ false };
		/** Set by the invoked frame, representing the return value */
		badem::uint128_t incoming_result{ 0 };
	};

public:
	summation_visitor (badem::transaction const &, badem::block_store &);
	virtual ~summation_visitor () = default;
	/** Computes the balance as of \p block_hash */
	badem::uint128_t compute_balance (badem::block_hash const & block_hash);
	/** Computes the amount delta between \p block_hash and its predecessor */
	badem::uint128_t compute_amount (badem::block_hash const & block_hash);

protected:
	badem::transaction const & transaction;
	badem::block_store & store;

	/** The final result */
	badem::uint128_t result{ 0 };
	/** The current invocation frame */
	frame * current{ nullptr };
	/** Invocation frames */
	std::stack<frame> frames;
	/** Push a copy of \p hash of the given summation \p type */
	badem::summation_visitor::frame push (badem::summation_visitor::summation_type type, badem::block_hash const & hash);
	void sum_add (badem::uint128_t addend_a);
	void sum_set (badem::uint128_t value_a);
	/** The epilogue yields the result to previous frame, if any */
	void epilogue ();

	badem::uint128_t compute_internal (badem::summation_visitor::summation_type type, badem::block_hash const &);
	void send_block (badem::send_block const &) override;
	void receive_block (badem::receive_block const &) override;
	void open_block (badem::open_block const &) override;
	void change_block (badem::change_block const &) override;
	void state_block (badem::state_block const &) override;
};

/**
 * Determine the representative for this block
 */
class representative_visitor : public badem::block_visitor
{
public:
	representative_visitor (badem::transaction const & transaction_a, badem::block_store & store_a);
	virtual ~representative_visitor () = default;
	void compute (badem::block_hash const & hash_a);
	void send_block (badem::send_block const & block_a) override;
	void receive_block (badem::receive_block const & block_a) override;
	void open_block (badem::open_block const & block_a) override;
	void change_block (badem::change_block const & block_a) override;
	void state_block (badem::state_block const & block_a) override;
	badem::transaction const & transaction;
	badem::block_store & store;
	badem::block_hash current;
	badem::block_hash result;
};
template <typename T, typename U>
class store_iterator_impl
{
public:
	virtual ~store_iterator_impl () = default;
	virtual badem::store_iterator_impl<T, U> & operator++ () = 0;
	virtual bool operator== (badem::store_iterator_impl<T, U> const & other_a) const = 0;
	virtual bool is_end_sentinal () const = 0;
	virtual void fill (std::pair<T, U> &) const = 0;
	badem::store_iterator_impl<T, U> & operator= (badem::store_iterator_impl<T, U> const &) = delete;
	bool operator== (badem::store_iterator_impl<T, U> const * other_a) const
	{
		return (other_a != nullptr && *this == *other_a) || (other_a == nullptr && is_end_sentinal ());
	}
	bool operator!= (badem::store_iterator_impl<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}
};
/**
 * Iterates the key/value pairs of a transaction
 */
template <typename T, typename U>
class store_iterator
{
public:
	store_iterator (std::nullptr_t)
	{
	}
	store_iterator (std::unique_ptr<badem::store_iterator_impl<T, U>> impl_a) :
	impl (std::move (impl_a))
	{
		impl->fill (current);
	}
	store_iterator (badem::store_iterator<T, U> && other_a) :
	current (std::move (other_a.current)),
	impl (std::move (other_a.impl))
	{
	}
	badem::store_iterator<T, U> & operator++ ()
	{
		++*impl;
		impl->fill (current);
		return *this;
	}
	badem::store_iterator<T, U> & operator= (badem::store_iterator<T, U> && other_a)
	{
		impl = std::move (other_a.impl);
		current = std::move (other_a.current);
		return *this;
	}
	badem::store_iterator<T, U> & operator= (badem::store_iterator<T, U> const &) = delete;
	std::pair<T, U> * operator-> ()
	{
		return &current;
	}
	bool operator== (badem::store_iterator<T, U> const & other_a) const
	{
		return (impl == nullptr && other_a.impl == nullptr) || (impl != nullptr && *impl == other_a.impl.get ()) || (other_a.impl != nullptr && *other_a.impl == impl.get ());
	}
	bool operator!= (badem::store_iterator<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}

private:
	std::pair<T, U> current;
	std::unique_ptr<badem::store_iterator_impl<T, U>> impl;
};

class block_predecessor_set;

class transaction_impl
{
public:
	virtual ~transaction_impl () = default;
};
/**
 * RAII wrapper of MDB_txn where the constructor starts the transaction
 * and the destructor commits it.
 */
class transaction
{
public:
	std::unique_ptr<badem::transaction_impl> impl;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	virtual ~block_store () = default;
	virtual void initialize (badem::transaction const &, badem::genesis const &) = 0;
	virtual void block_put (badem::transaction const &, badem::block_hash const &, badem::block const &, badem::block_sideband const &, badem::epoch version = badem::epoch::epoch_0) = 0;
	virtual badem::block_hash block_successor (badem::transaction const &, badem::block_hash const &) = 0;
	virtual void block_successor_clear (badem::transaction const &, badem::block_hash const &) = 0;
	virtual std::shared_ptr<badem::block> block_get (badem::transaction const &, badem::block_hash const &, badem::block_sideband * = nullptr) = 0;
	virtual std::shared_ptr<badem::block> block_random (badem::transaction const &) = 0;
	virtual void block_del (badem::transaction const &, badem::block_hash const &) = 0;
	virtual bool block_exists (badem::transaction const &, badem::block_hash const &) = 0;
	virtual bool block_exists (badem::transaction const &, badem::block_type, badem::block_hash const &) = 0;
	virtual badem::block_counts block_count (badem::transaction const &) = 0;
	virtual bool root_exists (badem::transaction const &, badem::uint256_union const &) = 0;
	virtual bool source_exists (badem::transaction const &, badem::block_hash const &) = 0;
	virtual badem::account block_account (badem::transaction const &, badem::block_hash const &) = 0;

	virtual void frontier_put (badem::transaction const &, badem::block_hash const &, badem::account const &) = 0;
	virtual badem::account frontier_get (badem::transaction const &, badem::block_hash const &) = 0;
	virtual void frontier_del (badem::transaction const &, badem::block_hash const &) = 0;

	virtual void account_put (badem::transaction const &, badem::account const &, badem::account_info const &) = 0;
	virtual bool account_get (badem::transaction const &, badem::account const &, badem::account_info &) = 0;
	virtual void account_del (badem::transaction const &, badem::account const &) = 0;
	virtual bool account_exists (badem::transaction const &, badem::account const &) = 0;
	virtual size_t account_count (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v0_begin (badem::transaction const &, badem::account const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v0_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v0_end () = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v1_begin (badem::transaction const &, badem::account const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v1_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_v1_end () = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_begin (badem::transaction const &, badem::account const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, badem::account_info> latest_end () = 0;

	virtual void pending_put (badem::transaction const &, badem::pending_key const &, badem::pending_info const &) = 0;
	virtual void pending_del (badem::transaction const &, badem::pending_key const &) = 0;
	virtual bool pending_get (badem::transaction const &, badem::pending_key const &, badem::pending_info &) = 0;
	virtual bool pending_exists (badem::transaction const &, badem::pending_key const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_begin (badem::transaction const &, badem::pending_key const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v0_end () = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_begin (badem::transaction const &, badem::pending_key const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_v1_end () = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_begin (badem::transaction const &, badem::pending_key const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::pending_key, badem::pending_info> pending_end () = 0;

	virtual bool block_info_get (badem::transaction const &, badem::block_hash const &, badem::block_info &) = 0;
	virtual badem::uint128_t block_balance (badem::transaction const &, badem::block_hash const &) = 0;
	virtual badem::epoch block_version (badem::transaction const &, badem::block_hash const &) = 0;

	virtual badem::uint128_t representation_get (badem::transaction const &, badem::account const &) = 0;
	virtual void representation_put (badem::transaction const &, badem::account const &, badem::uint128_t const &) = 0;
	virtual void representation_add (badem::transaction const &, badem::account const &, badem::uint128_t const &) = 0;
	virtual badem::store_iterator<badem::account, badem::uint128_union> representation_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, badem::uint128_union> representation_end () = 0;

	virtual void unchecked_clear (badem::transaction const &) = 0;
	virtual void unchecked_put (badem::transaction const &, badem::unchecked_key const &, badem::unchecked_info const &) = 0;
	virtual void unchecked_put (badem::transaction const &, badem::block_hash const &, std::shared_ptr<badem::block> const &) = 0;
	virtual std::vector<badem::unchecked_info> unchecked_get (badem::transaction const &, badem::block_hash const &) = 0;
	virtual bool unchecked_exists (badem::transaction const &, badem::unchecked_key const &) = 0;
	virtual void unchecked_del (badem::transaction const &, badem::unchecked_key const &) = 0;
	virtual badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_begin (badem::transaction const &, badem::unchecked_key const &) = 0;
	virtual badem::store_iterator<badem::unchecked_key, badem::unchecked_info> unchecked_end () = 0;
	virtual size_t unchecked_count (badem::transaction const &) = 0;

	// Return latest vote for an account from store
	virtual std::shared_ptr<badem::vote> vote_get (badem::transaction const &, badem::account const &) = 0;
	// Populate vote with the next sequence number
	virtual std::shared_ptr<badem::vote> vote_generate (badem::transaction const &, badem::account const &, badem::raw_key const &, std::shared_ptr<badem::block>) = 0;
	virtual std::shared_ptr<badem::vote> vote_generate (badem::transaction const &, badem::account const &, badem::raw_key const &, std::vector<badem::block_hash>) = 0;
	// Return either vote or the stored vote with a higher sequence number
	virtual std::shared_ptr<badem::vote> vote_max (badem::transaction const &, std::shared_ptr<badem::vote>) = 0;
	// Return latest vote for an account considering the vote cache
	virtual std::shared_ptr<badem::vote> vote_current (badem::transaction const &, badem::account const &) = 0;
	virtual void flush (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, std::shared_ptr<badem::vote>> vote_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<badem::account, std::shared_ptr<badem::vote>> vote_end () = 0;

	virtual void online_weight_put (badem::transaction const &, uint64_t, badem::amount const &) = 0;
	virtual void online_weight_del (badem::transaction const &, uint64_t) = 0;
	virtual badem::store_iterator<uint64_t, badem::amount> online_weight_begin (badem::transaction const &) = 0;
	virtual badem::store_iterator<uint64_t, badem::amount> online_weight_end () = 0;
	virtual size_t online_weight_count (badem::transaction const &) const = 0;
	virtual void online_weight_clear (badem::transaction const &) = 0;

	virtual void version_put (badem::transaction const &, int) = 0;
	virtual int version_get (badem::transaction const &) = 0;

	virtual void peer_put (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) = 0;
	virtual void peer_del (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) = 0;
	virtual bool peer_exists (badem::transaction const & transaction_a, badem::endpoint_key const & endpoint_a) const = 0;
	virtual size_t peer_count (badem::transaction const & transaction_a) const = 0;
	virtual void peer_clear (badem::transaction const & transaction_a) = 0;
	virtual badem::store_iterator<badem::endpoint_key, badem::no_value> peers_begin (badem::transaction const & transaction_a) = 0;
	virtual badem::store_iterator<badem::endpoint_key, badem::no_value> peers_end () = 0;

	// Requires a write transaction
	virtual badem::raw_key get_node_id (badem::transaction const &) = 0;

	/** Deletes the node ID from the store */
	virtual void delete_node_id (badem::transaction const &) = 0;

	/** Start read-write transaction */
	virtual badem::transaction tx_begin_write () = 0;

	/** Start read-only transaction */
	virtual badem::transaction tx_begin_read () = 0;

	/**
	 * Start a read-only or read-write transaction
	 * @param write If true, start a read-write transaction
	 */
	virtual badem::transaction tx_begin (bool write = false) = 0;
};
}
