#include <badem/common.hpp>

#include <badem/blockstore.hpp>
#include <badem/lib/interface.h>
#include <badem/node/common.hpp>
#include <badem/versioning.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <queue>

#include <ed25519-donna/ed25519.h>

// Genesis keys for network variants
namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // bdm_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "2D1D8E3C9D12200F92FE816B10DAF4A53883A2C320AC903BDFBECF91DC7AAD5D"; // bdm_1daxjrybt6j13ybhx1dd45fhbbbrigje8a7ek1xxzhphk9g9odcxn874any1
char const * live_public_key_data = "1A89922741BA8FB529EB7C9A1A465C813417B91383D18E7CCF9003F61495DDCA"; // bdm_18nbkamn5gnhpnnypz6t5b57s1bn4ywj91yjjsyez615yrcbdqgcuxms6e43
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
	"source": "2D1D8E3C9D12200F92FE816B10DAF4A53883A2C320AC903BDFBECF91DC7AAD5D",
	"representative": "bdm_1daxjrybt6j13ybhx1dd45fhbbbrigje8a7ek1xxzhphk9g9odcxn874any1",
	"account": "bdm_1daxjrybt6j13ybhx1dd45fhbbbrigje8a7ek1xxzhphk9g9odcxn874any1",
	"work": "d967179634337e2d",
	"signature": "AF7F1917ED42989442FD6ED7D367F0EF3D010AC1B6D7F37556F6D9666B1CD5F59108C92CD0D249DE10592EBC8B721C5C655DD0824C82304DBF13FEE654A87E03"
})%%%";

char const * live_genesis_data = R"%%%({
	"type": "open",
	"source": "1A89922741BA8FB529EB7C9A1A465C813417B91383D18E7CCF9003F61495DDCA",
	"representative": "bdm_18nbkamn5gnhpnnypz6t5b57s1bn4ywj91yjjsyez615yrcbdqgcuxms6e43",
	"account": "bdm_18nbkamn5gnhpnnypz6t5b57s1bn4ywj91yjjsyez615yrcbdqgcuxms6e43",
	"work": "f1c935a200662651",
	"signature": "2971C2F11CCBB69846ACA4A5CDD72FA28094D9C8641788C24256C47792776EC1A685E1877700E7D2DC2ECDA25AC418BD82A4F816EFA8A23645001205FA369B06"
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
	genesis_account (rai::badem_network == rai::badem_networks::badem_test_network ? badem_test_account : rai::badem_network == rai::badem_networks::badem_beta_network ? badem_beta_account : badem_live_account),
	genesis_block (rai::badem_network == rai::badem_networks::badem_test_network ? badem_test_genesis : rai::badem_network == rai::badem_networks::badem_beta_network ? badem_beta_genesis : badem_live_genesis),
	genesis_amount (std::numeric_limits<rai::uint128_t>::max ()),
	burn_account (0)
	{
		CryptoPP::AutoSeededRandomPool random_pool;
		// Randomly generating these mean no two nodes will ever have the same sentinel values which protects against some insecure algorithms
		random_pool.GenerateBlock (not_a_block.bytes.data (), not_a_block.bytes.size ());
		random_pool.GenerateBlock (not_an_account.bytes.data (), not_an_account.bytes.size ());
	}
	rai::keypair zero_key;
	rai::keypair test_genesis_key;
	rai::account badem_test_account;
	rai::account badem_beta_account;
	rai::account badem_live_account;
	std::string badem_test_genesis;
	std::string badem_beta_genesis;
	std::string badem_live_genesis;
	rai::account genesis_account;
	std::string genesis_block;
	rai::uint128_t genesis_amount;
	rai::block_hash not_a_block;
	rai::account not_an_account;
	rai::account burn_account;
};
ledger_constants globals;
}

size_t constexpr rai::send_block::size;
size_t constexpr rai::receive_block::size;
size_t constexpr rai::open_block::size;
size_t constexpr rai::change_block::size;
size_t constexpr rai::state_block::size;

rai::keypair const & rai::zero_key (globals.zero_key);
rai::keypair const & rai::test_genesis_key (globals.test_genesis_key);
rai::account const & rai::badem_test_account (globals.badem_test_account);
rai::account const & rai::badem_beta_account (globals.badem_beta_account);
rai::account const & rai::badem_live_account (globals.badem_live_account);
std::string const & rai::badem_test_genesis (globals.badem_test_genesis);
std::string const & rai::badem_beta_genesis (globals.badem_beta_genesis);
std::string const & rai::badem_live_genesis (globals.badem_live_genesis);

