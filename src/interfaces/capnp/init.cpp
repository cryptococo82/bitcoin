#include <init.h>
#include <interfaces/capnp/init-types.h>
#include <rpc/util.h>

namespace mp {

std::unique_ptr<interfaces::ChainClient>
ProxyServerMethodTraits<interfaces::capnp::messages::Init::MakeWalletClientParams>::invoke(Context& context,
    std::vector<std::string> wallet_filenames)
{
    // FIXME need to initialize LogInstance() members, adding them to implicitly passed context.

    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInitSanityChecks
    RandomInit();
    ECC_Start();
    static ECCVerifyHandle globalVerifyHandle;
    // TODO in future PR: Refactor bitcoin startup code, dedup this with InitSanityCheck
    if (!ECC_InitSanityCheck()) {
        throw std::runtime_error("Elliptic curve cryptography sanity check failure. Aborting.");
    }
    if (!Random_SanityCheck()) {
        throw std::runtime_error("OS cryptographic RNG sanity check failure. Aborting.");
    }
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInit.
    SelectParams(interfaces::capnp::GlobalArgsNetwork());
    // TODO in future PR: Maybe add AppInitBasicSetup signal handler calls
    // TODO in future PR: Refactor bitcoin startup code, dedup this with AppInitMain.
    if (!LogInstance().StartLogging()) {
        throw std::runtime_error("Could not open wallet debug log file");
    }

    auto params = context.call_context.getParams();
    auto interfaces = MakeUnique<InitInterfaces>();
    g_rpc_interfaces = interfaces.get();
    interfaces->chain = MakeUnique<ProxyClient<interfaces::capnp::messages::Chain>>(
        params.getChain(), *context.proxy_server.m_connection);
    auto client = context.proxy_server.m_impl->makeWalletClient(*interfaces->chain, std::move(wallet_filenames));
    client->addCloseHook(MakeUnique<interfaces::Deleter<std::unique_ptr<InitInterfaces>>>(std::move(interfaces)));
    return client;
}

} // namespace mp
