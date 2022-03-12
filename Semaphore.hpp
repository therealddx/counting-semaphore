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

/**
 * SemaphoreClient
 *
 * @summary
 * Simple class to represent a client to the Semaphore.
 * SemaphoreClient's hope to access a flag from the Semaphore,
 *   as a prerequisite to doing their work for the application.
 *
 */
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

/**
 * Semaphore
 *
 * @summary
 * Represents a deterministic interface to a group of resources.
 *
 * These resources must must be taken by clients (via public method `Take`),
 *   in order for a client caller to operate,
 * and then given back by clients (via public method `Give`),
 *   after that client caller is done, so as to allow in other client callers.
 *
 */
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

  /**
   * GetFlagMax
   * Trivial accessor returning the number of flag indexes this semaphore offers
   *   (i.e., how many separate threads can have semaphore access at any one time).
   *
   */
  uint32_t GetFlagMax() { return _flag_max; }

  /**
   * Take
   * The calling thread uses this method to take a flag index,
   *   denoting its temporary control over a limited-quantity resource.
   *
   */
  Error Take(SemaphoreClient*, uint32_t = 0, uint32_t = 0);

  /**
   * Give
   * The calling thread uses this method to give back a flag index,
   *   denoting its relinquishing of control over a limited-quantity resource.
   *
   */
  Error Give(SemaphoreClient*);

  /**
   * DumpState
   * The calling thread locks access to the backing data of this class,
   *   and prints out a string representation for that backing data.
   *
   */
  void DumpState();

private:

  // private methods: flag control
  //
  // firstAvailableFlag()
  //   informs the current thread of the first available flag index.
  //   note: verify `numAvailableFlags() > 0` before calling this method.
  //
  // numAvailableFlags() 
  //   informs the current thread of the #available flag indexes.
  //
  // note: these accessors assume '_available_flags' is locked to the executing thread.
  //
  uint32_t firstAvailableFlag();
  uint32_t numAvailableFlags();

  // private data: flag control
  //
  //   _working
  //     maps any given client to this class to their currently flag index.
  //     a client has a flag index of '0' if they were-but-are-no-longer using a flag.
  //
  //   _available_flags
  //     maps any given flag index to whether it's available (true => available, false ow).
  //     valid flag indexes range from [1, _flag_max], endpoint-inclusive.
  //
  //   _flag_max
  //     denotes max amount of available flags
  //
  //   _map_lock
  //     locks access to '_working' and '_available_flags'.
  //
  std::map<SemaphoreClient*, uint32_t> _working;
  std::map<uint32_t, bool>             _available_flags;
  uint32_t                             _flag_max;
  std::mutex                           _map_lock;

  // private members: logging
  // 
  bool canLog() { return ( ! (_log == nullptr || !_log->is_open()) ) ; }
  void doLog(std::string);
  std::ofstream* _log;
  std::mutex _log_lock; // lock for access to the log.

  typedef std::chrono::time_point<std::chrono::steady_clock> Semaphore_time_point;
  Semaphore_time_point _inception_time;
};

#endif // SEMAPHORE_HPP

