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
 
#include <efi.h>
#include <efilib.h>

#include "utils.h"

static CHAR8* strchra(CHAR8 *s, CHAR8 c);

EFI_STATUS efi_set_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 *buf, UINTN size, BOOLEAN persistent) {
	UINT32 flags;
	
	flags = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
	if (persistent) {
		flags |= EFI_VARIABLE_NON_VOLATILE;
	}
	
	return uefi_call_wrapper(RT->SetVariable, 5, name, (EFI_GUID *)vendor, flags, size, buf);
}

EFI_STATUS efi_get_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 **buffer, UINTN *size) {
	CHAR8 *buf;
	UINTN l;
	EFI_STATUS err;

	l = sizeof(CHAR16 *) * EFI_MAXIMUM_VARIABLE_SIZE;
	buf = AllocatePool(l);
	if (!buf) {
		return EFI_OUT_OF_RESOURCES;
	}

	err = uefi_call_wrapper(RT->GetVariable, 5, name, (EFI_GUID *)vendor, NULL, &l, buf);
	if (!EFI_ERROR(err)) {
		*buffer = buf;
		if (size) {
			*size = l;
		}
	} else {
		FreePool(buf);
	}
	
	return err;
}

VOID DisplayColoredText(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_YELLOW|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}

VOID DisplayErrorText(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_RED|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}

CHAR8* UTF16toASCII(CHAR16 *InString, UINTN InLength) {
	CHAR8 *OutString, *InAs8;
	UINTN i = 0;
	
	OutString = AllocateZeroPool(InLength * sizeof(CHAR8));
	InAs8 = (CHAR8*)InString;
	while ((InAs8[i * 2] != '\0') && (i < InLength)) {
		OutString[i] = InAs8[i * 2];
		i++;
	}
	return OutString;
}

BOOLEAN FileExists(EFI_FILE_HANDLE dir, CHAR16 *name) {
	EFI_FILE_HANDLE handle;
	EFI_STATUS err;

	err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, NULL);
	if (EFI_ERROR(err)) {
		goto out;
	}

	uefi_call_wrapper(handle->Close, 1, handle);
	return TRUE;
out:
	return FALSE;
}

UINTN FileRead(EFI_FILE_HANDLE dir, const CHAR16 *name, CHAR8 **content) {
	EFI_FILE_HANDLE handle;
	EFI_FILE_INFO *info;
	CHAR8 *buf;
	UINTN buflen;
	EFI_STATUS err;
	UINTN len = 0;
	
	err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, NULL);
	if (EFI_ERROR(err)) {
		goto out;
	}
	
	info = LibFileInfo(handle);
	buflen = info->FileSize+1;
	buf = AllocatePool(buflen);
	
	err = uefi_call_wrapper(handle->Read, 3, handle, &buflen, buf);
	if (EFI_ERROR(err) == EFI_SUCCESS) {
		buf[buflen] = '\0';
		*content = buf;
		len = buflen;
	} else {
		FreePool(buf);
	}
	
	FreePool(info);
	uefi_call_wrapper(handle->Close, 1, handle);
out:
	return len;
}

// This code has been adapted from gummiboot. Thanks, guys!
static CHAR8 *strchra(CHAR8 *s, CHAR8 c) {
	do {
		if (*s == c) {
			return s;
		}
	} while (*s++);
	return NULL;
}

CHAR8* GetConfigurationKeyAndValue(CHAR8 *content, UINTN *pos, CHAR8 **key_ret, CHAR8 **value_ret) {
	CHAR8 *line;
	UINTN linelen;
	CHAR8 *value;

skip:
	line = content + *pos;
	if (*line == '\0') {
		return NULL;
	}

	linelen = 0;
	while (line[linelen] && !strchra((CHAR8 *)"\n\r", line[linelen])) {
		linelen++;
	}

	/* Move the position to the next line. */
	*pos += linelen;
	if (content[*pos]) {
		(*pos)++;
	}

	/* The line has no length, so it is empty. */
	if (linelen == 0) {
		goto skip;
	}

	/* Add a string terminator character. */
	line[linelen] = '\0';

	/* Remove leading and trailing whitespace. */
	while (strchra((CHAR8 *)" \t", *line)) {
		line++;
		linelen--;
	}

	while (linelen > 0 && strchra((CHAR8 *)" \t", line[linelen-1])) {
		linelen--;
	}
	line[linelen] = '\0';

	if (*line == '#') {
		goto skip;
	}

	/* Split the key and the value. */
	value = line;
	while (*value && !strchra((CHAR8 *)" \t", *value)) {
		value++;
	}
	
	if (*value == '\0') {
		goto skip;
	}
	
	*value = '\0';
	value++;
	while (*value && strchra((CHAR8 *)" \t", *value)) {
		value++;
	}

	*key_ret = line;
	*value_ret = value;
	return line;
}
