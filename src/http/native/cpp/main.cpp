#include "httpserver.h"
#include <support/raw_socket_istream.h>
#include <support/raw_socket_ostream.h>

using namespace hs;
using namespace wpi;


// Standard header to send along with other header information like mimetype.
//
// The parameters should ensure the browser does not cache our answer.
// A browser should connect for each file and not serve files from its cache.
// Using cached pictures would lead to showing old/outdated pictures.
// Many browsers seem to ignore, or at least not always obey, those headers.
static void SendHeader(llvm::raw_ostream& os, int code,
	llvm::StringRef codeText, llvm::StringRef contentType,
	llvm::StringRef extra = llvm::StringRef{}) {
	os << "HTTP/1.0 " << code << ' ' << codeText << "\r\n";
	os << "Connection: close\r\n"
		"Server: CameraServer/1.0\r\n"
		"Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, "
		"post-check=0, max-age=0\r\n"
		"Pragma: no-cache\r\n"
		"Expires: Mon, 3 Jan 2000 12:34:56 GMT\r\n";
	os << "Content-Type: " << contentType << "\r\n";
	os << "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: *\r\n";
	if (!extra.empty()) os << extra << "\r\n";
	os << "\r\n";  // header ends with a blank line
}

// Send error header and message
// @param code HTTP error code (e.g. 404)
// @param message Additional message text
static void SendError(llvm::raw_ostream& os, int code,
	llvm::StringRef message) {
	llvm::StringRef codeText, extra, baseMessage;
	switch (code) {
	case 401:
		codeText = "Unauthorized";
		extra = "WWW-Authenticate: Basic realm=\"CameraServer\"";
		baseMessage = "401: Not Authenticated!";
		break;
	case 404:
		codeText = "Not Found";
		baseMessage = "404: Not Found!";
		break;
	case 500:
		codeText = "Internal Server Error";
		baseMessage = "500: Internal Server Error!";
		break;
	case 400:
		codeText = "Bad Request";
		baseMessage = "400: Not Found!";
		break;
	case 403:
		codeText = "Forbidden";
		baseMessage = "403: Forbidden!";
		break;
	case 503:
		codeText = "Service Unavailable";
		baseMessage = "503: Service Unavailable";
		break;
	default:
		code = 501;
		codeText = "Not Implemented";
		baseMessage = "501: Not Implemented!";
		break;
	}
	SendHeader(os, code, codeText, "text/plain", extra);
	os << baseMessage << "\r\n" << message;
}



static bool ProcessSet(llvm::raw_ostream& os, llvm::StringRef data)
{
	llvm::outs() << "data: " << data << "\n";
	llvm::outs().flush();

	llvm::SmallVector<llvm::StringRef, 16> splitData;
	data.split(splitData, ',');
	if (splitData.size() == 0 || splitData.size() > 4)
	{
		SendError(os, 400, "Failed to parse command");
		return false;
	}

	llvm::SmallVector<std::pair<int, int>, 4> items;

	for (auto& item : splitData) {
		auto pair = item.split(':');
		int firstItem = 0;
		if (pair.first.getAsInteger(0, firstItem)) {
			SendError(os, 400, "Invalid data");
			return false;
		}

		

		int secondItem = 0;
		if (pair.second.getAsInteger(0, secondItem)) {
			SendError(os, 400, "Invalid data");
			return false;
		}

		if (firstItem < 1 || firstItem > 4 || secondItem < 1 || secondItem > 4)
		{
			SendError(os, 400, "Invalid Data");
			return false;
		}

		items.emplace_back( firstItem, secondItem );
	}

	// TODO, use items


	return true;

}

static bool ProcessGet(llvm::raw_ostream& os, wpi::raw_istream& is, llvm::StringRef url) {
	if (url.find("/get") != llvm::StringRef::npos) {
		// Running a get
		SendHeader(os, 200, "OK", "application/json");
		os << "[1,2,3,4]";
		os << "\r\n";
	}
	else if (url.find("/set") != llvm::StringRef::npos) {
		// running a set
		size_t last = url.find_last_of('/');
		if (last < 0) {
			SendError(os, 400, "Error finding command");
		}
		llvm::StringRef params = url.substr(last + 1);

		return ProcessSet(os, params);
	}
	else {
		SendError(os, 400, "Only allowed urls are /get and /set");
		return false;
	}
}

int main() {
  Logger logger;
  logger.SetLogger([](auto a, auto b, auto c, auto d){


  });

  auto server = CreateHttpServer("MyServer", "", 1111, logger);

  server->SetProcessFunc([](wpi::NetworkStream* buf) {
	  wpi::raw_socket_istream is{ *buf };
	  wpi::raw_socket_ostream os{ *buf, true };

	  // Read the request string from the stream
	  llvm::SmallString<128> reqBuf;
	  llvm::StringRef req = is.getline(reqBuf, 4096);
	  if (is.has_error()) {
		  llvm::outs() << "error getting request string\n";
		  return false;
	  }
	  
	  int status = 0;
	  llvm::StringRef parameters;
	  size_t pos;
	  if ((pos = req.find("GET")) != llvm::StringRef::npos) {
		  status = 1;
	  }

	  if (status == 0) {
		  SendError(os, 400, "Can only accept GET");
		  return false;
	  }

	  size_t startSpace = req.find(' ', pos);
	  size_t endSpace = req.find(' ', startSpace + 1);
	  llvm::StringRef url = req.substr(startSpace, endSpace - startSpace).trim();


	  llvm::outs() << "Request: " << req << "\n";
	  llvm::outs() << "url: " << url << "\n";
	  llvm::outs().flush();


	  // Read the rest of the HTTP request.
	  // The end of the request is marked by a single, empty line
	  llvm::SmallString<128> lineBuf;
	  for (;;) {
		  if (is.getline(lineBuf, 4096).startswith("\n")) break;
		  if (is.has_error()) return false;
	  }



	  llvm::StringRef lb{ lineBuf.data(), lineBuf.size() };

	  llvm::outs() << lb << "\n";
	  llvm::outs().flush();

	 if (status == 1) {
		  // Running GET
		  return ProcessGet(os, is, url);
	  }


	  return false;
  });

  std::condition_variable cv;
std::mutex m;
std::unique_lock<std::mutex> lock(m);
cv.wait(lock, []{return false;});
}
