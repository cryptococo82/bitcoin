#ifndef BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H
#define BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H

#include <interfaces/capnp/common-types.h>

//! Specialization of handleNotifications needed because it takes a
//! Notifications& reference argument, not a unique_ptr<Notifications> argument,
//! so a manual addCloseHook() callback is needed to clean up the
//! ProxyServer<interfaces::capnp::messages::ChainNotifications> proxy object.
template <>
struct mp::ProxyServerMethodTraits<interfaces::capnp::messages::Chain::HandleNotificationsParams>
{
    using Context = ServerContext<interfaces::capnp::messages::Chain,
        interfaces::capnp::messages::Chain::HandleNotificationsParams,
        interfaces::capnp::messages::Chain::HandleNotificationsResults>;
    static std::unique_ptr<interfaces::Handler> invoke(Context& context);
};

//! Specialization of requestMempoolTransactions needed because it takes a
//! Notifications& reference argument, not a unique_ptr<Notifications> argument,
//! so a manual deletion is needed to clean up the
//! ProxyServer<interfaces::capnp::messages::ChainNotifications> proxy object.
template <>
struct mp::ProxyServerMethodTraits<interfaces::capnp::messages::Chain::RequestMempoolTransactionsParams>
{
    using Context = ServerContext<interfaces::capnp::messages::Chain,
        interfaces::capnp::messages::Chain::RequestMempoolTransactionsParams,
        interfaces::capnp::messages::Chain::RequestMempoolTransactionsResults>;
    static void invoke(Context& context);
};

//! Specialization of handleRpc needed because it takes a CRPCCommand& reference
//! argument so a manual addCloseHook() callback is needed to free the passed
//! CRPCCommand struct and proxy ActorCallback object.
template <>
struct mp::ProxyServerMethodTraits<interfaces::capnp::messages::Chain::HandleRpcParams>
{
    using Context = ServerContext<interfaces::capnp::messages::Chain,
        interfaces::capnp::messages::Chain::HandleRpcParams,
        interfaces::capnp::messages::Chain::HandleRpcResults>;
    static std::unique_ptr<interfaces::Handler> invoke(Context& context);
};

//! Specialization of start method needed to provide CScheduler& reference
//! argument.
template <>
struct mp::ProxyServerMethodTraits<interfaces::capnp::messages::ChainClient::StartParams>
{
    using Context = ServerContext<interfaces::capnp::messages::ChainClient,
        interfaces::capnp::messages::ChainClient::StartParams,
        interfaces::capnp::messages::ChainClient::StartResults>;
    static void invoke(Context& context);
};

namespace mp {

//! CScheduler& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<CScheduler&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! CRPCCommand& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<const CRPCCommand&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! Chain::Notifications& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<interfaces::Chain::Notifications&>,
    ServerContext& server_context,
    const Fn& fn,
    Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

//! Chain& server-side argument handling. Skips argument so it can
//! be handled by ProxyServerCustom code.
template <typename Accessor, typename ServerContext, typename Fn, typename... Args>
void CustomPassField(TypeList<interfaces::Chain&>, ServerContext& server_context, const Fn& fn, Args&&... args)
{
    fn.invoke(server_context, std::forward<Args>(args)...);
}

void CustomBuildMessage(InvokeContext& invoke_context,
    CValidationState const& state,
    interfaces::capnp::messages::ValidationState::Builder&& builder);
void CustomReadMessage(InvokeContext& invoke_context,
    interfaces::capnp::messages::ValidationState::Reader const& reader,
    CValidationState& state);
bool CustomHasValue(InvokeContext& invoke_context, const Coin& coin);
} // namespace mp

#endif // BITCOIN_INTERFACES_CAPNP_CHAIN_TYPES_H
