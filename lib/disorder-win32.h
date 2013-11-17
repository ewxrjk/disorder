/*
 * This file is part of DisOrder.
 * Copyright (C) 2013 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file lib/disorder-win32.h
 * @brief Windows support
 */
#ifndef DISORDER_WIN32_H
# define DISORDER_WIN32_H

#define HAVE_WS2TCPIP_H 1
#define HAVE_SHLOBJ_H 1
#define HAVE_BCRYPT_H 1
#define HAVE_CLOSESOCKET 1

#include <WinSock2.h>
#include <io.h>

#define access _access /* quieten compiler */

#include <time.h>

#define attribute(x) /* nothing */
#define declspec(x) __declspec(x)
#define inline __inline
#define DEFAULT_SOX_GENERATION 1
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define S_ISREG(mode) ((mode) & _S_IFREG)
#define S_ISDIR(mode) ((mode) & _S_IFDIR)
#define strcasecmp _stricmp

#ifdef _WIN64
typedef long long ssize_t;
#else
typedef long ssize_t;
#endif
typedef int socklen_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int pid_t;

struct timezone;

int gettimeofday(struct timeval *tv, struct timezone *tz);
char *win_wtomb(const wchar_t *ws);
void network_init(void);

#define socket_error() (WSAGetLastError())
#define system_error() (GetLastError())

#pragma comment(lib, "Ws2_32.lib")

#endif