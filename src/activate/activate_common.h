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

// unum router activate include file

#ifndef _ACTIVATE_COMMON_H
#define _ACTIVATE_COMMON_H

// The first try to activate is random at 0-ACTIVATE_PERIOD_START)
#define ACTIVATE_PERIOD_START   0
// After each failure the delay is incremented by random from
// 0-ACTIVATE_PERIOD_INC range
#define ACTIVATE_PERIOD_INC    30
// Ater reaching ACTIVATE_MAX_PERIOD the delay stays there
#define ACTIVATE_MAX_PERIOD   600

// This function blocks the caller till device is activated.
void wait_for_activate(void);

// This function returns TRUE if the agent is activated, FALSE otherwise.
int is_agent_activated(void);

// Set the activate event (for tests to bypass the activate step)
void set_activate_event(void);

// Subsystem init function
int activate_init(int level);

#endif // _ACTIVATE_COMMON_H
