// Copyright 2019 - 2020 Minim Inc
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

// unum helper utils, mutex wrappers
// default implementation, platform might require its own

#ifndef _UTIL_MUTEX_H
#define _UTIL_MUTEX_H


// Type for using mutexes internally
typedef pthread_mutex_t UTIL_MUTEX_t;


// The most common case
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)

// Init mutex, dynamic and static init, mutex has to be recursive
#define UTIL_MUTEX_INIT(_m)  \
     { pthread_mutexattr_t _attr; pthread_mutexattr_init(&_attr);     \
       pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE_NP); \
       pthread_mutex_init(_m, &_attr); }
#define UTIL_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

// Some platforms define the type and initilizer without '_NP' suffix
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)

// Init mutex, dynamic and static init, mutex has to be recursive
#define UTIL_MUTEX_INIT(_m)  \
     { pthread_mutexattr_t _attr; pthread_mutexattr_init(&_attr);  \
       pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE); \
       pthread_mutex_init(_m, &_attr); }
#define UTIL_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER

#else  // Can't find recursive mutex static initializer
#  error No PTHREAD_RECURSIVE_MUTEX_INITIALIZER... defines!
#endif //

// Take mutex
#define UTIL_MUTEX_TAKE(_m) pthread_mutex_lock(_m)
// Release mutex
#define UTIL_MUTEX_GIVE(_m) pthread_mutex_unlock(_m)

#endif // _UTIL_MUTEX_H
