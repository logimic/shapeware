#define IRestApiService_EXPORTS
#define ITestSimulationIRestApiService_EXPORTS

#include "TestSimulationIRestApiService.h"
#include "Trace.h"

#include "rapidjson/pointer.h"

#include <mutex>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <algorithm>

#include "shape__TestSimulationIRestApiService.hxx"

TRC_INIT_MNAME(shape::TestSimulationIRestApiService)

namespace shape {


  class TestSimulationIRestApiService::Imp {
  private:

    class Url
    {
    public:
      void parse(const std::string& url_s)
      {
        using namespace std;

        const std::string prot_end("://");
        string::const_iterator prot_i = search(url_s.begin(), url_s.end(),
          prot_end.begin(), prot_end.end());
        protocol_.reserve(distance(url_s.begin(), prot_i));
        transform(url_s.begin(), prot_i,
          back_inserter(protocol_),
          ptr_fun<int, int>(tolower)); // protocol is icase
        if (prot_i == url_s.end())
          return;
        advance(prot_i, prot_end.length());
        const char separ = '/';
        string::const_iterator path_i = find(prot_i, url_s.end(), separ);
        host_.reserve(distance(prot_i, path_i));
        transform(prot_i, path_i,
          back_inserter(host_),
          ptr_fun<int, int>(tolower)); // host is icase
        string::const_iterator query_i = find(path_i, url_s.end(), '?');
        path_.assign(path_i, query_i);
        if (query_i != url_s.end())
          ++query_i;
        query_.assign(query_i, url_s.end());
      }

      std::string protocol_, host_, path_, query_, m_fname, m_request;

    };

    std::string m_name;
    std::queue<Url> m_incomingRequestQueue;
    std::mutex  m_queueMux;
    std::condition_variable m_cv;
    
    std::string m_resourceDir = "iqrfRepoResource1";

  public:

    Imp()
    {
    }

    ~Imp()
    {
    }

    void getFile(const std::string & urlStr, const std::string& fname)
    {
      TRC_FUNCTION_ENTER(PAR(urlStr) << PAR(fname));

      Url url;
      url.parse(urlStr);
      url.m_fname = fname;
      url.m_request = urlStr;

      TRC_DEBUG(PAR(url.protocol_) << PAR(url.host_) << PAR(url.path_) << PAR(url.query_) << PAR(url.m_fname));

      std::ostringstream os;
      os << "./configuration/testResources/" << m_resourceDir << url.path_ << '/' << "data.json";
      std::string from = os.str();
      
      TRC_DEBUG(PAR(from));

      std::ifstream  src(from, std::ios::binary);
      std::ofstream  dst(fname, std::ios::binary);
      dst << src.rdbuf();
      src.close();
      dst.close();

      std::unique_lock<std::mutex> lck(m_queueMux);
      m_incomingRequestQueue.push(url);
      m_cv.notify_one();
      
      TRC_FUNCTION_LEAVE("")
    }

    std::string popIncomingRequest(unsigned millisToWait)
    {
      TRC_FUNCTION_ENTER(PAR(millisToWait));
      std::unique_lock<std::mutex> lck(m_queueMux);
      std::string retval;
      if (m_incomingRequestQueue.empty()) {
        while (m_cv.wait_for(lck, std::chrono::milliseconds(millisToWait)) != std::cv_status::timeout) {
          if (!m_incomingRequestQueue.empty()) break;
        }
      }

      if (!m_incomingRequestQueue.empty()) {
        retval = m_incomingRequestQueue.front().m_request;
        m_incomingRequestQueue.pop();
      }
      TRC_FUNCTION_LEAVE(PAR(retval));
      return retval;
    }

    void setResourceDirectory(const std::string& dir)
    {
      TRC_FUNCTION_ENTER(PAR(dir));
      m_resourceDir = dir;
      TRC_FUNCTION_LEAVE("");
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestSimulationIRestApiService instance activate" << std::endl <<
        "******************************"
      );

      const rapidjson::Value* val = rapidjson::Pointer("/instance").Get(props->getAsJson());
      m_name = val->GetString();

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestSimulationIRestApiService instance deactivate" << std::endl <<
        "******************************"
      );
      TRC_FUNCTION_LEAVE("")
    }
  };

  ////////////////////////////////////
  TestSimulationIRestApiService::TestSimulationIRestApiService()
  {
    m_imp = shape_new Imp();
  }

  TestSimulationIRestApiService::~TestSimulationIRestApiService()
  {
    delete m_imp;
  }

  void TestSimulationIRestApiService::getFile(const std::string & url, const std::string& fname)
  {
    m_imp->getFile(url, fname);
  }

  std::string TestSimulationIRestApiService::popIncomingRequest(unsigned millisToWait)
  {
    return m_imp->popIncomingRequest(millisToWait);
  }

  void TestSimulationIRestApiService::setResourceDirectory(const std::string& dir)
  {
    m_imp->setResourceDirectory(dir);
  }

  /////////////////////////
  void TestSimulationIRestApiService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void TestSimulationIRestApiService::deactivate()
  {
    m_imp->deactivate();
  }

  void TestSimulationIRestApiService::modify(const shape::Properties *props)
  {
  }

  void TestSimulationIRestApiService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestSimulationIRestApiService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
