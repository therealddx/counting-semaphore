#include "Semaphore.hpp"
#include <sstream>

Semaphore::Semaphore(uint32_t arg_flag_max, std::string arg_log_path)
{
  // assign the max count.
  //
  _flag_max = arg_flag_max;

  // assign all flags as initially available.
  // 
  for (uint32_t n = 1; n <= _flag_max; n++)
  {
    _available_flags[n] = true; // performs `std::map<T>::insert`
  }

  // assign the std::ofstream as log.
  //
  if (arg_log_path == "") // if equal to default-optional (""); do nothing.
  {
    _log = nullptr;
  }
  else // else, try-open.
  {
    _log = new std::ofstream(arg_log_path, std::ios_base::trunc);

    if (_log->is_open()) // if open, `operator<<`.
    {
      (*_log)
        << "+-------------------+" << std::endl
        << "|   Semaphore Log   |" << std::endl
        << "+-------------------+" << std::endl;
    }
  }
}

Semaphore::~Semaphore()
{
  // force out all working semaphore clients.
  for (auto each : _working)
  {
    Give(each.first);
  }

  // all other members are stack-allocated and/or primitives.
  // of primary import is to clear out the OO references,
  //   as is done above.
  //

  // close the log, if it was instantiated.
  if (_log != nullptr)
  {
    if (_log->is_open()) { _log->close(); }
    delete _log;
  }
}

Semaphore::Error Semaphore::Take
  ( SemaphoreClient* arg_client
  , uint32_t arg_timeout_ms
  , uint32_t arg_dtimeout_ms
  )
{
  // null-check.
  if (arg_client == nullptr) { return Semaphore::E_TakeFail; }

  // lock the maps to the current thread.
  //
  _map_lock.lock();

  // if 'arg_client' is already in '_working':
  //   (map::find doesn't return end() -- denotes client is present)
  //   (flag index for that client isn't 0 -- denotes client is using a flag index)
  //   exit: already have a spot
  //
  if (_working.find(arg_client) != _working.end() && _working[arg_client] != 0)
  {
    if (canLog())
    {
      std::stringstream ss;

      ss << 
        "Semaphore::Take: client '" << arg_client->GetName() << "'" <<
        " already has flag '" << _working[arg_client] << "'" << std::endl;

      doLog(ss.str());
    }

    // unlock maps to other threads + ret.
    _map_lock.unlock();
    return Semaphore::E_TakeFail;
  }

  // block this thread for a maximum of'arg_timeout_ms',
  //   in increments of 'arg_dtimeout_ms'.
  //
  // note:
  //   blocking while '_map_lock' is active is not so shameful,
  //     since any other clients waiting on '_map_lock' would end up here too.
  //   but, should the time spent at `_map_lock.lock` count against 'arg_timeout_ms'?
  // 
  uint32_t wait_so_far = 0;
  while (numAvailableFlags() == 0 && wait_so_far < arg_timeout_ms)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(arg_dtimeout_ms));
    wait_so_far += arg_dtimeout_ms;

    if (canLog())
    {
      std::stringstream ss;
      ss << "Semaphore::Take: client '" << arg_client->GetName() << "' waiting: "
        << wait_so_far << "ms..." << std::endl;
      doLog(ss.str());
    }
  }

  // if there's still no available flags after waiting:
  if (numAvailableFlags() == 0)
  {
    if (canLog())
    {
      std::stringstream ss;

      ss <<
        "Semaphore::Take: client '" << arg_client->GetName() << "'" <<
        " timed out waiting after '" << arg_timeout_ms << "' ms" << std::endl;

      doLog(ss.str());
    }

    // unlock the maps + ret.
    _map_lock.unlock();
    return Semaphore::E_TakeFail;
  }

  // log.
  if (canLog())
  {
    std::stringstream ss;

    ss << 
       "Semaphore::Take: " << "flag" << " " <<
         "'" << firstAvailableFlag() << "'" << " " <<
       "to client" << " " <<
         "'" << arg_client->GetName() << "'" <<
       " ('" << (numAvailableFlags() - 1) << "' available)" <<
       std::endl;

    doLog(ss.str());
  }

  // execute the 'take'.
  //   the client is assigned the first available flag.
  //   that flag gets marked as 'not available'.
  // 
  _working[arg_client] = firstAvailableFlag();
  _available_flags[firstAvailableFlag()] = false;

  // unlock the maps + ret.
  _map_lock.unlock();
  return Semaphore::E_OK;
}

