#include <boost/property_tree/json_parser.hpp>

#include <fstream>

#include <gtest/gtest.h>
#include <badem/core_test/testutil.hpp>

#include <badem/lib/interface.h>
#include <badem/node/common.hpp>
#include <badem/node/node.hpp>

#include <crypto/ed25519-donna/ed25519.h>

TEST (ed25519, signing)
{
	badem::uint256_union prv (0);
	badem::uint256_union pub (badem::pub_key (prv));
	badem::uint256_union message (0);
	badem::uint512_union signature;
	ed25519_sign (message.bytes.data (), sizeof (message.bytes), prv.bytes.data (), pub.bytes.data (), signature.bytes.data ());
	auto valid1 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
	ASSERT_EQ (0, valid1);
	signature.bytes[32] ^= 0x1;
	auto valid2 (ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), pub.bytes.data (), signature.bytes.data ()));
	ASSERT_NE (0, valid2);
}

TEST (transaction_block, empty)
{
	badem::keypair key1;
	badem::send_block block (0, 1, 13, key1.prv, key1.pub, 2);
	badem::uint256_union hash (block.hash ());
	ASSERT_FALSE (badem::validate_message (key1.pub, hash, block.signature));
	block.signature.bytes[32] ^= 0x1;
	ASSERT_TRUE (badem::validate_message (key1.pub, hash, block.signature));
}

