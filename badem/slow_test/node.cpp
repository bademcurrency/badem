#include <gtest/gtest.h>
#include <badem/node/testing.hpp>

#include <thread>

TEST (system, generate_mass_activity)
{
	badem::system system (24000, 1);
	system.wallet (0)->insert_adhoc (badem::test_genesis_key.prv);
	uint32_t count (20);
	system.generate_mass_activity (count, *system.nodes[0]);
	size_t accounts (0);
	auto transaction (system.nodes[0]->store.tx_begin ());
	for (auto i (system.nodes[0]->store.latest_begin (transaction)), n (system.nodes[0]->store.latest_end ()); i != n; ++i)
	{
		++accounts;
	}
}

TEST (system, generate_mass_activity_long)
{
	badem::system system (24000, 1);
	badem::thread_runner runner (system.io_ctx, system.nodes[0]->config.io_threads);
	system.wallet (0)->insert_adhoc (badem::test_genesis_key.prv);
	uint32_t count (1000000000);
	system.generate_mass_activity (count, *system.nodes[0]);
	size_t accounts (0);
	auto transaction (system.nodes[0]->store.tx_begin ());
	for (auto i (system.nodes[0]->store.latest_begin (transaction)), n (system.nodes[0]->store.latest_end ()); i != n; ++i)
	{
		++accounts;
	}
	system.stop ();
	runner.join ();
}

TEST (system, receive_while_synchronizing)
{
	std::vector<boost::thread> threads;
	{
		badem::system system (24000, 1);
		badem::thread_runner runner (system.io_ctx, system.nodes[0]->config.io_threads);
		system.wallet (0)->insert_adhoc (badem::test_genesis_key.prv);
		uint32_t count (1000);
		system.generate_mass_activity (count, *system.nodes[0]);
		badem::keypair key;
		badem::node_init init1;
		auto node1 (std::make_shared<badem::node> (init1, system.io_ctx, 24001, badem::unique_path (), system.alarm, system.logging, system.work));
		ASSERT_FALSE (init1.error ());
		node1->network.send_keepalive (system.nodes[0]->network.endpoint ());
		auto wallet (node1->wallets.create (1));
		ASSERT_EQ (key.pub, wallet->insert_adhoc (key.prv));
		node1->start ();
		system.nodes.push_back (node1);
		system.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (200), ([&system, &key]() {
			auto hash (system.wallet (0)->send_sync (badem::test_genesis_key.pub, key.pub, system.nodes[0]->config.receive_minimum.number ()));
			auto transaction (system.nodes[0]->store.tx_begin ());
			auto block (system.nodes[0]->store.block_get (transaction, hash));
			std::string block_text;
			block->serialize_json (block_text);
		}));
		while (node1->balance (key.pub).is_zero ())
		{
			system.poll ();
		}
		node1->stop ();
		system.stop ();
		runner.join ();
	}
	for (auto i (threads.begin ()), n (threads.end ()); i != n; ++i)
	{
		i->join ();
	}
}

TEST (ledger, deep_account_compute)
{
	badem::logging logging;
	bool init (false);
	badem::mdb_store store (init, logging, badem::unique_path ());
	ASSERT_FALSE (init);
	badem::stat stats;
	badem::ledger ledger (store, stats);
	badem::genesis genesis;
	auto transaction (store.tx_begin (true));
	store.initialize (transaction, genesis);
	badem::keypair key;
	auto balance (badem::genesis_amount - 1);
	badem::send_block send (genesis.hash (), key.pub, balance, badem::test_genesis_key.prv, badem::test_genesis_key.pub, 0);
	ASSERT_EQ (badem::process_result::progress, ledger.process (transaction, send).code);
	badem::open_block open (send.hash (), badem::test_genesis_key.pub, key.pub, key.prv, key.pub, 0);
	ASSERT_EQ (badem::process_result::progress, ledger.process (transaction, open).code);
	auto sprevious (send.hash ());
	auto rprevious (open.hash ());
	for (auto i (0), n (100000); i != n; ++i)
	{
		balance -= 1;
		badem::send_block send (sprevious, key.pub, balance, badem::test_genesis_key.prv, badem::test_genesis_key.pub, 0);
		ASSERT_EQ (badem::process_result::progress, ledger.process (transaction, send).code);
		sprevious = send.hash ();
		badem::receive_block receive (rprevious, send.hash (), key.prv, key.pub, 0);
		ASSERT_EQ (badem::process_result::progress, ledger.process (transaction, receive).code);
		rprevious = receive.hash ();
		if (i % 100 == 0)
		{
			std::cerr << i << ' ';
		}
		auto account (ledger.account (transaction, sprevious));
		(void)account;
		auto balance (ledger.balance (transaction, rprevious));
		(void)balance;
	}
}

