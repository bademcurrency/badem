#pragma once

#include <badem/lib/blocks.hpp>
#include <badem/node/lmdb.hpp>
#include <badem/secure/utility.hpp>

namespace badem
{
class account_info_v1
{
public:
	account_info_v1 ();
	account_info_v1 (MDB_val const &);
	account_info_v1 (badem::account_info_v1 const &) = default;
	account_info_v1 (badem::block_hash const &, badem::block_hash const &, badem::amount const &, uint64_t);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	badem::mdb_val val () const;
	badem::block_hash head;
	badem::block_hash rep_block;
	badem::amount balance;
	uint64_t modified;
};
class pending_info_v3
{
public:
	pending_info_v3 ();
	pending_info_v3 (MDB_val const &);
	pending_info_v3 (badem::account const &, badem::amount const &, badem::account const &);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	bool operator== (badem::pending_info_v3 const &) const;
	badem::mdb_val val () const;
	badem::account source;
	badem::amount amount;
	badem::account destination;
};
// Latest information about an account
class account_info_v5
{
public:
	account_info_v5 ();
	account_info_v5 (MDB_val const &);
	account_info_v5 (badem::account_info_v5 const &) = default;
	account_info_v5 (badem::block_hash const &, badem::block_hash const &, badem::block_hash const &, badem::amount const &, uint64_t);
	void serialize (badem::stream &) const;
	bool deserialize (badem::stream &);
	badem::mdb_val val () const;
	badem::block_hash head;
	badem::block_hash rep_block;
	badem::block_hash open_block;
	badem::amount balance;
	uint64_t modified;
};
}
