#include <gtest/gtest.h>
#include <badem/node/node.hpp>

TEST (peer_container, empty_peers)
{
	badem::peer_container peers (badem::endpoint{});
	auto list (peers.purge_list (std::chrono::steady_clock::now ()));
	ASSERT_EQ (0, list.size ());
}

TEST (peer_container, no_recontact)
{
	badem::peer_container peers (badem::endpoint{});
	auto observed_peer (0);
	auto observed_disconnect (false);
	badem::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 10000);
	ASSERT_EQ (0, peers.size ());
	peers.peer_observer = [&observed_peer](badem::endpoint const &) { ++observed_peer; };
	peers.disconnect_observer = [&observed_disconnect]() { observed_disconnect = true; };
	ASSERT_FALSE (peers.insert (endpoint1, badem::protocol_version));
	ASSERT_EQ (1, peers.size ());
	ASSERT_TRUE (peers.insert (endpoint1, badem::protocol_version));
	auto remaining (peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (5)));
	ASSERT_TRUE (remaining.empty ());
	ASSERT_EQ (1, observed_peer);
	ASSERT_TRUE (observed_disconnect);
}

TEST (peer_container, no_self_incoming)
{
	badem::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	badem::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, no_self_contacting)
{
	badem::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	badem::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, reserved_peers_no_contact)
{
	badem::peer_container peers (badem::endpoint{});
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0x00000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc0000201)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc6336401)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xcb007101)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xe9fc0001)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xf0000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xffffffff)), 10000), 0));
	ASSERT_EQ (0, peers.size ());
}

TEST (peer_container, split)
{
	badem::peer_container peers (badem::endpoint{});
	auto now (std::chrono::steady_clock::now ());
	badem::endpoint endpoint1 (boost::asio::ip::address_v6::any (), 100);
	badem::endpoint endpoint2 (boost::asio::ip::address_v6::any (), 101);
	peers.peers.insert (badem::peer_information (endpoint1, now - std::chrono::seconds (1), now));
	peers.peers.insert (badem::peer_information (endpoint2, now + std::chrono::seconds (1), now));
	ASSERT_EQ (2, peers.peers.size ());
	auto list (peers.purge_list (now));
	ASSERT_EQ (1, peers.peers.size ());
	ASSERT_EQ (1, list.size ());
	ASSERT_EQ (endpoint2, list[0].endpoint);
}

TEST (peer_container, fill_random_clear)
{
	badem::peer_container peers (badem::endpoint{});
	std::array<badem::endpoint, 8> target;
	std::fill (target.begin (), target.end (), badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::all_of (target.begin (), target.end (), [](badem::endpoint const & endpoint_a) { return endpoint_a == badem::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, fill_random_full)
{
	badem::peer_container peers (badem::endpoint{});
	for (auto i (0); i < 100; ++i)
	{
		peers.insert (badem::endpoint (boost::asio::ip::address_v6::loopback (), i), 0);
	}
	std::array<badem::endpoint, 8> target;
	std::fill (target.begin (), target.end (), badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.end (), [](badem::endpoint const & endpoint_a) { return endpoint_a == badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
}

TEST (peer_container, fill_random_part)
{
	badem::peer_container peers (badem::endpoint{});
	std::array<badem::endpoint, 8> target;
	auto half (target.size () / 2);
	for (auto i (0); i < half; ++i)
	{
		peers.insert (badem::endpoint (boost::asio::ip::address_v6::loopback (), i + 1), 0);
	}
	std::fill (target.begin (), target.end (), badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](badem::endpoint const & endpoint_a) { return endpoint_a == badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](badem::endpoint const & endpoint_a) { return endpoint_a == badem::endpoint (boost::asio::ip::address_v6::loopback (), 0); }));
	ASSERT_TRUE (std::all_of (target.begin () + half, target.end (), [](badem::endpoint const & endpoint_a) { return endpoint_a == badem::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, list_fanout)
{
	badem::peer_container peers (badem::endpoint{});
	auto list1 (peers.list_fanout ());
	ASSERT_TRUE (list1.empty ());
	for (auto i (0); i < 1000; ++i)
	{
		ASSERT_FALSE (peers.insert (badem::endpoint (boost::asio::ip::address_v6::loopback (), 10000 + i), badem::protocol_version));
	}
	auto list2 (peers.list_fanout ());
	ASSERT_EQ (32, list2.size ());
}

TEST (peer_container, rep_weight)
{
	badem::peer_container peers (badem::endpoint{});
	peers.insert (badem::endpoint (boost::asio::ip::address_v6::loopback (), 24001), 0);
	ASSERT_TRUE (peers.representatives (1).empty ());
	badem::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	badem::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24002);
	badem::endpoint endpoint2 (boost::asio::ip::address_v6::loopback (), 24003);
	badem::amount amount (100);
	peers.insert (endpoint2, badem::protocol_version);
	peers.insert (endpoint0, badem::protocol_version);
	peers.insert (endpoint1, badem::protocol_version);
	badem::keypair keypair;
	peers.rep_response (endpoint0, keypair.pub, amount);
	auto reps (peers.representatives (1));
	ASSERT_EQ (1, reps.size ());
	ASSERT_EQ (100, reps[0].rep_weight.number ());
	ASSERT_EQ (keypair.pub, reps[0].probable_rep_account);
	ASSERT_EQ (endpoint0, reps[0].endpoint);
}

// Test to make sure we don't repeatedly send keepalive messages to nodes that aren't responding
TEST (peer_container, reachout)
{
	badem::peer_container peers (badem::endpoint{});
	badem::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	// Make sure having been contacted by them already indicates we shouldn't reach out
	peers.insert (endpoint0, badem::protocol_version);
	ASSERT_TRUE (peers.reachout (endpoint0));
	badem::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24001);
	ASSERT_FALSE (peers.reachout (endpoint1));
	// Reaching out to them once should signal we shouldn't reach out again.
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we don't purge new items
	peers.purge_list (std::chrono::steady_clock::now () - std::chrono::seconds (10));
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we purge old items
	peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (10));
	ASSERT_FALSE (peers.reachout (endpoint1));
}

TEST (peer_container, depeer)
{
	badem::peer_container peers (badem::endpoint{});
	badem::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	peers.contacted (endpoint0, badem::protocol_version_min - 1);
	ASSERT_EQ (0, peers.size ());
}