Semaphore::Error Semaphore::Give(SemaphoreClient* arg_sem_client)
{
  // null-check.
  if (arg_sem_client == nullptr) { return Semaphore::E_GiveFail; }

  // lock the maps to the current thread.
  //
  _map_lock.lock();

  // if 'arg_sem_client' isn't in '_working':
  //   (map::find returns end() -- denotes client is not present)
  //   (flag index for that client is 0 -- denotes client is not present)
  //   exit: must have a spot in order to enter.
  //
  if (_working.find(arg_sem_client) == _working.end() || _working[arg_sem_client] == 0)
  {
    if (canLog())
    {
      std::stringstream ss;
      ss << "Semaphore::Give: client '" << arg_sem_client->GetName() << "' has no flag" << std::endl;
      doLog(ss.str());
    }

    // unlock maps to other threads + ret.
    _map_lock.unlock();
    return Semaphore::E_GiveFail;
  }

  // log.
  if (canLog())
  {
    std::stringstream ss;

    ss << 
       "Semaphore::Give: " << "flag" << " " <<
         "'" << _working[arg_sem_client] << "'" << " " <<
       "from client" << " " <<
         "'" << arg_sem_client->GetName() << "'" <<
       " ('" << (numAvailableFlags() + 1) << "' available)" <<
       std::endl;

    doLog(ss.str());
  }

  // execute the 'give':
  //   the flag index for 'arg_sem_client'-- mark it as available now.
  //   mark 'arg_sem_client' as, no longer using any flag index.
  //
  // note:
  //   `std::map<>::erase` might be cleaner, but it throws a segv.
  //   figure out why...?
  //
  _available_flags[_working[arg_sem_client]] = true;
  _working[arg_sem_client] = 0;

  // unlock the maps to other threads + ret.
  _map_lock.unlock();
  return Semaphore::E_OK;
}

void Semaphore::DumpState()
{
  // lock the maps to this thread.
  //   this method requires a known state for the flag indexes.
  //
  _map_lock.lock();

  // vars.
  std::stringstream ss;

  // compile and log the talkout string.
  //
  ss << "*****************************************************" << std::endl;
  ss << "*** client name : flag" << " ('" << numAvailableFlags() << "' flags available)" << std::endl;
  for (auto each : _working)
  {
    ss << "*** " << each.first->GetName() << " : " << each.second << std::endl;
  }
  ss << "*****************************************************" << std::endl;

  doLog(ss.str());

  // unlock the maps to other threads.
  _map_lock.unlock();
}

void Semaphore::doLog(std::string arg_str)
{
  if (_log == nullptr || !_log->is_open())
  {
    // do nothing.
    //
    // this if-structure, and delegation in general to `canLog` and `doLog`,
    //   helps implement the optional debug logging.
    //
    // when enabled,
    //   debug logging is simple to inject into member functions.
    //
    // when not enabled,
    //   debug logging is skipped with `canLog` and this if-structure;
    //     and no time is wasted compiling and outputting log messages.
    // 
  }
  else
  {
    _log_lock.lock();

    (*_log) << arg_str;
    _log->flush();

    _log_lock.unlock();
  }
}

uint32_t Semaphore::firstAvailableFlag()
{
  for (auto each : _available_flags) // <uint32_t, bool>
  {
    if (each.second == true) // denotes 'available'
    {
      return each.first; // return that flag index
    }
  }

  // this should be unreachable...
  //   verify `numAvailableFlags() > 0` before entry.
  //
  return 0;
}

uint32_t Semaphore::numAvailableFlags()
{
  uint32_t rtn_count = 0;

  for (auto each : _available_flags) // <uint32_t, bool>
  {
    if (each.second) { rtn_count++; }
  }

  return rtn_count;
}

