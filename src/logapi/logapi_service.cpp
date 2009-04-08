#include "logapi_service.h"

BeatBoard::LogApiService::LogApiService( const std::string& db, 
                                         const std::string& tablename, 
                                         const std::string& host, 
                                         const in_port_t port )
{
}

BeatBoard::LogApiService::~LogApiService()
{

}


void
BeatBoard::LogApiService::RpcFunc(google::protobuf::RpcController* controller,
                                  const logapi::Request* request,
                                  logapi::Response* response,
                                  google::protobuf::Closure* done)
{
  std::cout << __func__ << std::endl;
  if (request->identifier() != "+" || request->identifier() != "-") 
  {
    response->set_result(LOGAPI_RESULT_OK);
    std::cout << "OK: " << response->result() << std::endl;
  }
  else 
  {
    response->set_result(LOGAPI_RESULT_ERROR);
    response->set_error("message wasn't set");
    std::cout << "NG: " << response->result() << std::endl;
  }
  done->Run();
}
