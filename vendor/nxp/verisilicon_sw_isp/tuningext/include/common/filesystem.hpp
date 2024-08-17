/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

#include <algorithm>
#include <dirent.h>
#include <iostream>
#include <list>
#include <string>
#include <sys/stat.h>

namespace fs {

inline bool isExists(const std::string &name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

inline int ls(const std::string path, const std::string ext,
              std::list<std::string> &list) {
  DIR *pDir;

  struct dirent *pEnt;

  if ((pDir = opendir(path.c_str())) != NULL) {
    while ((pEnt = readdir(pDir)) != NULL) {
      std::string fileName = pEnt->d_name;

      if (ext.empty()) {
        list.push_back(fileName);
      } else if (fileName.substr(fileName.find_last_of(".") + 1) == ext) {
        list.push_back(fileName);
      }
    }

    list.sort();

    closedir(pDir);
  } else {
    return EXIT_FAILURE;
  }

  return 0;
} // namespace fs

inline void mkdirP(const std::string dir) {
  char tmp[256];

  snprintf(tmp, sizeof(tmp), "%s", dir.c_str());

  size_t len = dir.length();

  if (tmp[len - 1] == '/') {
    tmp[len - 1] = 0;
  }

  char *p = NULL;

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;

      int ret = mkdir(tmp, S_IRWXU);
      if (ret) {
        std::cout << "mkdir failed: " << tmp << std::endl;
      }

      *p = '/';
    }
  }

  mkdir(tmp, S_IRWXU);
}

} // namespace fs
