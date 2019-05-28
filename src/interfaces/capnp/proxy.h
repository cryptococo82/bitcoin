#ifndef BITCOIN_INTERFACES_CAPNP_PROXY_H
#define BITCOIN_INTERFACES_CAPNP_PROXY_H

#include <interfaces/base.h>
#include <interfaces/capnp/proxy.capnp.h>
#include <interfaces/capnp/util.h>
#include <util/system.h>

#include <capnp/blob.h> // for capnp::Text
#include <capnp/common.h>
#include <functional>
#include <kj/async.h>
#include <list>
#include <memory>

namespace interfaces {
namespace capnp {

class EventLoop;
struct Connection;

using CleanupList = std::list<std::function<void()>>;
using CleanupIt = typename CleanupList::iterator;

//! Mapping from capnp interface type to proxy client implementation (specializations are generated by
//! proxy-codegen.cpp).
template <typename Interface>
struct ProxyClient;

//! Mapping from capnp interface type to proxy server implementation (specializations are generated by
//! proxy-codegen.cpp).
template <typename Interface>
struct ProxyServer;

//! Mapping from capnp method params type to method traits (specializations are generated by proxy-codegen.cpp).
template <typename Params>
struct ProxyMethod;

//! Mapping from capnp struct type to struct traits (specializations are generated by proxy-codegen.cpp).
template <typename Struct>
struct ProxyStruct;

//! Mapping from local c++ type to capnp type and traits (specializations are generated by proxy-codegen.cpp).
template <typename Type>
struct ProxyType;

//! Context argument to BuildField / ReadField.
struct InvokeContext
{
    Connection& connection;
};

template <typename Interface, typename Impl>
std::unique_ptr<Impl> MakeProxyClient(InvokeContext& context, typename Interface::Client&& client)
{
    return MakeUnique<ProxyClient<Interface>>(std::move(client), context.connection);
}

template <typename Interface, typename Impl>
kj::Own<typename Interface::Server> MakeProxyServer(InvokeContext& context, std::unique_ptr<Impl>&& impl)
{
    return kj::heap<ProxyServer<Interface>>(impl.release(), true /* owned */, context.connection);
}

template <typename Interface, typename Impl>
std::unique_ptr<Impl> CustomMakeProxyClient(InvokeContext& context, typename Interface::Client&& client)
{
    return MakeProxyClient<Interface, Impl>(context, kj::mv(client));
}

template <typename Interface, typename Impl>
kj::Own<typename Interface::Server> CustomMakeProxyServer(InvokeContext& context, std::unique_ptr<Impl>&& impl)
{
    return MakeProxyServer<Interface, Impl>(context, std::move(impl));
}

//! Wrapper around std::function for passing std::function objects between client and servers.
template <typename Fn>
class ProxyCallback;

//! Template specialization to separate Result and Arg types.
template <typename Result, typename... Args>
class ProxyCallback<std::function<Result(Args...)>> : public Base
{
public:
    virtual Result call(Args&&... args) = 0;
};

//! Get return type of a callable type.
template <typename Callable>
using ResultOf = decltype(std::declval<Callable>()());

//! Needed for libc++/macOS compatibility. Lets code work with shared_ptr nothrow declaration
//! https://github.com/capnproto/capnproto/issues/553#issuecomment-328554603
template <typename T>
struct DestructorCatcher
{
    T value;
    template <typename... Params>
    DestructorCatcher(Params&&... params) : value(kj::fwd<Params>(params)...)
    {
    }
    ~DestructorCatcher() noexcept try {
    } catch (const kj::Exception& e) {
    }
};

//! Wrapper around callback function for compatibility with std::async.
//!
//! std::async requires callbacks to be copyable and requires noexcept
//! destructors, but this doesn't work well with kj types which are generally
//! move-only and not noexcept.
template <typename Callable>
struct AsyncCallable
{
    AsyncCallable(Callable&& callable) : m_callable(std::make_shared<DestructorCatcher<Callable>>(std::move(callable)))
    {
    }
    AsyncCallable(const AsyncCallable&) = default;
    AsyncCallable(AsyncCallable&&) = default;
    ~AsyncCallable() noexcept {}
    ResultOf<Callable> operator()() const { return (m_callable->value)(); }
    mutable std::shared_ptr<DestructorCatcher<Callable>> m_callable;
};

//! Construct AsyncCallable object.
template <typename Callable>
AsyncCallable<typename std::remove_reference<Callable>::type> MakeAsyncCallable(Callable&& callable)
{
    return std::move(callable);
}

//! Base class for generated ProxyClient classes that implement a C++ interface
//! and forward calls to a capnp interface.
template <typename Interface_, typename Impl_>
class ProxyClientBase : public Impl_
{
public:
    using Interface = Interface_;
    using Impl = Impl_;

