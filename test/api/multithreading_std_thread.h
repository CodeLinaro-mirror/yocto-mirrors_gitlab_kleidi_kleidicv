// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_THREAD_MULTITHREADING_STD_THREAD_H
#define KLEIDICV_THREAD_MULTITHREADING_STD_THREAD_H

#include "kleidicv_thread/kleidicv_thread.h"

/// Get a multithreading implementation based on std::thread.
kleidicv_thread_multithreading get_multithreading_std_thread(
    uintptr_t thread_count);

#endif  // KLEIDICV_THREAD_MULTITHREADING_STD_THREAD_H
