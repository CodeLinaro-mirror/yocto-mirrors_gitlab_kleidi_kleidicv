// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <getopt.h>
#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include "framework/utils.h"

namespace test {

size_t Options::vector_length_{16};

/// Parses command line arguments.
static void parse_arguments(int argc, char **argv) {
  // clang-format off
  static struct option long_options[] = {
    {"vector-length", required_argument, nullptr, 'v'},
    {0, 0, nullptr, 0}
  };
  // clang-format on

  for (;;) {
    int option_index = 0;
    int opt = getopt_long(argc, argv, "", long_options, &option_index);
    if (opt == -1) {
      break;
    }

    switch (opt) {
      default:
        std::cerr << "Unknown command line argument." << std::endl;
        break;

      case 'v':
        Options::set_vector_length(std::stoi(optarg));
        break;
    }
  }

  std::cout << "Vector length is set to " << Options::vector_length()
            << " bytes." << std::endl;
}

}  // namespace test

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  test::parse_arguments(argc, argv);
  return RUN_ALL_TESTS();
}
