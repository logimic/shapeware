/**
* Copyright 2018 Logimic,s.r.o.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "WsServerTls.h"
#include "Trace.h"

#include <websocketpp/config/asio.hpp>

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

namespace shape {
  typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

  class WsServerTls::Imp : public WsServerTyped<websocketpp::server<websocketpp::config::asio_tls>>
  {
  public:
    // See https://wiki.mozilla.org/Security/Server_Side_TLS for more details about
    // the TLS modes. The code below demonstrates how to implement both the modern
    enum tls_mode {
      MOZILLA_INTERMEDIATE = 1,
      MOZILLA_MODERN = 2
    };

    void setTls(const std::string & cert, const std::string & key)
    {
      m_cert = cert;
      m_key = key;
      getServer().set_tls_init_handler([&](connection_hdl hdl)->context_ptr {
        //return on_tls_init(MOZILLA_INTERMEDIATE, hdl);
        return on_tls_init(MOZILLA_MODERN, hdl);
      });
    }

    context_ptr on_tls_init(tls_mode mode, connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER(NAME_PAR(mode, (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate")) << NAME_PAR(hdl, hdl.lock().get()));

      namespace asio = websocketpp::lib::asio;

      context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

      try {
        if (mode == MOZILLA_MODERN) {
          // Modern disables TLSv1
          ctx->set_options(asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::no_tlsv1 |
            asio::ssl::context::single_dh_use);
        }
        else {
          ctx->set_options(asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::single_dh_use);
        }
        //ctx->set_password_callback(bind(&get_password));
        ctx->use_certificate_chain_file(m_cert);
        ctx->use_private_key_file(m_key, asio::ssl::context::pem);

        // Example method of generating this file:
        // `openssl dhparam -out dh.pem 2048`
        // Mozilla Intermediate suggests 1024 as the minimum size to use
        // Mozilla Modern suggests 2048 as the minimum size to use.
        //ctx->use_tmp_dh_file("./tls/dh.pem");

        std::string ciphers;

        if (mode == MOZILLA_MODERN) {
          ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
        }
        else {
          ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        }

        if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1) {
          std::cout << "Error setting cipher list" << std::endl;
        }
      }
      catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
      }

      TRC_FUNCTION_LEAVE("");
      return ctx;
    }

  private:
    std::string m_cert;
    std::string m_key;
  };

  WsServerTls::WsServerTls()
  {
    m_imp = shape_new WsServerTls::Imp();
  }

  WsServerTls::~WsServerTls()
  {
    delete m_imp;
  }

  void WsServerTls::run()
  {
    m_imp->run();
  }

  bool WsServerTls::is_listening()
  {
    return m_imp->is_listening();
  }

  void WsServerTls::listen(int port)
  {
    m_imp->listen(port);
  }

  void WsServerTls::start_accept()
  {
    m_imp->start_accept();
  }

  void WsServerTls::send(connection_hdl chdl, const std::string & msg)
  {
    m_imp->send(chdl, msg);
  }

  void WsServerTls::close(connection_hdl chndl, const std::string & descr, const std::string & data)
  {
    m_imp->close(chndl, descr, data);
  }

  void WsServerTls::stop_listening()
  {
    m_imp->stop_listening();
  }

  void WsServerTls::getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri)
  {
    m_imp->getConnParams(chdl, connId, uri);
  }

  void WsServerTls::setOnFunctions(OnValidate onValidate, OnFail onFail, OnClose onClose, OnMessage onMessage)
  {
    m_imp->setOnFunctions(onValidate, onFail, onClose, onMessage);
  }

  void WsServerTls::setTls(const std::string & cert, const std::string & key)
  {
    m_imp->setTls(cert, key);
  }

}