rai::account const & rai::genesis_account (globals.genesis_account);
std::string const & rai::genesis_block (globals.genesis_block);
rai::uint128_t const & rai::genesis_amount (globals.genesis_amount);
rai::block_hash const & rai::not_a_block (globals.not_a_block);
rai::block_hash const & rai::not_an_account (globals.not_an_account);
rai::account const & rai::burn_account (globals.burn_account);

rai::votes::votes (std::shared_ptr<rai::block> block_a) :
id (block_a->root ())
{
	rep_votes.insert (std::make_pair (rai::not_an_account, block_a));
}

rai::tally_result rai::votes::vote (std::shared_ptr<rai::vote> vote_a)
{
	rai::tally_result result;
	auto existing (rep_votes.find (vote_a->account));
	if (existing == rep_votes.end ())
	{
		// Vote on this block hasn't been seen from rep before
		result = rai::tally_result::vote;
		rep_votes.insert (std::make_pair (vote_a->account, vote_a->block));
	}
	else
	{
		if (!(*existing->second == *vote_a->block))
		{
			// Rep changed their vote
			result = rai::tally_result::changed;
			existing->second = vote_a->block;
		}
		else
		{
			// Rep vote remained the same
			result = rai::tally_result::confirm;
		}
	}
	return result;
}

bool rai::votes::uncontested ()
{
	bool result (true);
	if (!rep_votes.empty ())
	{
		auto block (rep_votes.begin ()->second);
		for (auto i (rep_votes.begin ()), n (rep_votes.end ()); result && i != n; ++i)
		{
			result = *i->second == *block;
		}
	}
	return result;
}

// Create a new random keypair
rai::keypair::keypair ()
{
	random_pool.GenerateBlock (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
rai::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void rai::serialize_block (rai::stream & stream_a, rai::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

std::unique_ptr<rai::block> rai::deserialize_block (MDB_val const & val_a)
{
	rai::bufferstream stream (reinterpret_cast<uint8_t const *> (val_a.mv_data), val_a.mv_size);
	return deserialize_block (stream);
}

rai::account_info::account_info () :
head (0),
rep_block (0),
open_block (0),
balance (0),
modified (0),
block_count (0)
{
}

rai::account_info::account_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count) == sizeof (*this), "Class not packed");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

rai::account_info::account_info (rai::block_hash const & head_a, rai::block_hash const & rep_block_a, rai::block_hash const & open_block_a, rai::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
balance (balance_a),
modified (modified_a),
block_count (block_count_a)
{
}

void rai::account_info::serialize (rai::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, balance.bytes);
	write (stream_a, modified);
	write (stream_a, block_count);
}

bool rai::account_info::deserialize (rai::stream & stream_a)
{
	auto error (read (stream_a, head.bytes));
	if (!error)
	{
		error = read (stream_a, rep_block.bytes);
		if (!error)
		{
			error = read (stream_a, open_block.bytes);
			if (!error)
			{
				error = read (stream_a, balance.bytes);
				if (!error)
				{
					error = read (stream_a, modified);
					if (!error)
					{
						error = read (stream_a, block_count);
					}
				}
			}
		}
	}
	return error;
}

bool rai::account_info::operator== (rai::account_info const & other_a) const
{
	return head == other_a.head && rep_block == other_a.rep_block && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count;
}

bool rai::account_info::operator!= (rai::account_info const & other_a) const
{
	return !(*this == other_a);
}

rai::mdb_val rai::account_info::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::account_info *> (this));
}

rai::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0)
{
}

size_t rai::block_counts::sum ()
{
	return send + receive + open + change + state;
}

rai::pending_info::pending_info () :
source (0),
amount (0)
{
}

rai::pending_info::pending_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (source) + sizeof (amount) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

rai::pending_info::pending_info (rai::account const & source_a, rai::amount const & amount_a) :
source (source_a),
amount (amount_a)
{
}

void rai::pending_info::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, source.bytes);
	rai::write (stream_a, amount.bytes);
}

