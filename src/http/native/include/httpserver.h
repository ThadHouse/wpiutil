#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include "llvm/StringRef.h"
#include <support/SafeThread.h>
#include "llvm/Twine.h"
#include "tcpsockets/NetworkAcceptor.h"
#include "tcpsockets/NetworkStream.h"
#include "support/Logger.h"
#include "support/mutex.h"

#include <functional>

namespace hs {

class HttpServer {
 public:
  typedef std::function<bool(wpi::NetworkStream* stream)> HttpProcessFunction;

  HttpServer(const llvm::Twine& name, const llvm::Twine& listenAddress, int port, wpi::Logger& logger, std::unique_ptr<wpi::NetworkAcceptor> acceptor);
  ~HttpServer();

  void Stop();
  std::string GetListenAddress() { return m_listenAddress; }
  int GetPort() { return m_port; }
  std::string GetName() { return m_name; }

  void SetProcessFunc(HttpProcessFunction func);

 private:
  void ServerThreadMain();

  class ConnThread;

  std::string m_listenAddress;
  std::string m_name;
  int m_port;

  wpi::Logger& m_logger;
  HttpProcessFunction m_processFunc;
  wpi::mutex m_funcMutex;

  std::unique_ptr<wpi::NetworkAcceptor> m_acceptor;
  std::atomic_bool m_active;
  std::thread m_serverThread;

  std::vector<wpi::SafeThreadOwner<ConnThread>> m_connThreads;

  wpi::mutex m_mutex;
};

std::shared_ptr<HttpServer> CreateHttpServer(const llvm::Twine& name, const llvm::Twine& listenAddress, int port, wpi::Logger& logger);

}
