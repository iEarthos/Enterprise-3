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

#ifndef _utils_h
#define _utils_h

EFI_STATUS efi_set_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 *buf, UINTN size, BOOLEAN persistent);
EFI_STATUS efi_get_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 **buffer, UINTN *size);
CHAR8* UTF16toASCII(CHAR16 *InString, UINTN InLength);
VOID display_colored_text(CHAR16 *string);
VOID display_error_text(CHAR16 *string);

#endif
