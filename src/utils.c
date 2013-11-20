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

VOID display_colored_text(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_YELLOW|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}

VOID display_error_text(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_RED|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}
