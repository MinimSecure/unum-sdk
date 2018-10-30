// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// process monitor include file that pulls in everything else

#ifndef _MONITOR_H
#define _MONITOR_H


// Monitor's crash dump uploader timeout (in seconds)
#define PROCESS_MONITOR_DUMP_UPLOAD_TIMEOUT 60

// Amazon S3 URL to upload the crash data
#define PROCESS_MONITOR_DUMP_URL \
  "https://s3.amazonaws.com/crash-reports.minim.co"

// Monitor's crash dump uploader retry time (in seconds)
#define PROCESS_MONITOR_DUMP_UPLOAD_RETRY_TIME 10

// Monitor's time to wait for all the terminating process group
// to complete gracefully, till issuing the kill(-9) call (in seconds)
#define MONITOR_TRMINATE_KILL_TIME 10;

// Monitor's time to wait for all the terminating process group
// to complete after the kill(-9) call (in seconds)
#define MONITOR_TRMINATE_KILL_WAIT_TIME 5;

// Forks when parent process starts monitoring the child process.
// The parent process control never leaves the function. The child
// returns and proceeds to run the remaining code.
// log_dst - allows to set monitor log destination (i.e. if using
//           the monitor for multiple different processes)
// pid_suffix - suffix for the PID file of the monitoring process
void process_monitor(int log_dst, char *pid_suffix);

#endif // _MONITOR_H
