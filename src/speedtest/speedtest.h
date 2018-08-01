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

// Unum speedtest include file

#ifndef _SPEEDTEST_H
#define _SPEEDTEST_H

// Performs a speedtest, blocking until complete.
void speedtest_perform();

// Performs a speedtest asynchronously.
// This function starts the speedtest in a new thread and returns immediately.
void cmd_speedtest();

#endif // _SPEEDTEST_H
