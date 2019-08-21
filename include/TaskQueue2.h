/*
 * Copyright 2016-2017 MICRORISC s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>

/// \class TaskQueue
/// \brief Maintain queue of tasks and invoke sequential processing
/// \details
/// Provide asynchronous processing of incoming tasks of type T in dedicated worker thread.
/// The tasks are processed in FIFO way. Processing function is passed as parameter in constructor.
template <class T>
class TaskQueue
{
public:
  /// Processing function type
  typedef std::function<bool(T)> ProcessTaskFunc;

  /// \brief constructor
  /// \param [in] processTaskFunc processing function
  /// \details
  /// Processing function is used in dedicated worker thread to process incoming queued tasks.
  /// The function must be thread safe. The worker thread is started.
  TaskQueue(ProcessTaskFunc processTaskFunc)
    :m_processTaskFunc(processTaskFunc)
  {
    m_processTask = false;
    m_suspended = true;
    m_runWorkerThread = true;
    m_workerThread = std::thread(&TaskQueue::worker, this);
  }

  /// \brief destructor
  /// \details
  /// Stops working thread
  virtual ~TaskQueue()
  {
    stopQueue();
    m_processTaskFunc = nullptr;
  }

  /// \brief Push task to queue
  /// \param [in] task object to push to queue
  /// \return size of queue
  /// \details
  /// Pushes task to queue to be processed in worker thread. The task type T has to be copyable
  /// as the copy is pushed to queue container
  int pushToQueue(const T& task)
  {
    int retval = 0;
    {
      std::unique_lock<std::mutex> lck(m_taskQueueMutex);
      m_taskQueue.push(task);
      retval = static_cast<uint8_t>(m_taskQueue.size());
      m_processTask = true;
    }
    m_conditionVariable.notify_all();
    return retval;
  }

  /// \brief Stop queue
  /// \details
  /// Worker thread is explicitly stopped
  void stopQueue()
  {
    {
      std::unique_lock<std::mutex> lck(m_taskQueueMutex);
      m_runWorkerThread = false;
      m_processTask = true; // get out from waiting
      m_suspended = false;
    }
    m_conditionVariable.notify_all();

    if (m_workerThread.joinable())
      m_workerThread.join();
  }

  void suspend()
  {
    std::unique_lock<std::mutex> lck(m_taskQueueMutex);
    m_suspended = true;
  }

  void recover()
  {
    std::unique_lock<std::mutex> lck(m_taskQueueMutex);
    m_suspended = false;
    m_processTask = true;
    m_conditionVariable.notify_all();
  }

  bool isSuspended()
  {
    std::unique_lock<std::mutex> lck(m_taskQueueMutex);
    return m_suspended;
  }
  
  /// \brief Get actual queue size
  /// \return queue size
  size_t size()
  {
    size_t retval = 0;
    {
      std::unique_lock<std::mutex> lck(m_taskQueueMutex);
      retval = m_taskQueue.size();
    }
    return retval;
  }

private:
  /// Worker thread function
  void worker()
  {
    std::unique_lock<std::mutex> lck(m_taskQueueMutex, std::defer_lock);

    while (m_runWorkerThread) {

      //wait for something in the queue
      lck.lock();
      m_conditionVariable.wait(lck, [&] { return m_processTask && !m_suspended; }); //lock is released in wait
      //lock is reacquired here
      m_processTask = false;

      while (m_runWorkerThread) {
        if (!m_taskQueue.empty() && !m_suspended) {
          auto task = m_taskQueue.front();
          // m_taskQueue.pop();
          lck.unlock();
          bool processed = m_processTaskFunc(task);
          lck.lock();
          if (processed) {
            m_taskQueue.pop();
          }
        }
        else {
          lck.unlock();
          break;
        }
        //lck.lock(); //lock for next iteration
      }
    }
  }

  std::mutex m_taskQueueMutex;
  std::condition_variable m_conditionVariable;
  std::queue<T> m_taskQueue;
  bool m_processTask;
  bool m_suspended;
  bool m_runWorkerThread;
  std::thread m_workerThread;

  ProcessTaskFunc m_processTaskFunc;
};
