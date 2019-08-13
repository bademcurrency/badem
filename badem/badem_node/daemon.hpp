#include <badem/lib/errors.hpp>
#include <badem/node/node.hpp>
#include <badem/node/rpc.hpp>

namespace badem_daemon
{
class daemon
{
public:
	void run (boost::filesystem::path const &, badem::node_flags const & flags);
};
class daemon_config
{
public:
	daemon_config ();
	badem::error deserialize_json (bool &, badem::jsonconfig &);
	badem::error serialize_json (badem::jsonconfig &);
	/** 
	 * Returns true if an upgrade occurred
	 * @param version The version to upgrade to.
	 * @param config Configuration to upgrade.
	 */
	bool upgrade_json (unsigned version, badem::jsonconfig & config);
	bool rpc_enable;
	badem::rpc_config rpc;
	badem::node_config node;
	bool opencl_enable;
	badem::opencl_config opencl;
	int json_version () const
	{
		return 2;
	}
};
}