TEST (block, send_serialize)
{
	badem::send_block block1 (0, 1, 2, badem::keypair ().prv, 4, 5);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	auto data (bytes.data ());
	auto size (bytes.size ());
	ASSERT_NE (nullptr, data);
	ASSERT_NE (0, size);
	badem::bufferstream stream2 (data, size);
	bool error (false);
	badem::send_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, send_serialize_json)
{
	badem::send_block block1 (0, 1, 2, badem::keypair ().prv, 4, 5);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	badem::send_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, receive_serialize)
{
	badem::receive_block block1 (0, 1, badem::keypair ().prv, 3, 4);
	badem::keypair key1;
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	badem::receive_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, receive_serialize_json)
{
	badem::receive_block block1 (0, 1, badem::keypair ().prv, 3, 4);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	badem::receive_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, open_serialize_json)
{
	badem::open_block block1 (0, 1, 0, badem::keypair ().prv, 0, 0);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	badem::open_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (block, change_serialize_json)
{
	badem::change_block block1 (0, 1, badem::keypair ().prv, 3, 4);
	std::string string1;
	block1.serialize_json (string1);
	ASSERT_NE (0, string1.size ());
	boost::property_tree::ptree tree1;
	std::stringstream istream (string1);
	boost::property_tree::read_json (istream, tree1);
	bool error (false);
	badem::change_block block2 (error, tree1);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (uint512_union, parse_zero)
{
	badem::uint512_union input (badem::uint512_t (0));
	std::string text;
	input.encode_hex (text);
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_zero_short)
{
	std::string text ("0");
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint512_union, parse_one)
{
	badem::uint512_union input (badem::uint512_t (1));
	std::string text;
	input.encode_hex (text);
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_EQ (1, output.number ());
}

TEST (uint512_union, parse_error_symbol)
{
	badem::uint512_union input (badem::uint512_t (1000));
	std::string text;
	input.encode_hex (text);
	text[5] = '!';
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_TRUE (error);
}

TEST (uint512_union, max)
{
	badem::uint512_union input (std::numeric_limits<badem::uint512_t>::max ());
	std::string text;
	input.encode_hex (text);
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_FALSE (error);
	ASSERT_EQ (input, output);
	ASSERT_EQ (badem::uint512_t ("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), output.number ());
}

TEST (uint512_union, parse_error_overflow)
{
	badem::uint512_union input (std::numeric_limits<badem::uint512_t>::max ());
	std::string text;
	input.encode_hex (text);
	text.push_back (0);
	badem::uint512_union output;
	auto error (output.decode_hex (text));
	ASSERT_TRUE (error);
}

TEST (send_block, deserialize)
{
	badem::send_block block1 (0, 1, 2, badem::keypair ().prv, 4, 5);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (badem::send_block::size, bytes.size ());
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	badem::send_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (receive_block, deserialize)
{
	badem::receive_block block1 (0, 1, badem::keypair ().prv, 3, 4);
	ASSERT_EQ (block1.hash (), block1.hash ());
	block1.hashables.previous = 2;
	block1.hashables.source = 4;
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (badem::receive_block::size, bytes.size ());
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	badem::receive_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (open_block, deserialize)
{
	badem::open_block block1 (0, 1, 0, badem::keypair ().prv, 0, 0);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		block1.serialize (stream);
	}
	ASSERT_EQ (badem::open_block::size, bytes.size ());
	badem::bufferstream stream (bytes.data (), bytes.size ());
	bool error (false);
	badem::open_block block2 (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (change_block, deserialize)
{
	badem::change_block block1 (1, 2, badem::keypair ().prv, 4, 5);
	ASSERT_EQ (block1.hash (), block1.hash ());
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		block1.serialize (stream1);
	}
	ASSERT_EQ (badem::change_block::size, bytes.size ());
	auto data (bytes.data ());
	auto size (bytes.size ());
	ASSERT_NE (nullptr, data);
	ASSERT_NE (0, size);
	badem::bufferstream stream2 (data, size);
	bool error (false);
	badem::change_block block2 (error, stream2);
	ASSERT_FALSE (error);
	ASSERT_EQ (block1, block2);
}

TEST (frontier_req, serialization)
{
	badem::frontier_req request1;
	request1.start = 1;
	request1.age = 2;
	request1.count = 3;
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream (bytes.data (), bytes.size ());
	badem::message_header header (error, stream);
	ASSERT_FALSE (error);
	badem::frontier_req request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (block, publish_req_serialization)
{
	badem::keypair key1;
	badem::keypair key2;
	auto block (std::make_shared<badem::send_block> (0, key2.pub, 200, badem::keypair ().prv, 2, 3));
	badem::publish req (block);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	badem::message_header header (error, stream2);
	ASSERT_FALSE (error);
	badem::publish req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (*req.block, *req2.block);
}

TEST (block, confirm_req_serialization)
{
	badem::keypair key1;
	badem::keypair key2;
	auto block (std::make_shared<badem::send_block> (0, key2.pub, 200, badem::keypair ().prv, 2, 3));
	badem::confirm_req req (block);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	badem::message_header header (error, stream2);
	badem::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (*req.block, *req2.block);
}

TEST (block, confirm_req_hash_serialization)
{
	badem::keypair key1;
	badem::keypair key2;
	badem::send_block block (1, key2.pub, 200, badem::keypair ().prv, 2, 3);
	badem::confirm_req req (block.hash (), block.root ());
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	badem::message_header header (error, stream2);
	badem::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (req.roots_hashes, req2.roots_hashes);
}

TEST (block, confirm_req_hash_batch_serialization)
{
	badem::keypair key;
	badem::keypair representative;
	std::vector<std::pair<badem::block_hash, badem::block_hash>> roots_hashes;
	badem::state_block open (key.pub, 0, representative.pub, 2, 4, key.prv, key.pub, 5);
	roots_hashes.push_back (std::make_pair (open.hash (), open.root ()));
	for (auto i (roots_hashes.size ()); i < 7; i++)
	{
		badem::keypair key1;
		badem::keypair previous;
		badem::state_block block (key1.pub, previous.pub, representative.pub, 2, 4, key1.prv, key1.pub, 5);
		roots_hashes.push_back (std::make_pair (block.hash (), block.root ()));
	}
	roots_hashes.push_back (std::make_pair (open.hash (), open.root ()));
	badem::confirm_req req (roots_hashes);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	badem::message_header header (error, stream2);
	badem::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (req.roots_hashes, req2.roots_hashes);
	ASSERT_EQ (req.roots_hashes, roots_hashes);
	ASSERT_EQ (req2.roots_hashes, roots_hashes);
}

TEST (state_block, serialization)
{
	badem::keypair key1;
	badem::keypair key2;
	badem::state_block block1 (key1.pub, 1, key2.pub, 2, 4, key1.prv, key1.pub, 5);
	ASSERT_EQ (key1.pub, block1.hashables.account);
	ASSERT_EQ (badem::block_hash (1), block1.previous ());
	ASSERT_EQ (key2.pub, block1.hashables.representative);
	ASSERT_EQ (badem::amount (2), block1.hashables.balance);
	ASSERT_EQ (badem::uint256_union (4), block1.hashables.link);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		block1.serialize (stream);
	}
	ASSERT_EQ (0x5, bytes[215]); // Ensure work is serialized big-endian
	ASSERT_EQ (badem::state_block::size, bytes.size ());
	bool error1 (false);
	badem::bufferstream stream (bytes.data (), bytes.size ());
	badem::state_block block2 (error1, stream);
	ASSERT_FALSE (error1);
	ASSERT_EQ (block1, block2);
	block2.hashables.account.clear ();
	block2.hashables.previous.clear ();
	block2.hashables.representative.clear ();
	block2.hashables.balance.clear ();
	block2.hashables.link.clear ();
	block2.signature.clear ();
	block2.work = 0;
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	ASSERT_FALSE (block2.deserialize (stream2));
	ASSERT_EQ (block1, block2);
	std::string json;
	block1.serialize_json (json);
	std::stringstream body (json);
	boost::property_tree::ptree tree;
	boost::property_tree::read_json (body, tree);
	bool error2 (false);
	badem::state_block block3 (error2, tree);
	ASSERT_FALSE (error2);
	ASSERT_EQ (block1, block3);
	block3.hashables.account.clear ();
	block3.hashables.previous.clear ();
	block3.hashables.representative.clear ();
	block3.hashables.balance.clear ();
	block3.hashables.link.clear ();
	block3.signature.clear ();
	block3.work = 0;
	ASSERT_FALSE (block3.deserialize_json (tree));
	ASSERT_EQ (block1, block3);
}

TEST (state_block, hashing)
{
	badem::keypair key;
	badem::state_block block (key.pub, 0, key.pub, 0, 0, key.prv, key.pub, 0);
	auto hash (block.hash ());
	block.hashables.account.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.account.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.previous.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.previous.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.representative.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.representative.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.balance.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.balance.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
	block.hashables.link.bytes[0] ^= 0x1;
	ASSERT_NE (hash, block.hash ());
	block.hashables.link.bytes[0] ^= 0x1;
	ASSERT_EQ (hash, block.hash ());
}

TEST (block_uniquer, null)
{
	badem::block_uniquer uniquer;
	ASSERT_EQ (nullptr, uniquer.unique (nullptr));
}

TEST (block_uniquer, single)
{
	badem::keypair key;
	auto block1 (std::make_shared<badem::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0));
	auto block2 (std::make_shared<badem::state_block> (*block1));
	ASSERT_NE (block1, block2);
	ASSERT_EQ (*block1, *block2);
	std::weak_ptr<badem::state_block> block3 (block2);
	ASSERT_NE (nullptr, block3.lock ());
	badem::block_uniquer uniquer;
	auto block4 (uniquer.unique (block1));
	ASSERT_EQ (block1, block4);
	auto block5 (uniquer.unique (block2));
	ASSERT_EQ (block1, block5);
	block2.reset ();
	ASSERT_EQ (nullptr, block3.lock ());
}

TEST (block_uniquer, cleanup)
{
	badem::keypair key;
	auto block1 (std::make_shared<badem::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0));
	auto block2 (std::make_shared<badem::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 1));
	badem::block_uniquer uniquer;
	auto block3 (uniquer.unique (block1));
	auto block4 (uniquer.unique (block2));
	block2.reset ();
	block4.reset ();
	ASSERT_EQ (2, uniquer.size ());
	auto iterations (0);
	while (uniquer.size () == 2)
	{
		auto block5 (uniquer.unique (block1));
		ASSERT_LT (iterations++, 200);
	}
}

TEST (block_builder, zeroed_state_block)
{
	std::error_code ec;
	badem::block_builder builder;
	badem::keypair key;
	// Make sure manually- and builder constructed all-zero blocks have equal hashes, and check signature.
	auto zero_block_manual (std::make_shared<badem::state_block> (0, 0, 0, 0, 0, key.prv, key.pub, 0));
	auto zero_block_build = builder.state ().zero ().sign (key.prv, key.pub).build ();
	ASSERT_TRUE (zero_block_manual->hash () == zero_block_build->hash ());
	ASSERT_FALSE (badem::validate_message (key.pub, zero_block_build->hash (), zero_block_build->signature));
}

TEST (block_builder, state)
{
	// Test against a random hash from the live network
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .state ()
	             .account_address ("bdm_38regm4qcthdwm9w5urp7o6cy6tj5cp14ekjhoptocp74nxinsd9bdhe3adu")
	             .previous_hex ("FEFBCE274E75148AB31FF63EFB3082EF1126BF72BF3FA9C76A97FD5A9F0EBEC5")
	             .balance_dec ("2251569974100400000000000000000000")
	             .representative_address ("bdm_3uuw7nzrnbp5d86ze4pefky1efq736kr94dbt4bnfszpwrxs7c7rezwqystt")
	             .link_hex ("E16DD58C1EFA8B521545B0A74375AA994D9FC43828A4266D75ECF57F07A7EE86")
	             .build (ec);
	ASSERT_EQ (block->hash ().to_string (), "2D243F8F92CDD0AD94A1D456A6B15F3BE7A6FCBD98D4C5831D06D15C818CD81F");
}

TEST (block_builder, state_missing_rep)
{
	// Test against a random hash from the live network
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .state ()
	             .account_address ("bdm_38regm4qcthdwm9w5urp7o6cy6tj5cp14ekjhoptocp74nxinsd9bdhe3adu")
	             .previous_hex ("FEFBCE274E75148AB31FF63EFB3082EF1126BF72BF3FA9C76A97FD5A9F0EBEC5")
	             .balance_dec ("2251569974100400000000000000000000")
	             .link_hex ("E16DD58C1EFA8B521545B0A74375AA994D9FC43828A4266D75ECF57F07A7EE86")
	             .sign_zero ()
	             .work (0)
	             .build (ec);
	ASSERT_EQ (ec, badem::error_common::missing_representative);
}

TEST (block_builder, state_equality)
{
	std::error_code ec;
	badem::block_builder builder;

	// With constructor
	badem::keypair key1, key2;
	badem::state_block block1 (key1.pub, 1, key2.pub, 2, 4, key1.prv, key1.pub, 5);

	// With builder
	auto block2 = builder
	              .state ()
	              .account (key1.pub)
	              .previous (1)
	              .representative (key2.pub)
	              .balance (2)
	              .link (4)
	              .sign (key1.prv, key1.pub)
	              .work (5)
	              .build (ec);

	ASSERT_NO_ERROR (ec);
	ASSERT_EQ (block1.hash (), block2->hash ());
	ASSERT_EQ (block1.work, block2->work);
}

TEST (block_builder, state_errors)
{
	std::error_code ec;
	badem::block_builder builder;

	// Ensure the proper error is generated
	builder.state ().account_hex ("bdm_bad").build (ec);
	ASSERT_EQ (ec, badem::error_common::bad_account_number);

	builder.state ().zero ().account_address ("bdm_1111111111111111111111111111111111111111111111111111hifc8npp").build (ec);
	ASSERT_NO_ERROR (ec);
}

TEST (block_builder, open)
{
	// Test built block's hash against the Genesis open block from the live network
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .open ()
	             .account_address ("bdm_1i8aw9e8hpjjfub61m6cf7j6zz9zj37iopxkffehha84hi91akjn1n9s51fg")
	             .representative_address ("bdm_1i8aw9e8hpjjfub61m6cf7j6zz9zj37iopxkffehha84hi91akjn1n9s51fg")
	             .source_hex ("E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA")
	             .build (ec);
	ASSERT_EQ (block->hash ().to_string (), "991CF190094C00F0B68E2E5F75F6BEE95A2E0BD93CEAA4A6734DB9F19B728948");
}

TEST (block_builder, open_equality)
{
	std::error_code ec;
	badem::block_builder builder;

	// With constructor
	badem::keypair key1, key2;
	badem::open_block block1 (1, key1.pub, key2.pub, key1.prv, key1.pub, 5);

	// With builder
	auto block2 = builder
	              .open ()
	              .source (1)
	              .account (key2.pub)
	              .representative (key1.pub)
	              .sign (key1.prv, key1.pub)
	              .work (5)
	              .build (ec);

	ASSERT_NO_ERROR (ec);
	ASSERT_EQ (block1.hash (), block2->hash ());
	ASSERT_EQ (block1.work, block2->work);
}

TEST (block_builder, change)
{
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .change ()
	             .representative_address ("bdm_38regm4qcthdwm9w5urp7o6cy6tj5cp14ekjhoptocp74nxinsd9bdhe3adu")
	             .previous_hex ("088EE46429CA936F76C4EAA20B97F6D33E5D872971433EE0C1311BCB98764456")
	             .build (ec);
	ASSERT_EQ (block->hash ().to_string (), "13552AC3928E93B5C6C215F61879358E248D4A5246B8B3D1EEC5A566EDCEE077");
}

TEST (block_builder, change_equality)
{
	std::error_code ec;
	badem::block_builder builder;

	// With constructor
	badem::keypair key1, key2;
	badem::change_block block1 (1, key1.pub, key1.prv, key1.pub, 5);

	// With builder
	auto block2 = builder
	              .change ()
	              .previous (1)
	              .representative (key1.pub)
	              .sign (key1.prv, key1.pub)
	              .work (5)
	              .build (ec);

	ASSERT_NO_ERROR (ec);
	ASSERT_EQ (block1.hash (), block2->hash ());
	ASSERT_EQ (block1.work, block2->work);
}

TEST (block_builder, send)
{
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .send ()
	             .destination_address ("bdm_3uuw7nzrnbp5d86ze4pefky1efq736kr94dbt4bnfszpwrxs7c7rezwqystt")
	             .previous_hex ("F685856D73A488894F7F3A62BC3A88E17E985F9969629FF3FDD4A0D4FD823F24")
	             .balance_hex ("00F035A9C7D818E7C34148C524FFFFEE")
	             .build (ec);
	ASSERT_EQ (block->hash ().to_string (), "4560E7B1F3735D082700CFC2852F5D1F378F7418FD24CEF1AD45AB69316F15CD");
}

TEST (block_builder, send_equality)
{
	std::error_code ec;
	badem::block_builder builder;

	// With constructor
	badem::keypair key1, key2;
	badem::send_block block1 (1, key1.pub, 2, key1.prv, key1.pub, 5);

	// With builder
	auto block2 = builder
	              .send ()
	              .previous (1)
	              .destination (key1.pub)
	              .balance (2)
	              .sign (key1.prv, key1.pub)
	              .work (5)
	              .build (ec);

	ASSERT_NO_ERROR (ec);
	ASSERT_EQ (block1.hash (), block2->hash ());
	ASSERT_EQ (block1.work, block2->work);
}

TEST (block_builder, receive_equality)
{
	std::error_code ec;
	badem::block_builder builder;

	// With constructor
	badem::keypair key1;
	badem::receive_block block1 (1, 2, key1.prv, key1.pub, 5);

	// With builder
	auto block2 = builder
	              .receive ()
	              .previous (1)
	              .source (2)
	              .sign (key1.prv, key1.pub)
	              .work (5)
	              .build (ec);

	ASSERT_NO_ERROR (ec);
	ASSERT_EQ (block1.hash (), block2->hash ());
	ASSERT_EQ (block1.work, block2->work);
}

TEST (block_builder, receive)
{
	std::error_code ec;
	badem::block_builder builder;
	auto block = builder
	             .receive ()
	             .previous_hex ("59660153194CAC5DAC08509D87970BF86F6AEA943025E2A7ED7460930594950E")
	             .source_hex ("7B2B0A29C1B235FDF9B4DEF2984BB3573BD1A52D28246396FBB3E4C5FE662135")
	             .build (ec);
	ASSERT_EQ (block->hash ().to_string (), "6C004BF911D9CF2ED75CF6EC45E795122AD5D093FF5A83EDFBA43EC4A3EDC722");
}
