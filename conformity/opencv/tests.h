// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_
#define INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_

#include "common.h"

#if MANAGER
int run_tests(RecreatedMessageQueue& request_queue,
              RecreatedMessageQueue& reply_queue);
#else
void wait_for_requests(OpenedMessageQueue& request_queue,
                       OpenedMessageQueue& reply_queue);
#endif

#endif  // INTRINSICCV_OPENCV_CONFORMITY_TESTS_H_
