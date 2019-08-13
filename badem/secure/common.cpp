#include <badem/secure/common.hpp>

#include <badem/lib/interface.h>
#include <badem/lib/numbers.hpp>
#include <badem/node/common.hpp>
#include <badem/secure/blockstore.hpp>
#include <badem/secure/versioning.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <queue>

#include <crypto/ed25519-donna/ed25519.h>

// Genesis keys for network variants
namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // bdm_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "A59A47CC4F593E75AE9AD653FDA9358E2F7898D9ACC8C60E80D0495CE20FBA9F"; // bdm_3betaz86ypbygpqbookmzpnmd5jhh4efmd8arr9a3n4bdmj1zgnzad7xpmfp
char const * live_public_key_data = "565936F0F5F7A41201FD3F831A52B6B1CDBB74AE2C3E8A590ABA08C048F111FA"; // bdm_1oks8urhdxx64a1zthw55bbdfegfqftcwd3yjbeiogiar36h46htzzkmmw3e
char const * test_genesis_data = R"%%%({
	"type": "open",
	"source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
	"representative": "bdm_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"account": "bdm_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"work": "9680625b39d3363d",
	"signature": "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"
})%%%";

char const * beta_genesis_data = R"%%%({
        "type": "open",
        "source": "499D3E2FC2DB9FC2D0D4C445AD073C713E5634D7B2BBF24F57A727A1BABF68AB",
        "representative": "bdm_1kex9rqw7pwzrdafbj47on5mrwbycrtfheouyb9ohbs9n8xdyt7d4xso99rb",
        "account": "bdm_1kex9rqw7pwzrdafbj47on5mrwbycrtfheouyb9ohbs9n8xdyt7d4xso99rb",
        "work": "f066e9305cbee8c8",
        "signature": "45776E6CE2011A662BF1F59D4C7376FCF1249B31D345F582CC62381BB902C28C20EDE58FF57772D7CD65365FB4C3CD158C4FECE63AB3B0E0B0CC87A8ECC24607"
})%%%";

char const * live_genesis_data = R"%%%({
	"type": "open",
	"source": "565936F0F5F7A41201FD3F831A52B6B1CDBB74AE2C3E8A590ABA08C048F111FA",
	"representative": "bdm_1oks8urhdxx64a1zthw55bbdfegfqftcwd3yjbeiogiar36h46htzzkmmw3e",
	"account": "bdm_1oks8urhdxx64a1zthw55bbdfegfqftcwd3yjbeiogiar36h46htzzkmmw3e",
	"work": "948c664ac4c86930",
	"signature": "378B2DCA3B4F2D196435C1EBB01A405A627E270AFE98C07042358BD379E4DFD8215E9C817E995787DFB4E06826115A4BA8F2DB6C79020DA4E85E0F4DD19B0606"
})%%%";

class ledger_constants
{
public:
	ledger_constants () :
	zero_key ("0"),
	test_genesis_key (test_private_key_data),
	badem_test_account (test_public_key_data),
	badem_beta_account (beta_public_key_data),
	badem_live_account (live_public_key_data),
	badem_test_genesis (test_genesis_data),
	badem_beta_genesis (beta_genesis_data),
	badem_live_genesis (live_genesis_data),
	genesis_account (badem::is_test_network ? badem_test_account : badem::is_beta_network ? badem_beta_account : badem_live_account),
	genesis_block (badem::is_test_network ? badem_test_genesis : badem::is_beta_network ? badem_beta_genesis : badem_live_genesis),
	genesis_amount (std::numeric_limits<badem::uint128_t>::max ()),
	burn_account (0)
	{
	}
	badem::keypair zero_key;
	badem::keypair test_genesis_key;
	badem::account badem_test_account;
	badem::account badem_beta_account;
	badem::account badem_live_account;
	std::string badem_test_genesis;
	std::string badem_beta_genesis;
	std::string badem_live_genesis;
	badem::account genesis_account;
	std::string genesis_block;
	badem::uint128_t genesis_amount;
	badem::account burn_account;

