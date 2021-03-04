//   Copyright 2015 Ansersion
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#ifndef __UTILS_HEADER__
#define __UTILS_HEADER__

#include "jrtplib3/rtptypes.h"

#ifdef _MSC_VER
#define ssize_t int
#else
#include <unistd.h>
#endif
#include <errno.h>

#define MD5_SIZE 	32
#define MD5_BUF_SIZE 	(MD5_SIZE + sizeof('\0'))

#ifdef _MSC_VER
/* Read line by line.
 * See also:
 * <<Unix Network Programming>>*/
ssize_t ReadLine(SOCKET fd, void * buf, int maxlen);

/* Read n bytes
 * See also:
 * <<Unix Network Programming>>*/
ssize_t Readn(SOCKET fd, void *vptr, int n);

/* Write n bytes.
 * See also:
 * <<Unix Network Programming>>*/
ssize_t Writen(SOCKET fd, const void *vptr, int n);
#else
/* Read line by line.
* See also:
* <<Unix Network Programming>>*/
ssize_t ReadLine(int fd, void * buf, int maxlen);

/* Read n bytes
* See also:
* <<Unix Network Programming>>*/
ssize_t Readn(int fd, void *vptr, int n);

/* Write n bytes.
* See also:
* <<Unix Network Programming>>*/
ssize_t Writen(int fd, const void *vptr, int n);
#endif

/*
param: 
output: must sizeof(output) >= 32
   */
int Md5sum32(void * input, unsigned char * output, size_t input_size, size_t output_size);

#endif
