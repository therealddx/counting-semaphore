#include "stdinclude.hpp"
#include "Semaphore.hpp"

/**
 * loop_drone
 *   to 'drone' is to 'semaphore take', 'work', and 'semaphore give', in succession.
 *   this function 'drone's on for 'arg_total_drone_time_ms' milliseconds.
 *     while holding onto the Semaphore.
 *
 */
void loop_drone
  ( SemaphoreClient* arg_sem_client
  , Semaphore* arg_sem
  , uint32_t arg_total_drone_time_ms
  , uint32_t arg_work_time_ms
  )
{
  // vars.
  uint32_t drone_time_ms = 0; // tracks roughly against 'arg_total_drone_time_ms'
  uint32_t take_patience_ms = 1000; // represents time lost if timeout while waiting for semaphore
  uint32_t work_time_ms = arg_work_time_ms; // represents time required to do 'work' with the semaphore
  uint32_t sleep_time_ms = arg_work_time_ms; // after working, sleep this long.

  while (drone_time_ms < arg_total_drone_time_ms)
  {
    // try take + error-check.
    Semaphore::Error e_take = arg_sem->Take(arg_sem_client, take_patience_ms, take_patience_ms);
    if (e_take != Semaphore::E_OK)
    {
      drone_time_ms += take_patience_ms;
      continue;
    }
  
    // do fake work (sleep).
    std::this_thread::sleep_for(std::chrono::milliseconds(work_time_ms));
    drone_time_ms += work_time_ms;
  
    // give.
    arg_sem->Give(arg_sem_client);

    // sleep again.
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
    drone_time_ms += sleep_time_ms;
  }

  // ret.
  return;
}

void loop_dump(Semaphore* arg_sem, uint32_t arg_max_timeout_ms)
{
  uint32_t elapsed_ms = 0;

  while (elapsed_ms < arg_max_timeout_ms)
  {
    arg_sem->DumpState();

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    elapsed_ms += 2000;
  }

  return;
}

int main()
{
  // initialize the variables:
  //   Semaphore
  //   SemaphoreClient's
  //   Error codes
  //   Logging
  //
  Semaphore* my_sem = new Semaphore(4, "./semaphore-test.log");

  SemaphoreClient* sc_1 = new SemaphoreClient("sc_1");
  SemaphoreClient* sc_2 = new SemaphoreClient("sc_2");
  SemaphoreClient* sc_3 = new SemaphoreClient("sc_3");
  SemaphoreClient* sc_4 = new SemaphoreClient("sc_4");
  SemaphoreClient* sc_5 = new SemaphoreClient("sc_5");

  // make the threads.
  //
  std::thread t1( &loop_drone, sc_1, my_sem, 10000,  200 );
  std::thread t2( &loop_drone, sc_2, my_sem, 10000,  400 );
  std::thread t3( &loop_drone, sc_3, my_sem, 10000,  500 );
  std::thread t4( &loop_drone, sc_4, my_sem, 10000,  800 );
  std::thread t5( &loop_drone, sc_5, my_sem, 10000, 1000 );
  std::thread tdump ( &loop_dump, my_sem, 10000 );

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  tdump.join();

  // cleanup.
  delete my_sem; // my_sem holds agg. references to SemaphoreClient's... detach those agg.'s
  delete sc_1;
  delete sc_2;
  delete sc_3;
  delete sc_4;
  delete sc_5;

  return 0;
}

/*
 * // basic take/give
 *  
 *   my_sem->Take(sc_1,    0,    0);
 *   my_sem->Take(sc_2,    0,    0);
 *   my_sem->Take(sc_3,    0,    0);
 *   my_sem->Take(sc_4,    0,    0);
 *   my_sem->Take(sc_5, 3000, 1000);
 * 
 *   my_sem->Give(sc_1);
 *   my_sem->Give(sc_2);
 *   my_sem->Give(sc_3);
 *   my_sem->Give(sc_4);
 *   my_sem->Give(sc_5);
 *
 */


