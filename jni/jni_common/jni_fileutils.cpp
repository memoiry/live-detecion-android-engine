/*
 * Copyright (c) 2016 Nanyun
 * Create by Nanyun <tzu.ta.lin@gmail.com>
 */

#include "jni_fileutils.h"

/*
 *     Author : Nanyun
 * Created on : 07/22 2016
 *
 * Copyright (c) 2016 Nanyun. All rights reserved.
 */
namespace jniutils
{

bool fileExists(const char *name)
{
  std::ifstream ifs(name);
  return ifs.good();
}

bool dirExists(const char *name)
{
  struct stat file_info;
  if (stat(name, &file_info) != 0)
    return false;
  return (file_info.st_mode & S_IFDIR) != 0;
}

bool fileExists(const std::string &name) { return fileExists(name.c_str()); }

bool dirExists(const std::string &name) { return dirExists(name.c_str()); }

} // end jniutils
/* FILEUTILS_H */
