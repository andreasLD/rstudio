/*
 * ChildProcessSubprocPoll.cpp
 *
 * Copyright (C) 2009-17 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include "ChildProcessSubprocPoll.hpp"

namespace rstudio {
namespace core {
namespace system {

namespace {

boost::posix_time::ptime now()
{
   return boost::posix_time::microsec_clock::universal_time();
}

// how long does memory of "recent output" last?
const boost::posix_time::seconds kResetRecentDelay =
                                          boost::posix_time::seconds(1);

// what is the minimum length of time before we check for subprocesses?
const boost::posix_time::milliseconds kCheckSubprocDelay =
                                          boost::posix_time::milliseconds(200);

} // anonymous namespace

ChildProcessSubprocPoll::ChildProcessSubprocPoll(PidType pid, boost::function<bool (PidType pid)> subProcCheck)
   :
     pid_(pid),
     checkSubProcAfter_(boost::posix_time::not_a_date_time),
     resetRecentOutputAfter_(boost::posix_time::not_a_date_time),
     hasSubprocess_(true),
     hasRecentOutput_(true),
     stopped_(false),
     subProcCheck_(subProcCheck)
{
}

void ChildProcessSubprocPoll::poll(bool hadOutput)
{
   if (stopped_)
      return;

   boost::posix_time::ptime currentTime = now();

   // Update state of "hasRecentOutput". We remember that we saw output for
   // up to "kResetRecentDelay" milliseconds.
   if (hadOutput)
   {
      hasRecentOutput_ = true;
      resetRecentOutputAfter_ = boost::posix_time::not_a_date_time;
   }

   if (resetRecentOutputAfter_.is_not_a_date_time())
   {
      resetRecentOutputAfter_ = currentTime + kResetRecentDelay;
   }
   else if (currentTime > resetRecentOutputAfter_)
   {
      hasRecentOutput_ = false;
      resetRecentOutputAfter_ = currentTime + kResetRecentDelay;
   }

   if (!subProcCheck_)
      return;

   // Update state of "hasSubprocesses". We do this no more often than every
   // "kCheckSubprocDelay" milliseconds, and less if we haven't seen any
   // recent output. The latter is to reduce load when nothing is happening,
   // under the assumption that if all child processes are terminated, we
   // will always see output in the form of the command prompt.
   if (!hasRecentOutput())
   {
      checkSubProcAfter_ = currentTime + kCheckSubprocDelay;
      return;
   }

   if (checkSubProcAfter_.is_not_a_date_time())
   {
      checkSubProcAfter_ = currentTime + kCheckSubprocDelay;
      return;
   }

   if (currentTime <= checkSubProcAfter_)
      return;

   // Enough time has passed, update whether "pid" has subprocesses
   // and restart the timer.
   hasSubprocess_ = subProcCheck_(pid_);
   checkSubProcAfter_ = currentTime + kCheckSubprocDelay;
}

void ChildProcessSubprocPoll::stop()
{
   stopped_ = true;
}

bool ChildProcessSubprocPoll::hasSubprocess() const
{
   return hasSubprocess_;
}

bool ChildProcessSubprocPoll::hasRecentOutput() const
{
   return hasRecentOutput_;
}

} // namespace system
} // namespace core
} // namespace rstudio
