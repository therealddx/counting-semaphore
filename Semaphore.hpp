#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include <stdint.h>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>
#include <ostream>

class SemaphoreClient
{
public:

  SemaphoreClient(std::string arg_name)
  {
    _name = arg_name;
  };

  ~SemaphoreClient() { } ;

  const std::string& GetName() { return _name; }

private:
  std::string _name;
};

class Semaphore
{
public:

  typedef enum
  {
    E_OK = 0,
    E_TakeFail = 1,
    E_GiveFail = 2,
  } Error;

  Semaphore(uint32_t, std::string = "");

  ~Semaphore();

  uint32_t GetFlagMax() { return _flag_max; }

  Error Take(SemaphoreClient* arg_client, uint32_t arg_timeout_ms = 0, uint32_t arg_dtimeout_ms = 0);

  Error Give(SemaphoreClient*);

  void DumpState();

private:

  // accessors for '_available_flags'.
  //   verify `numAvailableFlags() > 0` before calling `firstAvailableFlag()`
  //   accessors assume '_available_flags' has a lock on exeucting thread.
  //
  uint32_t firstAvailableFlag();
  uint32_t numAvailableFlags();

  std::map<SemaphoreClient*, uint32_t> _working;
  std::map<uint32_t, bool> _available_flags; // true => available, false ow
  uint32_t _flag_max;
  std::mutex _map_lock; // lock for write access to the '_working' map from clients to flags.

  bool canLog() { return ( ! (_log == nullptr || !_log->is_open()) ) ; }
  void doLog(std::string);
  std::ofstream* _log;
  std::mutex _log_lock; // lock for access to the log.
};

#endif // SEMAPHORE_HPP