bool rai::pending_info::deserialize (rai::stream & stream_a)
{
	auto result (rai::read (stream_a, source.bytes));
	if (!result)
	{
		result = rai::read (stream_a, amount.bytes);
	}
	return result;
}

bool rai::pending_info::operator== (rai::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount;
}

rai::mdb_val rai::pending_info::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::pending_info *> (this));
}

rai::pending_key::pending_key (rai::account const & account_a, rai::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

rai::pending_key::pending_key (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (hash) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

void rai::pending_key::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, account.bytes);
	rai::write (stream_a, hash.bytes);
}

bool rai::pending_key::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, account.bytes));
	if (!error)
	{
		error = rai::read (stream_a, hash.bytes);
	}
	return error;
}

bool rai::pending_key::operator== (rai::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

rai::mdb_val rai::pending_key::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::pending_key *> (this));
}

rai::block_info::block_info () :
account (0),
balance (0)
{
}

rai::block_info::block_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (balance) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

rai::block_info::block_info (rai::account const & account_a, rai::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

void rai::block_info::serialize (rai::stream & stream_a) const
{
	rai::write (stream_a, account.bytes);
	rai::write (stream_a, balance.bytes);
}

bool rai::block_info::deserialize (rai::stream & stream_a)
{
	auto error (rai::read (stream_a, account.bytes));
	if (!error)
	{
		error = rai::read (stream_a, balance.bytes);
	}
	return error;
}

bool rai::block_info::operator== (rai::block_info const & other_a) const
{
	return account == other_a.account && balance == other_a.balance;
}

rai::mdb_val rai::block_info::val () const
{
	return rai::mdb_val (sizeof (*this), const_cast<rai::block_info *> (this));
}

bool rai::vote::operator== (rai::vote const & other_a) const
{
	return sequence == other_a.sequence && *block == *other_a.block && account == other_a.account && signature == other_a.signature;
}

bool rai::vote::operator!= (rai::vote const & other_a) const
{
	return !(*this == other_a);
}

std::string rai::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	tree.put ("block", block->to_json ());
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

rai::amount_visitor::amount_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a)
{
}

void rai::amount_visitor::send_block (rai::send_block const & block_a)
{
	balance_visitor prev (transaction, store);
	prev.compute (block_a.hashables.previous);
	result = prev.result - block_a.hashables.balance.number ();
	current = 0;
}

void rai::amount_visitor::receive_block (rai::receive_block const & block_a)
{
	current = block_a.hashables.source;
}

void rai::amount_visitor::open_block (rai::open_block const & block_a)
{
	if (block_a.hashables.source != rai::genesis_account)
	{
		current = block_a.hashables.source;
	}
	else
	{
		result = rai::genesis_amount;
		current = 0;
	}
}

void rai::amount_visitor::state_block (rai::state_block const & block_a)
{
	balance_visitor prev (transaction, store);
	prev.compute (block_a.hashables.previous);
	result = block_a.hashables.balance.number ();
	result = result < prev.result ? prev.result - result : result - prev.result;
	current = 0;
}

void rai::amount_visitor::change_block (rai::change_block const & block_a)
{
	result = 0;
	current = 0;
}

void rai::amount_visitor::compute (rai::block_hash const & block_hash)
{
	current = block_hash;
	while (!current.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		if (block != nullptr)
		{
			block->visit (*this);
		}
		else
		{
			if (block_hash == rai::genesis_account)
			{
				result = std::numeric_limits<rai::uint128_t>::max ();
				current = 0;
			}
			else
			{
				assert (false);
				result = 0;
				current = 0;
			}
		}
	}
}

rai::balance_visitor::balance_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current (0),
result (0)
{
}

void rai::balance_visitor::send_block (rai::send_block const & block_a)
{
	result += block_a.hashables.balance.number ();
	current = 0;
}

void rai::balance_visitor::receive_block (rai::receive_block const & block_a)
{
	rai::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		result += block_info.balance.number ();
		current = 0;
	}
	else
	{
		amount_visitor source (transaction, store);
		source.compute (block_a.hashables.source);
		result += source.result;
		current = block_a.hashables.previous;
	}
}

void rai::balance_visitor::open_block (rai::open_block const & block_a)
{
	amount_visitor source (transaction, store);
	source.compute (block_a.hashables.source);
	result += source.result;
	current = 0;
}

