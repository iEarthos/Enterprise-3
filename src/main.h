/*
 * Tool intended to help facilitate the process of booting Linux on Intel
 * Macintosh computers made by Apple from a USB stick or similar.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * Copyright (C) 2013 SevenBits
 *
 */

#pragma once
#ifndef _main_h
#define _main_h

typedef struct LinuxBootOption {
	CHAR8 *name;
	CHAR8 *file_name;
	CHAR8 *distro_family;
	CHAR8 *kernel_path;
	CHAR8 *initrd_path;
	CHAR8 *boot_folder;
} LinuxBootOption;

typedef struct BootableLinuxDistro {
	LinuxBootOption *bootOption;
	struct BootableLinuxDistro *next;
} BootableLinuxDistro;

EFI_STATUS BootLinuxWithOptions(CHAR16 *params);

#endif
