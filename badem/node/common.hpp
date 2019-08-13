#pragma once

#include <badem/lib/interface.h>
#include <badem/secure/common.hpp>

#include <boost/asio.hpp>

#include <bitset>

#include <crypto/xxhash/xxhash.h>

namespace badem
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, badem::endpoint &);
bool parse_tcp_endpoint (std::string const &, badem::tcp_endpoint &);
bool reserved_address (badem::endpoint const &, bool);
}

namespace
{
uint64_t endpoint_hash_raw (badem::endpoint const & endpoint_a)
{
	assert (endpoint_a.address ().is_v6 ());
	badem::uint128_union address;
	address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
	XXH64_state_t * const state = XXH64_createState ();
	XXH64_reset (state, 0);
	XXH64_update (state, address.bytes.data (), address.bytes.size ());
	auto port (endpoint_a.port ());
	XXH64_update (state, &port, sizeof (port));
	auto result (XXH64_digest (state));
	XXH64_freeState (state);
	return result;
}
uint64_t endpoint_hash_raw (badem::tcp_endpoint const & endpoint_a)
{
	assert (endpoint_a.address ().is_v6 ());
	badem::uint128_union address;
	address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
	XXH64_state_t * const state = XXH64_createState ();
	XXH64_reset (state, 0);
	XXH64_update (state, address.bytes.data (), address.bytes.size ());
	auto port (endpoint_a.port ());
	XXH64_update (state, &port, sizeof (port));
	auto result (XXH64_digest (state));
	XXH64_freeState (state);
	return result;
}
uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a)
{
	assert (ip_a.is_v6 ());
	badem::uint128_union bytes;
	bytes.bytes = ip_a.to_v6 ().to_bytes ();
	auto result (XXH64 (bytes.bytes.data (), bytes.bytes.size (), 0));
	return result;
}