void rai::balance_visitor::change_block (rai::change_block const & block_a)
{
	rai::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		result += block_info.balance.number ();
		current = 0;
	}
	else
	{
		current = block_a.hashables.previous;
	}
}

void rai::balance_visitor::state_block (rai::state_block const & block_a)
{
	result = block_a.hashables.balance.number ();
	current = 0;
}

void rai::balance_visitor::compute (rai::block_hash const & block_hash)
{
	current = block_hash;
	while (!current.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

rai::representative_visitor::representative_visitor (MDB_txn * transaction_a, rai::block_store & store_a) :
transaction (transaction_a),
store (store_a),
result (0)
{
}

void rai::representative_visitor::compute (rai::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

void rai::representative_visitor::send_block (rai::send_block const & block_a)
{
	current = block_a.previous ();
}

void rai::representative_visitor::receive_block (rai::receive_block const & block_a)
{
	current = block_a.previous ();
}

void rai::representative_visitor::open_block (rai::open_block const & block_a)
{
	result = block_a.hash ();
}

void rai::representative_visitor::change_block (rai::change_block const & block_a)
{
	result = block_a.hash ();
}

void rai::representative_visitor::state_block (rai::state_block const & block_a)
{
	result = block_a.hash ();
}

rai::vote::vote (rai::vote const & other_a) :
sequence (other_a.sequence),
block (other_a.block),
account (other_a.account),
signature (other_a.signature)
{
}

rai::vote::vote (bool & error_a, rai::stream & stream_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = rai::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = rai::read (stream_a, sequence);
				if (!error_a)
				{
					block = rai::deserialize_block (stream_a);
					error_a = block == nullptr;
				}
			}
		}
	}
}

rai::vote::vote (bool & error_a, rai::stream & stream_a, rai::block_type type_a)
{
	if (!error_a)
	{
		error_a = rai::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = rai::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = rai::read (stream_a, sequence);
				if (!error_a)
				{
					block = rai::deserialize_block (stream_a, type_a);
					error_a = block == nullptr;
				}
			}
		}
	}
}

rai::vote::vote (rai::account const & account_a, rai::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<rai::block> block_a) :
sequence (sequence_a),
block (block_a),
account (account_a),
signature (rai::sign_message (prv_a, account_a, hash ()))
{
}

rai::vote::vote (MDB_val const & value_a)
{
	rai::bufferstream stream (reinterpret_cast<uint8_t const *> (value_a.mv_data), value_a.mv_size);
	auto error (rai::read (stream, account.bytes));
	assert (!error);
	error = rai::read (stream, signature.bytes);
	assert (!error);
	error = rai::read (stream, sequence);
	assert (!error);
	block = rai::deserialize_block (stream);
	assert (block != nullptr);
}

rai::uint256_union rai::vote::hash () const
{
	rai::uint256_union result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	blake2b_update (&hash, block->hash ().bytes.data (), sizeof (result.bytes));
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

void rai::vote::serialize (rai::stream & stream_a, rai::block_type)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	block->serialize (stream_a);
}

void rai::vote::serialize (rai::stream & stream_a)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	rai::serialize_block (stream_a, *block);
}

rai::genesis::genesis ()
{
	boost::property_tree::ptree tree;
	std::stringstream istream (rai::genesis_block);
	boost::property_tree::read_json (istream, tree);
	auto block (rai::deserialize_block_json (tree));
	assert (dynamic_cast<rai::open_block *> (block.get ()) != nullptr);
	open.reset (static_cast<rai::open_block *> (block.release ()));
}

void rai::genesis::initialize (MDB_txn * transaction_a, rai::block_store & store_a) const
{
	auto hash_l (hash ());
	assert (store_a.latest_begin (transaction_a) == store_a.latest_end ());
	store_a.block_put (transaction_a, hash_l, *open);
	store_a.account_put (transaction_a, genesis_account, { hash_l, open->hash (), open->hash (), std::numeric_limits<rai::uint128_t>::max (), rai::seconds_since_epoch (), 1 });
	store_a.representation_put (transaction_a, genesis_account, std::numeric_limits<rai::uint128_t>::max ());
	store_a.checksum_put (transaction_a, 0, 0, hash_l);
	store_a.frontier_put (transaction_a, hash_l, genesis_account);
}

rai::block_hash rai::genesis::hash () const
{
	return open->hash ();
}
