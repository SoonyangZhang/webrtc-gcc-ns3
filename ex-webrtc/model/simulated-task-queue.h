/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#pragma once

#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "webrtc-simu-controller.h"

namespace webrtc {

class SimulatedTaskQueue : public TaskQueueBase,
                           public sim_time_impl::SimulatedSequenceRunner {
 public:
  SimulatedTaskQueue(sim_time_impl::SimulatedTimeControllerImpl* handler,
                     absl::string_view name);

  ~SimulatedTaskQueue();

  void RunReady(Timestamp at_time) override;

  Timestamp GetNextRunTime() const override {
    rtc::CritScope lock(&lock_);
    return next_run_time_;
  }
  TaskQueueBase* GetAsTaskQueue() override { return this; }

  // TaskQueueBase interface
  void Delete() override;
  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  sim_time_impl::SimulatedTimeControllerImpl* const handler_;
  // Using char* to be debugger friendly.
  char* name_;

  rtc::CriticalSection lock_;

  std::deque<std::unique_ptr<QueuedTask>> ready_tasks_ RTC_GUARDED_BY(lock_);
  std::map<Timestamp, std::vector<std::unique_ptr<QueuedTask>>> delayed_tasks_
      RTC_GUARDED_BY(lock_);

  Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();
};

}  // namespace webrtc

