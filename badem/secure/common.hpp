#pragma once

#include <badem/lib/blockbuilders.hpp>
#include <badem/lib/blocks.hpp>
#include <badem/lib/utility.hpp>
#include <badem/secure/utility.hpp>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/variant.hpp>

#include <unordered_map>

#include <crypto/blake2/blake2.h>

namespace boost
{
template <>
struct hash<::badem::uint256_union>
{
	size_t operator() (::badem::uint256_union const & value_a) const
	{
		std::hash<::badem::uint256_union> hash;
		return hash (value_a);
	}
};
template <>
struct hash<::badem::uint512_union>
{
	size_t operator() (::badem::uint512_union const & value_a) const
	{
		std::hash<::badem::uint512_union> hash;
		return hash (value_a);
	}
};
}
namespace badem
{
const uint8_t protocol_version = 0x10;
const uint8_t protocol_version_min = 0x0d;
const uint8_t node_id_version = 0x0c;

/*
 * Do not bootstrap from nodes older than this version.
 * Also, on the beta network do not process messages from
 * nodes older than this version.
 */
const uint8_t protocol_version_reasonable_min = 0x0d;

/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	keypair (badem::raw_key &&);
	badem::public_key pub;
	badem::raw_key prv;
};

/**
 * Tag for which epoch an entry belongs to
 */
enum class epoch : uint8_t
{
	invalid = 0,
	unspecified = 1,
	epoch_0 = 2,
	epoch_1 = 3
};

/**
 * Latest information about an account
 */
class account_info
{
public:
	account_info ();
	account_info (badem::account_info const &) = default;
	account_info (badem::block_hash const &, badem::block_hash const &, badem::block_hash const &, badem::amount const &, uint64_t, uint64_t, epoch);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::account_info const &) const;
	bool operator!= (badem::account_info const &) const;
	size_t db_size () const;
	badem::block_hash head;
	badem::block_hash rep_block;
	badem::block_hash open_block;
	badem::amount balance;
	/** Seconds since posix epoch */
	uint64_t modified;
	uint64_t block_count;
	badem::epoch epoch;
};

/**
 * Information on an uncollected send
 */
class pending_info
{
public:
	pending_info ();
	pending_info (badem::account const &, badem::amount const &, epoch);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::pending_info const &) const;
	badem::account source;
	badem::amount amount;
	badem::epoch epoch;
};
class pending_key
{
public:
	pending_key ();
	pending_key (badem::account const &, badem::block_hash const &);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::pending_key const &) const;
	badem::account account;
	badem::block_hash hash;
	badem::block_hash key () const;
};

class endpoint_key
{
public:
	endpoint_key () = default;

	/*
	 * @param address_a This should be in network byte order
	 * @param port_a This should be in host byte order
	 */
	endpoint_key (const std::array<uint8_t, 16> & address_a, uint16_t port_a);

	/*
	 * @return The ipv6 address in network byte order
	 */
	const std::array<uint8_t, 16> & address_bytes () const;

	/*
	 * @return The port in host byte order
	 */
	uint16_t port () const;

private:
	// Both stored internally in network byte order
	std::array<uint8_t, 16> address;
	uint16_t network_port{ 0 };
};

enum class no_value
{
	dummy
};

// Internally unchecked_key is equal to pending_key (2x uint256_union)
using unchecked_key = pending_key;

/**
 * Tag for block signature verification result
 */
enum class signature_verification : uint8_t
{
	unknown = 0,
	invalid = 1,
	valid = 2,
	valid_epoch = 3 // Valid for epoch blocks
};

/**
 * Information on an unchecked block
 */
class unchecked_info
{
public:
	unchecked_info ();
	unchecked_info (std::shared_ptr<badem::block>, badem::account const &, uint64_t, badem::signature_verification = badem::signature_verification::unknown);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::unchecked_info const &) const;
	std::shared_ptr<badem::block> block;
	badem::account account;
	/** Seconds since posix epoch */
	uint64_t modified;
	badem::signature_verification verified;
};