template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
	size_t operator() (badem::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
	size_t operator() (badem::tcp_endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};
template <>
struct endpoint_hash<4>
{
	size_t operator() (badem::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
	size_t operator() (badem::tcp_endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
template <size_t size>
struct ip_address_hash
{
};
template <>
struct ip_address_hash<8>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		return ip_address_hash_raw (ip_address_a);
	}
};
template <>
struct ip_address_hash<4>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		uint64_t big (ip_address_hash_raw (ip_address_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
}

namespace std
{
template <>
struct hash<::badem::endpoint>
{
	size_t operator() (::badem::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
template <>
struct hash<::badem::tcp_endpoint>
{
	size_t operator() (::badem::tcp_endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
template <>
struct hash<boost::asio::ip::address>
{
	size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		ip_address_hash<sizeof (size_t)> ihash;
		return ihash (ip_a);
	}
};
}
namespace boost
{
template <>
struct hash<::badem::endpoint>
{
	size_t operator() (::badem::endpoint const & endpoint_a) const
	{
		std::hash<::badem::endpoint> hash;
		return hash (endpoint_a);
	}
};
}

namespace badem
{
/**
 * Message types are serialized to the network and existing values must thus never change as
 * types are added, removed and reordered in the enum.
 */
enum class message_type : uint8_t
{
	invalid = 0x0,
	not_a_type = 0x1,
	keepalive = 0x2,
	publish = 0x3,
	confirm_req = 0x4,
	confirm_ack = 0x5,
	bulk_pull = 0x6,
	bulk_push = 0x7,
	frontier_req = 0x8,
	/* deleted 0x9 */
	node_id_handshake = 0x0a,
	bulk_pull_account = 0x0b
};
enum class bulk_pull_account_flags : uint8_t
{
	pending_hash_and_amount = 0x0,
	pending_address_only = 0x1,
	pending_hash_amount_and_address = 0x2
};
class message_visitor;
class message_header
{
public:
	message_header (badem::message_type);
	message_header (bool &, badem::stream &);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	badem::block_type block_type () const;
	void block_type_set (badem::block_type);
	static std::array<uint8_t, 2> constexpr magic_number = badem::is_test_network ? std::array<uint8_t, 2>{ { 'R', 'A' } } : badem::is_beta_network ? std::array<uint8_t, 2>{ { 'R', 'B' } } : std::array<uint8_t, 2>{ { 'R', 'C' } };
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	badem::message_type type;
	std::bitset<16> extensions;

	static size_t constexpr bulk_pull_count_present_flag = 0;
	bool bulk_pull_is_count_present () const;

	/** Size of the payload in bytes. For some messages, the payload size is based on header flags. */
	size_t payload_length_bytes () const;

	static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
	bool valid_magic () const
	{
		return magic_number[0] == 'R' && magic_number[1] >= 'A' && magic_number[1] <= 'C';
	}
	bool valid_network () const
	{
		return (magic_number[1] - 'A') == static_cast<int> (badem::badem_network);
	}
};
class message
{
public:
	message (badem::message_type);
	message (badem::message_header const &);
	virtual ~message () = default;
	virtual void serialize (badem::stream &) const = 0;
	virtual void visit (badem::message_visitor &) const = 0;
	virtual std::shared_ptr<std::vector<uint8_t>> to_bytes () const
	{
		std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
		badem::vectorstream stream (*bytes);
		serialize (stream);
		return bytes;
	}
	badem::message_header header;
};
class work_pool;
class message_parser
{
public:
	enum class parse_status
	{
		success,
		insufficient_work,
		invalid_header,
		invalid_message_type,
		invalid_keepalive_message,
		invalid_publish_message,
		invalid_confirm_req_message,
		invalid_confirm_ack_message,
		invalid_node_id_handshake_message,
		outdated_version,
		invalid_magic,
		invalid_network
	};
	message_parser (badem::block_uniquer &, badem::vote_uniquer &, badem::message_visitor &, badem::work_pool &);
	void deserialize_buffer (uint8_t const *, size_t);
	void deserialize_keepalive (badem::stream &, badem::message_header const &);
	void deserialize_publish (badem::stream &, badem::message_header const &);
	void deserialize_confirm_req (badem::stream &, badem::message_header const &);
	void deserialize_confirm_ack (badem::stream &, badem::message_header const &);
	void deserialize_node_id_handshake (badem::stream &, badem::message_header const &);
	bool at_end (badem::stream &);
	badem::block_uniquer & block_uniquer;
	badem::vote_uniquer & vote_uniquer;
	badem::message_visitor & visitor;
	badem::work_pool & pool;
	parse_status status;
	std::string status_string ();
	static const size_t max_safe_udp_message_size;
};
class keepalive : public message
{
public:
	keepalive (bool &, badem::stream &, badem::message_header const &);
	keepalive ();
	void visit (badem::message_visitor &) const override;
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	bool operator== (badem::keepalive const &) const;
	std::array<badem::endpoint, 8> peers;
	static size_t constexpr size = 8 * (16 + 2);
};
class publish : public message
{
public:
	publish (bool &, badem::stream &, badem::message_header const &, badem::block_uniquer * = nullptr);
	publish (std::shared_ptr<badem::block>);
	void visit (badem::message_visitor &) const override;
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &, badem::block_uniquer * = nullptr);
	bool operator== (badem::publish const &) const;
	std::shared_ptr<badem::block> block;
};
class confirm_req : public message
{
public:
	confirm_req (bool &, badem::stream &, badem::message_header const &, badem::block_uniquer * = nullptr);
	confirm_req (std::shared_ptr<badem::block>);
	confirm_req (std::vector<std::pair<badem::block_hash, badem::block_hash>> const &);
	confirm_req (badem::block_hash const &, badem::block_hash const &);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &, badem::block_uniquer * = nullptr);
	void visit (badem::message_visitor &) const override;
	bool operator== (badem::confirm_req const &) const;
	std::shared_ptr<badem::block> block;
	std::vector<std::pair<badem::block_hash, badem::block_hash>> roots_hashes;
	std::string roots_string () const;
};
class confirm_ack : public message
{
public:
	confirm_ack (bool &, badem::stream &, badem::message_header const &, badem::vote_uniquer * = nullptr);
	confirm_ack (std::shared_ptr<badem::vote>);
	void serialize (badem::stream &) const override;
	void visit (badem::message_visitor &) const override;
	bool operator== (badem::confirm_ack const &) const;
	std::shared_ptr<badem::vote> vote;
};
class frontier_req : public message
{
public:
	frontier_req ();
	frontier_req (bool &, badem::stream &, badem::message_header const &);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	void visit (badem::message_visitor &) const override;
	bool operator== (badem::frontier_req const &) const;
	badem::account start;
	uint32_t age;
	uint32_t count;
	static size_t constexpr size = sizeof (start) + sizeof (age) + sizeof (count);
};
class bulk_pull : public message
{
public:
	typedef uint32_t count_t;
	bulk_pull ();
	bulk_pull (bool &, badem::stream &, badem::message_header const &);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	void visit (badem::message_visitor &) const override;
	badem::uint256_union start;
	badem::block_hash end;
	count_t count;
	bool is_count_present () const;
	void set_count_present (bool);
	static size_t constexpr count_present_flag = badem::message_header::bulk_pull_count_present_flag;
	static size_t constexpr extended_parameters_size = 8;
	static size_t constexpr size = sizeof (start) + sizeof (end);
};
class bulk_pull_account : public message
{
public:
	bulk_pull_account ();
	bulk_pull_account (bool &, badem::stream &, badem::message_header const &);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	void visit (badem::message_visitor &) const override;
	badem::uint256_union account;
	badem::uint128_union minimum_amount;
	bulk_pull_account_flags flags;
	static size_t constexpr size = sizeof (account) + sizeof (minimum_amount) + sizeof (bulk_pull_account_flags);
};
class bulk_push : public message
{
public:
	bulk_push ();
	bulk_push (badem::message_header const &);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	void visit (badem::message_visitor &) const override;
};
class node_id_handshake : public message
{
public:
	node_id_handshake (bool &, badem::stream &, badem::message_header const &);
	node_id_handshake (boost::optional<badem::block_hash>, boost::optional<std::pair<badem::account, badem::signature>>);
	void serialize (badem::stream &) const override;
	bool deserialize (badem::stream &);
	void visit (badem::message_visitor &) const override;
	bool operator== (badem::node_id_handshake const &) const;
	bool is_query_flag () const;
	void set_query_flag (bool);
	bool is_response_flag () const;
	void set_response_flag (bool);
	boost::optional<badem::uint256_union> query;
	boost::optional<std::pair<badem::account, badem::signature>> response;
	static size_t constexpr query_flag = 0;
	static size_t constexpr response_flag = 1;
};
class message_visitor
{
public:
	virtual void keepalive (badem::keepalive const &) = 0;
	virtual void publish (badem::publish const &) = 0;
	virtual void confirm_req (badem::confirm_req const &) = 0;
	virtual void confirm_ack (badem::confirm_ack const &) = 0;
	virtual void bulk_pull (badem::bulk_pull const &) = 0;
	virtual void bulk_pull_account (badem::bulk_pull_account const &) = 0;
	virtual void bulk_push (badem::bulk_push const &) = 0;
	virtual void frontier_req (badem::frontier_req const &) = 0;
	virtual void node_id_handshake (badem::node_id_handshake const &) = 0;
	virtual ~message_visitor ();
};

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
	return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}
}
