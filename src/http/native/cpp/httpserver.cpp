#include "httpserver.h"
#include "tcpsockets/TCPAcceptor.h"

using namespace hs;

class HttpServer::ConnThread : public wpi::SafeThread {
 public:
  explicit ConnThread(llvm::StringRef name, wpi::Logger& logger, HttpServer::HttpProcessFunction func) : m_name(name), m_logger(logger), m_processFunc(func) { }

  void Main();

  void ProcessRequest();

  void SetProcessFunc(HttpServer::HttpProcessFunction func);

  std::unique_ptr<wpi::NetworkStream> m_stream;
  HttpServer::HttpProcessFunction m_processFunc;
  std::mutex m_funcMutex;

 private:
  std::string m_name;
  wpi::Logger& m_logger;



  llvm::StringRef GetName() { return m_name; }


};

void HttpServer::ConnThread::SetProcessFunc(HttpServer::HttpProcessFunction func) {
  std::lock_guard<wpi::mutex> lock(m_funcMutex);
  m_processFunc = func;
}

void HttpServer::ConnThread::ProcessRequest() {
  std::unique_lock<wpi::mutex> lock(m_funcMutex);
  auto func = m_processFunc;
  if (func) {
    lock.unlock();
    bool success = func(m_stream.get());
    if (!success) {
      WPI_ERROR(m_logger, "process failed");
    }
  } else {
    WPI_WARNING(m_logger, "func null");
  }
}

void HttpServer::ConnThread::Main() {
  std::unique_lock<wpi::mutex> lock(m_mutex);
  while (m_active) {
    while (!m_stream) {
      m_cond.wait(lock);
      if (!m_active) return;
    }
    lock.unlock();
    ProcessRequest();
    lock.lock();
    m_stream = nullptr;
  }
}

HttpServer::HttpServer(const llvm::Twine& name, const llvm::Twine& listenAddress, int port, wpi::Logger& logger, std::unique_ptr<wpi::NetworkAcceptor> acceptor) : m_logger(logger) {
  m_port = port;
  m_acceptor = std::move(acceptor);
  m_listenAddress = listenAddress.str();

  m_active = true;
  m_serverThread = std::thread(&HttpServer::ServerThreadMain, this);
}
HttpServer::~HttpServer() {

  Stop();
}

void HttpServer::Stop() {
    m_active = false;

  m_acceptor->shutdown();

  if (m_serverThread.joinable()) m_serverThread.join();

  // close streams
  for (auto& connThread : m_connThreads) {
    if (auto thr = connThread.GetThread()) {
      if (thr->m_stream) thr->m_stream->close();
    }
    connThread.Stop();
  }
}

void HttpServer::SetProcessFunc(HttpServer::HttpProcessFunction func) {
  std::lock_guard<wpi::mutex> lock(m_funcMutex);
  m_processFunc = func;
  std::for_each(m_connThreads.begin(), m_connThreads.end(), [&](const wpi::SafeThreadOwner<ConnThread>& owner)
  {
    auto thr = owner.GetThread();
    if (thr) thr->SetProcessFunc(func);
  });
}

void HttpServer::ServerThreadMain() {
  if (m_acceptor->start() != 0) {
    m_active = false;
    return;
  }

  WPI_DEBUG(m_logger, "waiting for clients to connect");
  while (m_active) {
    auto stream = m_acceptor->accept();
    if (!stream) {
      m_active = false;
      return;
    }
    if (!m_active) return;

    WPI_DEBUG(m_logger, "client connection from " << stream->getPeerIP());

    std::lock_guard<wpi::mutex> lock(m_mutex);

    auto it = std::find_if(m_connThreads.begin(), m_connThreads.end(), [](const wpi::SafeThreadOwner<ConnThread>& owner){
      auto thr = owner.GetThread();
      return !thr || !thr->m_stream;
    });

    if (it == m_connThreads.end()) {
      m_connThreads.emplace_back();
      it = std::prev(m_connThreads.end());
    }

    {
      auto thr = it->GetThread();
      if (!thr) it->Start(new ConnThread{GetName(), m_logger, m_processFunc});
    }

    auto thr = it->GetThread();
    thr->m_stream = std::move(stream);
    thr->m_cond.notify_one();
  }

  WPI_DEBUG(m_logger, "leaving server thread");
}



std::shared_ptr<HttpServer> hs::CreateHttpServer(const llvm::Twine& name, const llvm::Twine& listenAddress, int port, wpi::Logger& logger) {
  llvm::SmallVector<char, 32> listen;
  auto listenStr = listenAddress.toNullTerminatedStringRef(listen);

  return std::make_shared<HttpServer>(name, listenAddress, port, logger, std::make_unique<wpi::TCPAcceptor>(port, listenStr.data(), logger));
}
