#include "searchapi_service.h"

BeatBoard::SearchApiService::SearchApiService( const std::string& db, 
                                               const std::string& table_name, 
                                               const std::string& drizzle_host, 
                                               const in_port_t drizzle_port,
                                               const std::string& memcached_host, 
                                               const in_port_t memcached_port )
{
  client = new BeatBoard::DrizzleClient( drizzle_host, drizzle_port, db );
  this->table_name = table_name;
  this->expiration= 60;

  setUpMemcached( memcached_host, memcached_port );
}

BeatBoard::SearchApiService::~SearchApiService()
{
  delete client;
  client = NULL;
  memcached_free(memc);
}

void
BeatBoard::SearchApiService::setUpMemcached( const std::string& memcached_host,
                                             const in_port_t memcached_port )
{
  memcached_server_st *servers;
  memcached_return rc;

  memcached_status = false;
  memc = memcached_create(NULL);
  servers = memcached_server_list_append(NULL, memcached_host.c_str(),
                                         memcached_port, &rc);
  if (rc != MEMCACHED_SUCCESS) {
    std::cerr << memcached_strerror(memc, rc) << std::endl;
    return;
  }

  rc = memcached_server_push(memc, servers);
  if (rc != MEMCACHED_SUCCESS) {
    std::cerr << memcached_strerror(memc, rc) << std::endl;
    return;
  }

  memcached_server_list_free(servers);
  memcached_status = true;
}

void
BeatBoard::SearchApiService::RpcFunc(google::protobuf::RpcController* controller,
                                     const searchapi::Request* request,
                                     searchapi::Response* response,
                                     google::protobuf::Closure* done)
{
  std::cout << __func__ << std::endl;

  std::string query = request->query();
  std::string result = "";

  bool ret = searchDB( query, result );
  if (ret)
  {
    response->set_result(result);
    std::cout << "OK: " << response->result() << std::endl;
    response->set_result_code(SEARCHAPI_RESULT_OK);
  }
  else 
  {
    result = " "; // "" does not work at protobuf serialize
    response->set_result(result);
    response->set_result_code(SEARCHAPI_RESULT_ERROR);
//    response->set_error("message wasn't set");
    std::cout << "NG: " << response->result() << std::endl;
  }
  done->Run();
}

bool
BeatBoard::SearchApiService::searchDB( std::string& query, std::string& result )
{
  bool ret = false;

  if (memcached_status)
  {
    ret = getMemcachedData(query, result);
  }

  if (!ret)
  {
    std::cerr << "select drizzle " << std::endl;
    ret = searchDrizzleDB(query, result);
    if (ret && memcached_status)
    {
      std::cerr << "set memcached " << std::endl;
      setMemcachedData(query, result);
    }
  }
  else
  {
    std::cerr << "get memcached " << std::endl;
  }
  return ret;
}

bool
BeatBoard::SearchApiService::searchDrizzleDB( std::string& query, std::string& result )
{
  bool ret = false;

  std::string select_list = "*";
  std::string from_clause = table_name;
  // escape query here
  std::string where_clause = "where message like '%" + ApiCommon::escape(query) + "%'";

  ret = client->select(select_list, from_clause, where_clause, drizzle_response);
  if (ret && drizzle_response.ret == DRIZZLE_RETURN_OK )
  {
    ret = drizzleResultToJson(result);
  }
  else
  {
    std::cerr << "select failure" << std::endl;
    ret = false;
  }

  return ret;
}

bool
BeatBoard::SearchApiService::setMemcachedData( std::string& query, std::string& result )
{
  bool ret = true;

  std::cerr << __func__ << std::endl;

  memcached_return rc;
  rc = memcached_set( memc, query.c_str(), query.size(),
                      result.c_str(), result.size(),
                      this->expiration, (uint32_t)0 );
  if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_BUFFERED)
  {
    std::cerr << memcached_strerror(memc, rc) << std::endl;
    ret = false;
  }
  return ret;
}

bool
BeatBoard::SearchApiService::getMemcachedData( std::string& query, std::string& result )
{
  bool ret = true;
  char *value;
  size_t value_length;
  uint32_t flags;

  std::cerr << __func__ << std::endl;

  memcached_return rc;
  value = memcached_get( memc, query.c_str(), query.size(),
                         &value_length, &flags, &rc);
  if (rc != MEMCACHED_SUCCESS)
  {
    std::cerr << memcached_strerror(memc, rc) << std::endl;
    ret = false;
  }
  else
  {
    std::cerr << "memc value: " << std::string(value) << std::endl;
    result = std::string(value);
  }
  return ret;
}

bool
BeatBoard::SearchApiService::drizzleResultToJson( std::string& result )
{
  bool ret = false;
  struct json_object *my_object = json_object_new_object();
  struct json_object *my_array = json_object_new_array();
  char *label = "messages";

  while ( drizzle_row_read(&drizzle_response.result, &drizzle_response.ret) != 0 && drizzle_response.ret == DRIZZLE_RETURN_OK )
  {
    //ret = readDrizzleField( my_object );
    ret = readDrizzleField( my_array );
  }
  json_object_object_add(my_object, label, my_array);
  result = std::string(json_object_to_json_string(my_object));
  json_object_put(my_object);
  return ret;
}


bool
BeatBoard::SearchApiService::readDrizzleField( struct json_object* my_array )
{
  drizzle_field_t field;
  size_t offset;
  size_t size;
  size_t total;
  std::string data;
  std::vector<std::string> field_data;

  while (1)
  {
    field= drizzle_field_read(&drizzle_response.result, &offset, &size, &total, &drizzle_response.ret);
    if ( drizzle_response.ret == DRIZZLE_RETURN_ROW_END)
    {
      std::cerr << "row end" << std::endl;
      fieldToJsonArray( my_array, field_data );
      field_data.clear();
      break;
    }
    else if ( drizzle_response.ret != DRIZZLE_RETURN_OK)
    {
      std::cerr << "drizzle_field_read: " << client->client_error() << std::endl;
      return false;
    }

    if (field != NULL)
    {
      data = std::string(field);
      field_data.push_back(data.substr(0, size));
    }
  }
  return true;
}

void
BeatBoard::SearchApiService::fieldToJsonArray( struct json_object* my_array,
                                               std::vector<std::string>& field_data )
{
  struct json_object *array;
  array = json_object_new_array();

  std::vector<std::string>::iterator it = field_data.begin();
  it += 2; // skip added_id, id

  std::string ts = *it;
  it++;

  char *c_ts;
  c_ts = (char *)malloc(sizeof(char) * ts.size() + 1);
  std::strncpy(c_ts, ts.c_str(), ts.size() + 1);
  json_object_array_add(array, json_object_new_string(c_ts));
  
  for( ; it != field_data.end(); ++it )
  {
    //std::cout << *it << std::endl;
    char *c_char;
    c_char = (char *)malloc(sizeof(char) * it->size() + 1);
    std::strncpy(c_char, it->c_str(), it->size() + 1);
    //json_object_array_add(my_array, json_object_new_string(c_char));
    json_object_array_add(array, json_object_new_string(c_char));
    free(c_char);
  }
//  json_object_object_add(my_object, c_ts, my_array);
  json_object_array_add(my_array, array);
  free(c_ts);
}
