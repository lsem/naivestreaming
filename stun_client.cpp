#include "stun_client.hpp"
#include "log.hpp"

class STUN_ClientImpl : public STUN_Client {
 public:
  explicit STUN_ClientImpl(asio::io_context& ctx, std::string stun_host)
      : m_ctx(ctx), m_stun_host(stun_host) {}

  // The point of STUN is to tell to some external server so it can communicate
  // us what is our IP address behind the NAT>
  virtual void async_resolve_own_ip(
      callback<asio::ip::udp::endpoint> cb) override {
    LOG_ERROR("not imlemented");
  }

 private:
  asio::io_context& m_ctx;
  std::string m_stun_host;
};

std::unique_ptr<STUN_Client> make_stun_client(asio::io_context& ctx,
                                              std::string stun_host) {
  return std::make_unique<STUN_ClientImpl>(ctx, stun_host);
}