TEST (wallet, multithreaded_send)
{
	std::vector<boost::thread> threads;
	{
		badem::system system (24000, 1);
		badem::keypair key;
		auto wallet_l (system.wallet (0));
		wallet_l->insert_adhoc (badem::test_genesis_key.prv);
		for (auto i (0); i < 20; ++i)
		{
			threads.push_back (boost::thread ([wallet_l, &key]() {
				for (auto i (0); i < 1000; ++i)
				{
					wallet_l->send_action (badem::test_genesis_key.pub, key.pub, 1000);
				}
			}));
		}
		while (system.nodes[0]->balance (badem::test_genesis_key.pub) != (badem::genesis_amount - 20 * 1000 * 1000))
		{
			system.poll ();
		}
	}
	for (auto i (threads.begin ()), n (threads.end ()); i != n; ++i)
	{
		i->join ();
	}
}

TEST (store, load)
{
	badem::system system (24000, 1);
	std::vector<boost::thread> threads;
	for (auto i (0); i < 100; ++i)
	{
		threads.push_back (boost::thread ([&system]() {
			for (auto i (0); i != 1000; ++i)
			{
				auto transaction (system.nodes[0]->store.tx_begin (true));
				for (auto j (0); j != 10; ++j)
				{
					badem::block_hash hash;
					badem::random_pool::generate_block (hash.bytes.data (), hash.bytes.size ());
					system.nodes[0]->store.account_put (transaction, hash, badem::account_info ());
				}
			}
		}));
	}
	for (auto & i : threads)
	{
		i.join ();
	}
}

TEST (node, fork_storm)
{
	badem::system system (24000, 64);
	system.wallet (0)->insert_adhoc (badem::test_genesis_key.prv);
	auto previous (system.nodes[0]->latest (badem::test_genesis_key.pub));
	auto balance (system.nodes[0]->balance (badem::test_genesis_key.pub));
	ASSERT_FALSE (previous.is_zero ());
	for (auto j (0); j != system.nodes.size (); ++j)
	{
		balance -= 1;
		badem::keypair key;
		badem::send_block send (previous, key.pub, balance, badem::test_genesis_key.prv, badem::test_genesis_key.pub, 0);
		previous = send.hash ();
		for (auto i (0); i != system.nodes.size (); ++i)
		{
			auto send_result (system.nodes[i]->process (send));
			ASSERT_EQ (badem::process_result::progress, send_result.code);
			badem::keypair rep;
			auto open (std::make_shared<badem::open_block> (previous, rep.pub, key.pub, key.prv, key.pub, 0));
			system.nodes[i]->work_generate_blocking (*open);
			auto open_result (system.nodes[i]->process (*open));
			ASSERT_EQ (badem::process_result::progress, open_result.code);
			auto transaction (system.nodes[i]->store.tx_begin ());
			system.nodes[i]->network.republish_block (open);
		}
	}
	auto again (true);

	int empty (0);
	int single (0);
	int iteration (0);
	while (again)
	{
		empty = 0;
		single = 0;
		std::for_each (system.nodes.begin (), system.nodes.end (), [&](std::shared_ptr<badem::node> const & node_a) {
			if (node_a->active.empty ())
			{
				++empty;
			}
			else
			{
				if (node_a->active.roots.begin ()->election->last_votes_size () == 1)
				{
					++single;
				}
			}
		});
		system.poll ();
		if ((iteration & 0xff) == 0)
		{
			std::cerr << "Empty: " << empty << " single: " << single << std::endl;
		}
		again = empty != 0 || single != 0;
		++iteration;
	}
	ASSERT_TRUE (true);
}

namespace
{
size_t heard_count (std::vector<uint8_t> const & nodes)
{
	auto result (0);
	for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
	{
		switch (*i)
		{
			case 0:
				break;
			case 1:
				++result;
				break;
			case 2:
				++result;
				break;
		}
	}
	return result;
}
}

TEST (broadcast, world_broadcast_simulate)
{
	auto node_count (10000);
	// 0 = starting state
	// 1 = heard transaction
	// 2 = repeated transaction
	std::vector<uint8_t> nodes;
	nodes.resize (node_count, 0);
	nodes[0] = 1;
	auto any_changed (true);
	auto message_count (0);
	while (any_changed)
	{
		any_changed = false;
		for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
		{
			switch (*i)
			{
				case 0:
					break;
				case 1:
					for (auto j (nodes.begin ()), m (nodes.end ()); j != m; ++j)
					{
						++message_count;
						switch (*j)
						{
							case 0:
								*j = 1;
								any_changed = true;
								break;
							case 1:
								break;
							case 2:
								break;
						}
					}
					*i = 2;
					any_changed = true;
					break;
				case 2:
					break;
				default:
					assert (false);
					break;
			}
		}
	}
	auto count (heard_count (nodes));
	(void)count;
}