    ProxyClientBase(typename Interface::Client client, Connection& connection);
    ~ProxyClientBase() noexcept;

    // Methods called during client construction/destruction that can optionally
    // be defined in capnp interface to trigger the server.
    void construct() {}
    void destroy() {}

    ProxyClient<Interface>& self() { return static_cast<ProxyClient<Interface>&>(*this); }

    typename Interface::Client m_client;
    Connection* m_connection;
    CleanupIt m_cleanup;
};

template <typename MethodTraits, typename GetRequest, typename ProxyClient, typename... _Params>
void clientInvoke(MethodTraits, const GetRequest& get_request, ProxyClient& proxy_client, _Params&&... params);

void AddClient(EventLoop& loop);
void RemoveClient(EventLoop& loop);

//! Base class for generated ProxyServer classes that implement capnp server
//! methods and forward calls to a wrapped c++ implementation class.
template <typename Interface_, typename Impl_>
struct ProxyServerBase : public virtual Interface_::Server
{
public:
    using Interface = Interface_;
    using Impl = Impl_;

    ProxyServerBase(Impl* impl, bool owned, Connection& connection);
    //! Special constructor only used to create Init server, before there is any connection.
    ProxyServerBase(Impl& impl, EventLoop& loop);
    virtual ~ProxyServerBase();
    void invokeDestroy();

    Impl* m_impl;
    /**
     * Whether or not to delete native interface pointer when this capnp server
     * goes out of scope. This is true for servers created to wrap
     * unique_ptr<Impl> method arguments, but false for servers created to wrap
     * Impl& method arguments.
     *
     * In the case of Impl& arguments, custom code is required on other side of
     * the connection to delete the capnp client & server objects since native
     * code on that side of the connection will just be taking a plain reference
     * rather than a pointer, so won't be able to do its own cleanup. Right now
     * this is implemented with addCloseHook callbacks to delete clients at
     * appropriate times depending on semantics of the particular method being
     * wrapped. */
    bool m_owned;
    /**
     * Connection is a pointer rather than a reference because for the Init
     * server, the server object needs to be created before the connection.
     */
    Connection* m_connection;
};

template <typename Interface, typename Impl>
struct ProxyServerCustom : public ProxyServerBase<Interface, Impl>
{
    using ProxyServerBase<Interface, Impl>::ProxyServerBase;
};

template <typename Interface, typename Impl>
class ProxyClientCustom : public ProxyClientBase<Interface, Impl>
{
    using ProxyClientBase<Interface, Impl>::ProxyClientBase;
};

//! Function traits class.
template <class Fn>
struct FunctionTraits;

template <class _Class, class _Result, class... _Params>
struct FunctionTraits<_Result (_Class::*const)(_Params...)>
{
    using Params = TypeList<_Params...>;
    using Result = _Result;
    template <size_t N>
    using Param = typename std::tuple_element<N, std::tuple<_Params...>>::type;
    using Fields =
        typename std::conditional<std::is_same<void, Result>::value, Params, TypeList<_Params..., _Result>>::type;
};

template <class _Class, class _Result, class... _Params>
struct FunctionTraits<_Result (_Class::*)(_Params...)>
{
    using Result = _Result;
};

template <class _Class, class _Result, class... _Params>
struct FunctionTraits<_Result (_Class::*)(_Params...) const>
{
    using Result = _Result;
};

template <>
struct FunctionTraits<std::nullptr_t>
{
    using Result = std::nullptr_t;
};

template <typename Method, typename Enable = void>
struct ProxyMethodTraits
{
    using Params = TypeList<>;
    using Result = void;
    using Fields = Params;

    template <typename ServerContext>
    static void invoke(ServerContext&)
    {
    }
};

template <typename Method>
struct ProxyMethodTraits<Method, typename std::enable_if<true || ProxyMethod<Method>::impl>::type>
    : public FunctionTraits<decltype(ProxyMethod<Method>::impl)>
{
    template <typename ServerContext, typename... Args>
    static auto invoke(ServerContext& server_context, Args&&... args)
        -> AUTO_RETURN((server_context.proxy_server.m_impl->*ProxyMethod<Method>::impl)(std::forward<Args>(args)...))
};

template <typename Method>
struct ProxyClientMethodTraits : public ProxyMethodTraits<Method>
{
};
template <typename Method>
struct ProxyServerMethodTraits : public ProxyMethodTraits<Method>
{
};

template <typename ProxyServer, typename CallContext_>
struct ServerInvokeContext : InvokeContext
{
    using CallContext = CallContext_;

    ProxyServer& proxy_server;
    CallContext& call_context;
    int req;

