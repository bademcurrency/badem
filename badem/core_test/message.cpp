#include <gtest/gtest.h>

#include <badem/node/common.hpp>

TEST (message, keepalive_serialization)
{
	badem::keepalive request1;
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	badem::bufferstream stream (bytes.data (), bytes.size ());
	badem::message_header header (error, stream);
	ASSERT_FALSE (error);
	badem::keepalive request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (message, keepalive_deserialize)
{
	badem::keepalive message1;
	message1.peers[0] = badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		message1.serialize (stream);
	}
	badem::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	badem::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (badem::message_type::keepalive, header.type);
	badem::keepalive message2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (message1.peers, message2.peers);
}

TEST (message, publish_serialization)
{
	badem::publish publish (std::make_shared<badem::send_block> (0, 1, 2, badem::keypair ().prv, 4, 5));
	ASSERT_EQ (badem::block_type::send, publish.header.block_type ());
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream (bytes);
		publish.header.serialize (stream);
	}
	ASSERT_EQ (8, bytes.size ());
	ASSERT_EQ (0x52, bytes[0]);
	ASSERT_EQ (0x41, bytes[1]);
	ASSERT_EQ (badem::protocol_version, bytes[2]);
	ASSERT_EQ (badem::protocol_version, bytes[3]);
	ASSERT_EQ (badem::protocol_version_min, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (badem::message_type::publish), bytes[5]);
	ASSERT_EQ (0x00, bytes[6]); // extensions
	ASSERT_EQ (static_cast<uint8_t> (badem::block_type::send), bytes[7]);
	badem::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	badem::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (badem::protocol_version_min, header.version_min);
	ASSERT_EQ (badem::protocol_version, header.version_using);
	ASSERT_EQ (badem::protocol_version, header.version_max);
	ASSERT_EQ (badem::message_type::publish, header.type);
}

TEST (message, confirm_ack_serialization)
{
	badem::keypair key1;
	auto vote (std::make_shared<badem::vote> (key1.pub, key1.prv, 0, std::make_shared<badem::send_block> (0, 1, 2, key1.prv, 4, 5)));
	badem::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		badem::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	badem::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	badem::message_header header (error, stream2);
	badem::confirm_ack con2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (con1, con2);
}
