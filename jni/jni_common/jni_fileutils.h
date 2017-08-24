/*
 * Copyright (c) 2016 Nanyun
 * Create by Nanyun <tzu.ta.lin@gmail.com>
 */

#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

/*
 *     Author : Nanyun
 * Created on : 07/22 2016
 *
 * Copyright (c) 2016 Nanyun. All rights reserved.
 */
namespace jniutils
{

bool fileExists(const char *name);

bool dirExists(const char *name);

bool fileExists(const std::string &name);

bool dirExists(const std::string &name);

} // end jniutils
  /* FILEUTILS_H */