    ServerInvokeContext(ProxyServer& proxy_server, CallContext& call_context, int req)
        : InvokeContext{*proxy_server.m_connection}, proxy_server{proxy_server}, call_context{call_context}, req{req}
    {
    }
};

template <typename Interface, typename Params, typename Results>
using ServerContext = ServerInvokeContext<ProxyServer<Interface>, ::capnp::CallContext<Params, Results>>;

template <typename Value>
class ValueField
{
public:
    ValueField(Value& value) : m_value(value) {}
    ValueField(Value&& value) : m_value(value) {}
    Value& m_value;

    Value& get() { return m_value; }
    Value& init() { return m_value; }
    bool has() { return true; }
};

using BuildFieldPriority = Priority<3>;

template <typename Accessor, typename Struct>
struct StructField
{
    template <typename S>
    StructField(S& struct_) : m_struct(struct_)
    {
    }
    Struct& m_struct;

    // clang-format off
    template<typename A = Accessor> auto get() const -> AUTO_RETURN(A::get(this->m_struct))
    template<typename A = Accessor> auto has() const -> typename std::enable_if<A::optional, bool>::type { return A::getHas(m_struct); }
    template<typename A = Accessor> auto has() const -> typename std::enable_if<!A::optional && A::boxed, bool>::type { return A::has(m_struct); }
    template<typename A = Accessor> auto has() const -> typename std::enable_if<!A::optional && !A::boxed, bool>::type { return true; }
    template<typename A = Accessor> auto want() const -> typename std::enable_if<A::requested, bool>::type { return A::getWant(m_struct); }
    template<typename A = Accessor> auto want() const -> typename std::enable_if<!A::requested, bool>::type { return true; }

    template<typename A = Accessor, typename... Args> auto set(Args&&... args) const -> AUTO_RETURN(A::set(this->m_struct, std::forward<Args>(args)...))
    template<typename A = Accessor, typename... Args> auto init(Args&&... args) const -> AUTO_RETURN(A::init(this->m_struct, std::forward<Args>(args)...))
    template<typename A = Accessor> auto setHas() const -> typename std::enable_if<A::optional>::type { return A::setHas(m_struct); }
    template<typename A = Accessor> auto setHas() const -> typename std::enable_if<!A::optional>::type { }
    template<typename A = Accessor> auto setWant() const -> typename std::enable_if<A::requested>::type { return A::setWant(m_struct); }
    template<typename A = Accessor> auto setWant() const -> typename std::enable_if<!A::requested>::type { }
    // clang-format on
};

// Adapter to let BuildField overloads methods work set & init list elements as
// if they were fields of a struct. If BuildField is changed to use some kind of
// accessor class instead of calling method pointers, then then maybe this could
// go away or be simplified, because would no longer be a need to return
// ListOutput method pointers emulating capnp struct method pointers..
template <typename ListType>
struct ListOutput;

template <typename T, ::capnp::Kind kind>
struct ListOutput<::capnp::List<T, kind>>
{
    using Builder = typename ::capnp::List<T, kind>::Builder;

    ListOutput(Builder& builder, size_t index) : m_builder(builder), m_index(index) {}
    Builder& m_builder;
    size_t m_index;

    // clang-format off
    auto get() const -> AUTO_RETURN(this->m_builder[this->m_index])
    auto init() const -> AUTO_RETURN(this->m_builder[this->m_index])
    template<typename B = Builder, typename Arg> auto set(Arg&& arg) const -> AUTO_RETURN(static_cast<B&>(this->m_builder).set(m_index, std::forward<Arg>(arg)))
    template<typename B = Builder, typename Arg> auto init(Arg&& arg) const -> AUTO_RETURN(static_cast<B&>(this->m_builder).init(m_index, std::forward<Arg>(arg)))
    // clang-format on
};

static constexpr int FIELD_IN = 1;
static constexpr int FIELD_OUT = 2;
static constexpr int FIELD_OPTIONAL = 4;
static constexpr int FIELD_REQUESTED = 8;
static constexpr int FIELD_BOXED = 16;

template <typename Field, int flags>
struct Accessor : public Field
{
    static const bool in = flags & FIELD_IN;
    static const bool out = flags & FIELD_OUT;
    static const bool optional = flags & FIELD_OPTIONAL;
    static const bool requested = flags & FIELD_REQUESTED;
    static const bool boxed = flags & FIELD_BOXED;
};

template <>
struct ProxyServer<ThreadMap> final : public virtual ThreadMap::Server
{
public:
    ProxyServer(Connection& connection);
    kj::Promise<void> makeThread(MakeThreadContext context) override;
    Connection& m_connection;
};

std::string LongThreadName(const char* exe_name);

#define LogIpc(loop, format, ...) \
    LogPrint(::BCLog::IPC, "{%s} " format, LongThreadName((loop).m_exe_name), ##__VA_ARGS__)

} // namespace capnp
} // namespace interfaces

#endif // BITCOIN_INTERFACES_CAPNP_PROXY_H
