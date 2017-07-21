#ifndef BITCOIN_IPC_INTERFACES_H
#define BITCOIN_IPC_INTERFACES_H

#include <memory>
#include <vector>

class CScheduler;

namespace ipc {

//! Interface for giving wallet processes access to blockchain state.
class Chain
{
public:
    virtual ~Chain() {}

    //! Interface to let node manage chain clients (wallets, or maybe tools for
    //! monitoring and analysis in the future).
    class Client
    {
    public:
        virtual ~Client() {}

        //! Register rpcs.
        virtual void registerRpcs() = 0;

        //! Prepare for execution, loading any needed state.
        virtual bool prepare() = 0;

        //! Start client execution and provide a scheduler. (Scheduler is
        //! ignored if client is out-of-process).
        virtual void start(CScheduler& scheduler) = 0;

        //! Stop client execution and prepare for shutdown.
        virtual void stop() = 0;

        //! Shut down client.
        virtual void shutdown() = 0;
    };

    //! List of clients.
    using Clients = std::vector<std::unique_ptr<Client>>;
};

//! Protocol IPC interface should use to communicate with implementation.
enum Protocol {
    LOCAL, //!< Call functions linked into current executable.
};

//! Create IPC chain interface, communicating with requested protocol. Returns
//! null if protocol isn't implemented or is not available in the current build
//! configuration.
std::unique_ptr<Chain> MakeChain(Protocol protocol);

//! Chain client creation options.
struct ChainClientOptions
{
    //! Type of IPC chain client. Currently wallet processes are the only
    //! clients. In the future other types of client processes could be added
    //! (tools for monitoring, analysis, fee estimation, etc).
    enum Type { WALLET = 0 };
    Type type;

    //! For WALLET client, wallet filenames to load.
    std::vector<std::string> wallet_filenames;
};

//! Create chain client interface, communicating with requested protocol.
//! Returns null if protocol or client type aren't implemented or available in
//! the current build configuration.
std::unique_ptr<Chain::Client> MakeChainClient(Protocol protocol, Chain& chain, ChainClientOptions options);

//! Convenience function to return options object for wallet clients.
inline ChainClientOptions WalletOptions(std::vector<std::string> wallet_filenames = {})
{
    ChainClientOptions options;
    options.type = ChainClientOptions::WALLET;
    options.wallet_filenames = std::move(wallet_filenames);
    return options;
}

} // namespace ipc

#endif // BITCOIN_IPC_INTERFACES_H
