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
        << "+++++++++++++++++++++" << std::endl
        << "+++ Semaphore Log +++" << std::endl
        << "+++++++++++++++++++++" << std::endl;
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

  // close the log.
  //
  if (_log != nullptr)
  {
    if (_log->is_open()) { _log->close(); }
    delete _log;
  }
}

Semaphore::Error Semaphore::Take(SemaphoreClient* arg_client, uint32_t arg_timeout_ms, uint32_t arg_dtimeout_ms)
{
  // null-check.
  if (arg_client == nullptr) { return Semaphore::E_TakeFail; }

  // lock the map.
  //   require faithful reads and polls on who's available and working.
  //
  _map_lock.lock();

  // if 'arg_client' is already in '_working':
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

    // unlock map + ret.
    _map_lock.unlock();
    return Semaphore::E_TakeFail;
  }

  // not a fan of doing anything more complex than this.
  // reason being--
  //   using a `std::unique_lock<T>::try_lock_for` would only be a guarantee
  //     of "at least" 'arg_timeout_ms', per cppreference.
  //   so, it wouldn't be a "more exact" wait.
  //
  // this approach, though (total time/delta time) grants me a little extra freedom.
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

    // unlock the map + ret.
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

  _working[arg_client] = firstAvailableFlag();
  _available_flags[firstAvailableFlag()] = false;

  // unlock the map.
  _map_lock.unlock();

  // ret.
  return Semaphore::E_OK;
}

Semaphore::Error Semaphore::Give(SemaphoreClient* arg_sem_client)
{
  // null-check.
  if (arg_sem_client == nullptr) { return Semaphore::E_GiveFail; }

  // lock the map.
  //   require faithful reads and polls on '_flag'.
  //
  _map_lock.lock();

  // if 'arg_sem_client' isn't in '_working':
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

    // unlock.
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

  // todo:
  //   `std::map<>::erase` would be cleaner, but it throws a segv.
  //   figure out why...?
  //
  // member '_flag' ranges [1..<_flag_max>].
  //   so, '0' is appropriate to denote "no client assigned" for a flag value.
  //
  _available_flags[_working[arg_sem_client]] = true;
  _working[arg_sem_client] = 0;

  // unlock.
  _map_lock.unlock();

  // ret.
  return Semaphore::E_OK;
}

void Semaphore::DumpState()
{
  _map_lock.lock();

  std::stringstream ss;

  ss << "*****************************************************" << std::endl;
  ss << "*** client name : flag" << " ('" << numAvailableFlags() << "' flags available)" << std::endl;
  for (auto each : _working)
  {
    ss << "*** " << each.first->GetName() << " : " << each.second << std::endl;
  }
  ss << "*****************************************************" << std::endl;

  doLog(ss.str());

  _map_lock.unlock();
}

void Semaphore::doLog(std::string arg_str)
{
  if (_log == nullptr || !_log->is_open())
  {
    // do nothing
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
  uint32_t rtn_flag = 0;

  for (auto each : _available_flags)
  {
    if (each.second == true)
    {
      return each.first;
    }
  }

  // this should be unreachable... verify `numAvailableFlags > 0` before entry.
  return 0;
}

uint32_t Semaphore::numAvailableFlags()
{
  uint32_t rtn_count = 0;

  for (auto each : _available_flags)
  {
    if (each.second) { rtn_count++; }
  }

  return rtn_count;
}