	badem::account const & not_an_account ()
	{
		std::lock_guard<std::mutex> lk (mutex);
		if (!is_initialized)
		{
			// Randomly generating this means that no two nodes will ever have the same sentinel value which protects against some insecure algorithms
			badem::random_pool::generate_block (not_an_account_m.bytes.data (), not_an_account_m.bytes.size ());
			is_initialized = true;
		}
		return not_an_account_m;
	}

private:
	badem::account not_an_account_m;
	std::mutex mutex;
	bool is_initialized{ false };
};
ledger_constants globals;
}

size_t constexpr badem::send_block::size;
size_t constexpr badem::receive_block::size;
size_t constexpr badem::open_block::size;
size_t constexpr badem::change_block::size;
size_t constexpr badem::state_block::size;

badem::keypair const & badem::zero_key (globals.zero_key);
badem::keypair const & badem::test_genesis_key (globals.test_genesis_key);
badem::account const & badem::badem_test_account (globals.badem_test_account);
badem::account const & badem::badem_beta_account (globals.badem_beta_account);
badem::account const & badem::badem_live_account (globals.badem_live_account);
std::string const & badem::badem_test_genesis (globals.badem_test_genesis);
std::string const & badem::badem_beta_genesis (globals.badem_beta_genesis);
std::string const & badem::badem_live_genesis (globals.badem_live_genesis);