TEST (broadcast, sqrt_broadcast_simulate)
{
	auto node_count (200);
	auto broadcast_count (std::ceil (std::sqrt (node_count)));
	// 0 = starting state
	// 1 = heard transaction
	// 2 = repeated transaction
	std::vector<uint8_t> nodes;
	nodes.resize (node_count, 0);
	nodes[0] = 1;
	auto any_changed (true);
	uint64_t message_count (0);
	while (any_changed)
	{
		any_changed = false;
		for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
		{
			switch (*i)
			{
				case 0:
					break;
				case 1:
					for (auto j (0); j != broadcast_count; ++j)
					{
						++message_count;
						auto entry (badem::random_pool::generate_word32 (0, node_count - 1));
						switch (nodes[entry])
						{
							case 0:
								nodes[entry] = 1;
								any_changed = true;
								break;
							case 1:
								break;
							case 2:
								break;
						}
					}
					*i = 2;
					any_changed = true;
					break;
				case 2:
					break;
				default:
					assert (false);
					break;
			}
		}
	}
	auto count (heard_count (nodes));
	(void)count;
}

TEST (peer_container, random_set)
{
	auto loopback (boost::asio::ip::address_v6::loopback ());
	badem::peer_container container (badem::endpoint (loopback, 24000));
	for (auto i (0); i < 200; ++i)
	{
		container.contacted (badem::endpoint (loopback, 24001 + i), 0);
	}
	auto old (std::chrono::steady_clock::now ());
	for (auto i (0); i < 10000; ++i)
	{
		auto list (container.list_fanout ());
	}
	auto current (std::chrono::steady_clock::now ());
	for (auto i (0); i < 10000; ++i)
	{
		auto list (container.random_set (15));
	}
	auto end (std::chrono::steady_clock::now ());
	(void)end;
	auto old_ms (std::chrono::duration_cast<std::chrono::milliseconds> (current - old));
	(void)old_ms;
	auto new_ms (std::chrono::duration_cast<std::chrono::milliseconds> (end - current));
	(void)new_ms;
}

TEST (store, unchecked_load)
{
	badem::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto block (std::make_shared<badem::send_block> (0, 0, 0, badem::test_genesis_key.prv, badem::test_genesis_key.pub, 0));
	for (auto i (0); i < 1000000; ++i)
	{
		auto transaction (node.store.tx_begin (true));
		node.store.unchecked_put (transaction, i, block);
	}
	auto transaction (node.store.tx_begin ());
	auto count (node.store.unchecked_count (transaction));
	(void)count;
}

TEST (store, vote_load)
{
	badem::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto block (std::make_shared<badem::send_block> (0, 0, 0, badem::test_genesis_key.prv, badem::test_genesis_key.pub, 0));
	for (auto i (0); i < 1000000; ++i)
	{
		auto vote (std::make_shared<badem::vote> (badem::test_genesis_key.pub, badem::test_genesis_key.prv, i, block));
		node.vote_processor.vote (vote, system.nodes[0]->network.endpoint ());
	}
}

TEST (wallets, rep_scan)
{
	badem::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto wallet (system.wallet (0));
	{
		auto transaction (node.wallets.tx_begin_write ());
		for (auto i (0); i < 10000; ++i)
		{
			wallet->deterministic_insert (transaction);
		}
	}
	auto transaction (node.store.tx_begin_read ());
	auto begin (std::chrono::steady_clock::now ());
	node.wallets.foreach_representative (transaction, [](badem::public_key const & pub_a, badem::raw_key const & prv_a) {
	});
	ASSERT_LT (std::chrono::steady_clock::now () - begin, std::chrono::milliseconds (5));
}

TEST (node, mass_vote_by_hash)
{
	badem::system system (24000, 1);
	system.wallet (0)->insert_adhoc (badem::test_genesis_key.prv);
	badem::genesis genesis;
	badem::block_hash previous (genesis.hash ());
	badem::keypair key;
	std::vector<std::shared_ptr<badem::state_block>> blocks;
	for (auto i (0); i < 10000; ++i)
	{
		auto block (std::make_shared<badem::state_block> (badem::test_genesis_key.pub, previous, badem::test_genesis_key.pub, badem::genesis_amount - (i + 1) * badem::kBDM_ratio, key.pub, badem::test_genesis_key.prv, badem::test_genesis_key.pub, system.work.generate (previous)));
		previous = block->hash ();
		blocks.push_back (block);
	}
	for (auto i (blocks.begin ()), n (blocks.end ()); i != n; ++i)
	{
		system.nodes[0]->block_processor.add (*i, badem::seconds_since_epoch ());
	}
}