class block_info
{
public:
	block_info ();
	block_info (badem::account const &, badem::amount const &);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::block_info const &) const;
	badem::account account;
	badem::amount balance;
};
class block_counts
{
public:
	block_counts ();
	size_t sum ();
	size_t send;
	size_t receive;
	size_t open;
	size_t change;
	size_t state_v0;
	size_t state_v1;
};
typedef std::vector<boost::variant<std::shared_ptr<badem::block>, badem::block_hash>>::const_iterator vote_blocks_vec_iter;
class iterate_vote_blocks_as_hash
{
public:
	iterate_vote_blocks_as_hash () = default;
	badem::block_hash operator() (boost::variant<std::shared_ptr<badem::block>, badem::block_hash> const & item) const;
};
class vote
{
public:
	vote () = default;
	vote (badem::vote const &);
	vote (bool &, badem::stream &, badem::block_uniquer * = nullptr);
	vote (bool &, badem::stream &, badem::block_type, badem::block_uniquer * = nullptr);
	vote (badem::account const &, badem::raw_key const &, uint64_t, std::shared_ptr<badem::block>);
	vote (badem::account const &, badem::raw_key const &, uint64_t, std::vector<badem::block_hash>);
	std::string hashes_string () const;
	badem::uint256_union hash () const;
	badem::uint256_union full_hash () const;
	bool operator== (badem::vote const &) const;
	bool operator!= (badem::vote const &) const;
	void serialize (badem::stream &, badem::block_type);
	void serialize (badem::stream &);
	bool deserialize (badem::stream &, badem::block_uniquer * = nullptr);
	bool validate ();
	boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> begin () const;
	boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> end () const;
	std::string to_json () const;
	// Vote round sequence number
	uint64_t sequence;
	// The blocks, or block hashes, that this vote is for
	std::vector<boost::variant<std::shared_ptr<badem::block>, badem::block_hash>> blocks;
	// Account that's voting
	badem::account account;
	// Signature of sequence + block hashes
	badem::signature signature;
	static const std::string hash_prefix;
};
/**
 * This class serves to find and return unique variants of a vote in order to minimize memory usage
 */
class vote_uniquer
{
public:
	using value_type = std::pair<const badem::uint256_union, std::weak_ptr<badem::vote>>;

	vote_uniquer (badem::block_uniquer &);
	std::shared_ptr<badem::vote> unique (std::shared_ptr<badem::vote>);
	size_t size ();

private:
	badem::block_uniquer & uniquer;
	std::mutex mutex;
	std::unordered_map<std::remove_const_t<value_type::first_type>, value_type::second_type> votes;
	static unsigned constexpr cleanup_count = 2;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_uniquer & vote_uniquer, const std::string & name);

enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest sequence number, it's a replay
	vote // Vote has the highest sequence number
};

enum class process_result
{
	progress, // Hasn't been seen before, signed correctly
	bad_signature, // Signature was bad, forged or transmission error
	old, // Already seen and was valid
	negative_spend, // Malicious attempt to spend a negative amount
	fork, // Malicious fork based on previous
	unreceivable, // Source block doesn't exist, has already been received, or requires an account upgrade (epoch blocks)
	gap_previous, // Block marked as previous is unknown
	gap_source, // Block marked as source is unknown
	opened_burn_account, // The impossible happened, someone found the private key associated with the public key '0'.
	balance_mismatch, // Balance and amount delta don't match
	representative_mismatch, // Representative is changed when it is not allowed
	block_position // This block cannot follow the previous block
};
class process_return
{
public:
	badem::process_result code;
	badem::account account;
	badem::amount amount;
	badem::account pending_account;
	boost::optional<bool> state_is_send;
	badem::signature_verification verified;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};
extern badem::keypair const & zero_key;
extern badem::keypair const & test_genesis_key;
extern badem::account const & badem_test_account;
extern badem::account const & badem_beta_account;
extern badem::account const & badem_live_account;
extern std::string const & badem_test_genesis;
extern std::string const & badem_beta_genesis;
extern std::string const & badem_live_genesis;
extern std::string const & genesis_block;
extern badem::account const & genesis_account;
extern badem::account const & burn_account;
extern badem::uint128_t const & genesis_amount;
// An account number that compares inequal to any real account number
extern badem::account const & not_an_account ();
class genesis
{
public:
	explicit genesis ();
	badem::block_hash hash () const;
	std::shared_ptr<badem::block> open;
};
}
