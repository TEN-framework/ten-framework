//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/terminal.h"

#include <Windows.h>
#include <io.h>

size_t ten_terminal_get_width_in_char(void) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  int ret = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  if (ret) {
    return csbi.dwSize.X;
  }

  return 0;
}

int ten_terminal_is_terminal(int fd) { return _isatty(fd); }