badem::account const & badem::genesis_account (globals.genesis_account);
std::string const & badem::genesis_block (globals.genesis_block);
badem::uint128_t const & badem::genesis_amount (globals.genesis_amount);
badem::account const & badem::burn_account (globals.burn_account);
badem::account const & badem::not_an_account ()
{
	return globals.not_an_account ();
}
// Create a new random keypair
badem::keypair::keypair ()
{
	random_pool::generate_block (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
badem::keypair::keypair (badem::raw_key && prv_a) :
prv (std::move (prv_a))
{
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
badem::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void badem::serialize_block (badem::stream & stream_a, badem::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

badem::account_info::account_info () :
head (0),
rep_block (0),
open_block (0),
balance (0),
modified (0),
block_count (0),
epoch (badem::epoch::epoch_0)
{
}

badem::account_info::account_info (badem::block_hash const & head_a, badem::block_hash const & rep_block_a, badem::block_hash const & open_block_a, badem::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a, badem::epoch epoch_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
balance (balance_a),
modified (modified_a),
block_count (block_count_a),
epoch (epoch_a)
{
}

void badem::account_info::serialize (badem::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, balance.bytes);
	write (stream_a, modified);
	write (stream_a, block_count);
}

bool badem::account_info::deserialize (badem::stream & stream_a)
{
	auto error (false);
	try
	{
		badem::read (stream_a, head.bytes);
		badem::read (stream_a, rep_block.bytes);
		badem::read (stream_a, open_block.bytes);
		badem::read (stream_a, balance.bytes);
		badem::read (stream_a, modified);
		badem::read (stream_a, block_count);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool badem::account_info::operator== (badem::account_info const & other_a) const
{
	return head == other_a.head && rep_block == other_a.rep_block && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count && epoch == other_a.epoch;
}

bool badem::account_info::operator!= (badem::account_info const & other_a) const
{
	return !(*this == other_a);
}

size_t badem::account_info::db_size () const
{
	assert (reinterpret_cast<const uint8_t *> (this) == reinterpret_cast<const uint8_t *> (&head));
	assert (reinterpret_cast<const uint8_t *> (&head) + sizeof (head) == reinterpret_cast<const uint8_t *> (&rep_block));
	assert (reinterpret_cast<const uint8_t *> (&rep_block) + sizeof (rep_block) == reinterpret_cast<const uint8_t *> (&open_block));
	assert (reinterpret_cast<const uint8_t *> (&open_block) + sizeof (open_block) == reinterpret_cast<const uint8_t *> (&balance));
	assert (reinterpret_cast<const uint8_t *> (&balance) + sizeof (balance) == reinterpret_cast<const uint8_t *> (&modified));
	assert (reinterpret_cast<const uint8_t *> (&modified) + sizeof (modified) == reinterpret_cast<const uint8_t *> (&block_count));
	return sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count);
}

badem::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0),
state_v0 (0),
state_v1 (0)
{
}

size_t badem::block_counts::sum ()
{
	return send + receive + open + change + state_v0 + state_v1;
}

badem::pending_info::pending_info () :
source (0),
amount (0),
epoch (badem::epoch::epoch_0)
{
}

badem::pending_info::pending_info (badem::account const & source_a, badem::amount const & amount_a, badem::epoch epoch_a) :
source (source_a),
amount (amount_a),
epoch (epoch_a)
{
}

void badem::pending_info::serialize (badem::stream & stream_a) const
{
	badem::write (stream_a, source.bytes);
	badem::write (stream_a, amount.bytes);
}

bool badem::pending_info::deserialize (badem::stream & stream_a)
{
	auto error (false);
	try
	{
		badem::read (stream_a, source.bytes);
		badem::read (stream_a, amount.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool badem::pending_info::operator== (badem::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && epoch == other_a.epoch;
}

badem::pending_key::pending_key () :
account (0),
hash (0)
{
}

badem::pending_key::pending_key (badem::account const & account_a, badem::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

void badem::pending_key::serialize (badem::stream & stream_a) const
{
	badem::write (stream_a, account.bytes);
	badem::write (stream_a, hash.bytes);
}

bool badem::pending_key::deserialize (badem::stream & stream_a)
{
	auto error (false);
	try
	{
		badem::read (stream_a, account.bytes);
		badem::read (stream_a, hash.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool badem::pending_key::operator== (badem::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

badem::block_hash badem::pending_key::key () const
{
	return account;
}

badem::unchecked_info::unchecked_info () :
block (nullptr),
account (0),
modified (0),
verified (badem::signature_verification::unknown)
{
}

badem::unchecked_info::unchecked_info (std::shared_ptr<badem::block> block_a, badem::account const & account_a, uint64_t modified_a, badem::signature_verification verified_a) :
block (block_a),
account (account_a),
modified (modified_a),
verified (verified_a)
{
}

void badem::unchecked_info::serialize (badem::stream & stream_a) const
{
	assert (block != nullptr);
	badem::serialize_block (stream_a, *block);
	badem::write (stream_a, account.bytes);
	badem::write (stream_a, modified);
	badem::write (stream_a, verified);
}

bool badem::unchecked_info::deserialize (badem::stream & stream_a)
{
	block = badem::deserialize_block (stream_a);
	bool error (block == nullptr);
	if (!error)
	{
		try
		{
			badem::read (stream_a, account.bytes);
			badem::read (stream_a, modified);
			badem::read (stream_a, verified);
		}
		catch (std::runtime_error const &)
		{
			error = true;
		}
	}
	return error;
}

bool badem::unchecked_info::operator== (badem::unchecked_info const & other_a) const
{
	return block->hash () == other_a.block->hash () && account == other_a.account && modified == other_a.modified && verified == other_a.verified;
}

badem::endpoint_key::endpoint_key (const std::array<uint8_t, 16> & address_a, uint16_t port_a) :
address (address_a), network_port (boost::endian::native_to_big (port_a))
{
}

const std::array<uint8_t, 16> & badem::endpoint_key::address_bytes () const
{
	return address;
}

uint16_t badem::endpoint_key::port () const
{
	return boost::endian::big_to_native (network_port);
}

badem::block_info::block_info () :
account (0),
balance (0)
{
}

badem::block_info::block_info (badem::account const & account_a, badem::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

void badem::block_info::serialize (badem::stream & stream_a) const
{
	badem::write (stream_a, account.bytes);
	badem::write (stream_a, balance.bytes);
}

bool badem::block_info::deserialize (badem::stream & stream_a)
{
	auto error (false);
	try
	{
		badem::read (stream_a, account.bytes);
		badem::read (stream_a, balance.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool badem::block_info::operator== (badem::block_info const & other_a) const
{
	return account == other_a.account && balance == other_a.balance;
}

bool badem::vote::operator== (badem::vote const & other_a) const
{
	auto blocks_equal (true);
	if (blocks.size () != other_a.blocks.size ())
	{
		blocks_equal = false;
	}
	else
	{
		for (auto i (0); blocks_equal && i < blocks.size (); ++i)
		{
			auto block (blocks[i]);
			auto other_block (other_a.blocks[i]);
			if (block.which () != other_block.which ())
			{
				blocks_equal = false;
			}
			else if (block.which ())
			{
				if (boost::get<badem::block_hash> (block) != boost::get<badem::block_hash> (other_block))
				{
					blocks_equal = false;
				}
			}
			else
			{
				if (!(*boost::get<std::shared_ptr<badem::block>> (block) == *boost::get<std::shared_ptr<badem::block>> (other_block)))
				{
					blocks_equal = false;
				}
			}
		}
	}
	return sequence == other_a.sequence && blocks_equal && account == other_a.account && signature == other_a.signature;
}

bool badem::vote::operator!= (badem::vote const & other_a) const
{
	return !(*this == other_a);
}

std::string badem::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	boost::property_tree::ptree blocks_tree;
	for (auto block : blocks)
	{
		if (block.which ())
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<badem::block>> (block)->to_json ());
		}
		else
		{
			blocks_tree.put ("", boost::get<std::shared_ptr<badem::block>> (block)->hash ().to_string ());
		}
	}
	tree.add_child ("blocks", blocks_tree);
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

badem::vote::vote (badem::vote const & other_a) :
sequence (other_a.sequence),
blocks (other_a.blocks),
account (other_a.account),
signature (other_a.signature)
{
}

badem::vote::vote (bool & error_a, badem::stream & stream_a, badem::block_uniquer * uniquer_a)
{
	error_a = deserialize (stream_a, uniquer_a);
}

badem::vote::vote (bool & error_a, badem::stream & stream_a, badem::block_type type_a, badem::block_uniquer * uniquer_a)
{
	try
	{
		badem::read (stream_a, account.bytes);
		badem::read (stream_a, signature.bytes);
		badem::read (stream_a, sequence);

		while (stream_a.in_avail () > 0)
		{
			if (type_a == badem::block_type::not_a_block)
			{
				badem::block_hash block_hash;
				badem::read (stream_a, block_hash);
				blocks.push_back (block_hash);
			}
			else
			{
				std::shared_ptr<badem::block> block (badem::deserialize_block (stream_a, type_a, uniquer_a));
				if (block == nullptr)
				{
					throw std::runtime_error ("Block is null");
				}
				blocks.push_back (block);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}

	if (blocks.empty ())
	{
		error_a = true;
	}
}

badem::vote::vote (badem::account const & account_a, badem::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<badem::block> block_a) :
sequence (sequence_a),
blocks (1, block_a),
account (account_a),
signature (badem::sign_message (prv_a, account_a, hash ()))
{
}

badem::vote::vote (badem::account const & account_a, badem::raw_key const & prv_a, uint64_t sequence_a, std::vector<badem::block_hash> blocks_a) :
sequence (sequence_a),
account (account_a)
{
	assert (blocks_a.size () > 0);
	assert (blocks_a.size () <= 12);
	for (auto hash : blocks_a)
	{
		blocks.push_back (hash);
	}
	signature = badem::sign_message (prv_a, account_a, hash ());
}

std::string badem::vote::hashes_string () const
{
	std::string result;
	for (auto hash : *this)
	{
		result += hash.to_string ();
		result += ", ";
	}
	return result;
}

const std::string badem::vote::hash_prefix = "vote ";

badem::uint256_union badem::vote::hash () const
{
	badem::uint256_union result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	if (blocks.size () > 1 || (blocks.size () > 0 && blocks[0].which ()))
	{
		blake2b_update (&hash, hash_prefix.data (), hash_prefix.size ());
	}
	for (auto block_hash : *this)
	{
		blake2b_update (&hash, block_hash.bytes.data (), sizeof (block_hash.bytes));
	}
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = sequence;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

badem::uint256_union badem::vote::full_hash () const
{
	badem::uint256_union result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ().bytes));
	blake2b_update (&state, account.bytes.data (), sizeof (account.bytes.data ()));
	blake2b_update (&state, signature.bytes.data (), sizeof (signature.bytes.data ()));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void badem::vote::serialize (badem::stream & stream_a, badem::block_type type)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			assert (type == badem::block_type::not_a_block);
			write (stream_a, boost::get<badem::block_hash> (block));
		}
		else
		{
			if (type == badem::block_type::not_a_block)
			{
				write (stream_a, boost::get<std::shared_ptr<badem::block>> (block)->hash ());
			}
			else
			{
				boost::get<std::shared_ptr<badem::block>> (block)->serialize (stream_a);
			}
		}
	}
}

void badem::vote::serialize (badem::stream & stream_a)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto block : blocks)
	{
		if (block.which ())
		{
			write (stream_a, badem::block_type::not_a_block);
			write (stream_a, boost::get<badem::block_hash> (block));
		}
		else
		{
			badem::serialize_block (stream_a, *boost::get<std::shared_ptr<badem::block>> (block));
		}
	}
}

bool badem::vote::deserialize (badem::stream & stream_a, badem::block_uniquer * uniquer_a)
{
	auto error (false);
	try
	{
		badem::read (stream_a, account);
		badem::read (stream_a, signature);
		badem::read (stream_a, sequence);

		badem::block_type type;

		while (true)
		{
			if (badem::try_read (stream_a, type))
			{
				// Reached the end of the stream
				break;
			}

			if (type == badem::block_type::not_a_block)
			{
				badem::block_hash block_hash;
				badem::read (stream_a, block_hash);
				blocks.push_back (block_hash);
			}
			else
			{
				std::shared_ptr<badem::block> block (badem::deserialize_block (stream_a, type, uniquer_a));
				if (block == nullptr)
				{
					throw std::runtime_error ("Block is empty");
				}

				blocks.push_back (block);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	if (blocks.empty ())
	{
		error = true;
	}

	return error;
}

bool badem::vote::validate ()
{
	auto result (badem::validate_message (account, hash (), signature));
	return result;
}

badem::block_hash badem::iterate_vote_blocks_as_hash::operator() (boost::variant<std::shared_ptr<badem::block>, badem::block_hash> const & item) const
{
	badem::block_hash result;
	if (item.which ())
	{
		result = boost::get<badem::block_hash> (item);
	}
	else
	{
		result = boost::get<std::shared_ptr<badem::block>> (item)->hash ();
	}
	return result;
}

boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> badem::vote::begin () const
{
	return boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> (blocks.begin (), badem::iterate_vote_blocks_as_hash ());
}

boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> badem::vote::end () const
{
	return boost::transform_iterator<badem::iterate_vote_blocks_as_hash, badem::vote_blocks_vec_iter> (blocks.end (), badem::iterate_vote_blocks_as_hash ());
}

badem::vote_uniquer::vote_uniquer (badem::block_uniquer & uniquer_a) :
uniquer (uniquer_a)
{
}

std::shared_ptr<badem::vote> badem::vote_uniquer::unique (std::shared_ptr<badem::vote> vote_a)
{
	auto result (vote_a);
	if (result != nullptr && !result->blocks.empty ())
	{
		if (!result->blocks[0].which ())
		{
			result->blocks[0] = uniquer.unique (boost::get<std::shared_ptr<badem::block>> (result->blocks[0]));
		}
		badem::uint256_union key (vote_a->full_hash ());
		std::lock_guard<std::mutex> lock (mutex);
		auto & existing (votes[key]);
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = vote_a;
		}

		release_assert (std::numeric_limits<CryptoPP::word32>::max () > votes.size ());
		for (auto i (0); i < cleanup_count && votes.size () > 0; ++i)
		{
			auto random_offset = badem::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (votes.size () - 1));

			auto existing (std::next (votes.begin (), random_offset));
			if (existing == votes.end ())
			{
				existing = votes.begin ();
			}
			if (existing != votes.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					votes.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t badem::vote_uniquer::size ()
{
	std::lock_guard<std::mutex> lock (mutex);
	return votes.size ();
}

namespace badem
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_uniquer & vote_uniquer, const std::string & name)
{
	auto count = vote_uniquer.size ();
	auto sizeof_element = sizeof (vote_uniquer::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "votes", count, sizeof_element }));
	return composite;
}
}

badem::genesis::genesis ()
{
	boost::property_tree::ptree tree;
	std::stringstream istream (badem::genesis_block);
	boost::property_tree::read_json (istream, tree);
	open = badem::deserialize_block_json (tree);
	assert (open != nullptr);
}

badem::block_hash badem::genesis::hash () const
{
	return open->hash ();
}
