#include <gtest/gtest.h>

#include <memory>

#include <badem/lib/blocks.hpp>
#include <badem/lib/interface.h>
#include <badem/lib/numbers.hpp>
#include <badem/lib/work.hpp>

TEST (interface, bdm_uint128_to_dec)
{
	badem::uint128_union zero (0);
	char text[40] = { 0 };
	bdm_uint128_to_dec (zero.bytes.data (), text);
	ASSERT_STREQ ("0", text);
}

TEST (interface, bdm_uint256_to_string)
{
	badem::uint256_union zero (0);
	char text[65] = { 0 };
	bdm_uint256_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("0000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, bdm_uint256_to_address)
{
	badem::uint256_union zero (0);
	char text[65] = { 0 };
	bdm_uint256_to_address (zero.bytes.data (), text);
	ASSERT_STREQ ("bdm_1111111111111111111111111111111111111111111111111111hifc8npp", text);
}

TEST (interface, bdm_uint512_to_string)
{
	badem::uint512_union zero (0);
	char text[129] = { 0 };
	bdm_uint512_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, bdm_uint128_from_dec)
{
	badem::uint128_union zero (0);
	ASSERT_EQ (0, bdm_uint128_from_dec ("340282366920938463463374607431768211455", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint128_from_dec ("340282366920938463463374607431768211456", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint128_from_dec ("3402823669209384634633%4607431768211455", zero.bytes.data ()));
}

TEST (interface, bdm_uint256_from_string)
{
	badem::uint256_union zero (0);
	ASSERT_EQ (0, bdm_uint256_from_string ("0000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint256_from_string ("00000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint256_from_string ("000000000000000000000000000%000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, bdm_uint512_from_string)
{
	badem::uint512_union zero (0);
	ASSERT_EQ (0, bdm_uint512_from_string ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint512_from_string ("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, bdm_uint512_from_string ("0000000000000000000000000000000000000000000000000000000000%000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, bdm_valid_address)
{
	ASSERT_EQ (0, bdm_valid_address ("bdm_1111111111111111111111111111111111111111111111111111hifc8npp"));
	ASSERT_EQ (1, bdm_valid_address ("bdm_1111111111111111111111111111111111111111111111111111hifc8nppp"));
	ASSERT_EQ (1, bdm_valid_address ("bdm_1111111211111111111111111111111111111111111111111111hifc8npp"));
}

TEST (interface, bdm_seed_create)
{
	badem::uint256_union seed;
	bdm_generate_random (seed.bytes.data ());
	ASSERT_FALSE (seed.is_zero ());
}

TEST (interface, bdm_seed_key)
{
	badem::uint256_union seed (0);
	badem::uint256_union prv;
	bdm_seed_key (seed.bytes.data (), 0, prv.bytes.data ());
	ASSERT_FALSE (prv.is_zero ());
}

TEST (interface, bdm_key_account)
{
	badem::uint256_union prv (0);
	badem::uint256_union pub;
	bdm_key_account (prv.bytes.data (), pub.bytes.data ());
	ASSERT_FALSE (pub.is_zero ());
}

TEST (interface, sign_transaction)
{
	badem::raw_key key;
	bdm_generate_random (key.data.bytes.data ());
	badem::uint256_union pub;
	bdm_key_account (key.data.bytes.data (), pub.bytes.data ());
	badem::send_block send (0, 0, 0, key, pub, 0);
	ASSERT_FALSE (badem::validate_message (pub, send.hash (), send.signature));
	send.signature.bytes[0] ^= 1;
	ASSERT_TRUE (badem::validate_message (pub, send.hash (), send.signature));
	auto send_json (send.to_json ());
	auto transaction (bdm_sign_transaction (send_json.c_str (), key.data.bytes.data ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (badem::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	auto send1 (dynamic_cast<badem::send_block *> (block.get ()));
	ASSERT_NE (nullptr, send1);
	ASSERT_FALSE (badem::validate_message (pub, send.hash (), send1->signature));
	// Signatures should be non-deterministic
	auto transaction2 (bdm_sign_transaction (send_json.c_str (), key.data.bytes.data ()));
	ASSERT_NE (0, strcmp (transaction, transaction2));
	free (transaction);
	free (transaction2);
}

TEST (interface, fail_sign_transaction)
{
	badem::uint256_union data (0);
	bdm_sign_transaction ("", data.bytes.data ());
}

TEST (interface, work_transaction)
{
	badem::raw_key key;
	bdm_generate_random (key.data.bytes.data ());
	badem::uint256_union pub;
	bdm_key_account (key.data.bytes.data (), pub.bytes.data ());
	badem::send_block send (1, 0, 0, key, pub, 0);
	auto transaction (bdm_work_transaction (send.to_json ().c_str ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (badem::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	ASSERT_FALSE (badem::work_validate (*block));
	free (transaction);
}

TEST (interface, fail_work_transaction)
{
	badem::uint256_union data (0);
	bdm_work_transaction ("");
}
