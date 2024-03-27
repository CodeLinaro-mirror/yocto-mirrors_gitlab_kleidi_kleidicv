// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "common.h"
#include "tests.h"

int main(void) {
  OpenedSharedMemory sm{SHM_ID, SHM_SIZE};
  OpenedMessageQueue request_queue{REQUEST_MQ_ID, sm};
  OpenedMessageQueue reply_queue{REPLY_MQ_ID, sm};

  wait_for_requests(request_queue, reply_queue);

  std::cout << "Subordinate exits normally" << std::endl;
}
