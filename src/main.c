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

#include "main.h"
#include "menu.h"
#define banner L"Welcome to Enterprise! - Version %d.%d\n"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

static EFI_STATUS console_text_mode(VOID);

EFI_LOADED_IMAGE *this_image = NULL;
EFI_FILE *root_dir;

EFI_DEVICE_PATH *first_new_option = NULL; // The path to the GRUB image we want to load.

/* entry function for EFI */
EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {
	EFI_STATUS err; // Define an error variable.
	
	InitializeLib(image_handle, systab); // Initialize EFI.
	console_text_mode(); // Put the console into text mode. If we don't do that, the image of the Apple
						 // boot manager will remain on the screen and the user won't see any output
						 // from the program.
	
	err = uefi_call_wrapper(BS->HandleProtocol, 3, image_handle, &LoadedImageProtocol, (void *)&this_image);
	if (EFI_ERROR(err)) {
		Print(L"Error: could not find loaded image: %d\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return err;
	}
	
	root_dir = LibOpenRoot(this_image->DeviceHandle);
    if (!root_dir) {
		Print(L"Unable to open root directory: %r ", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return EFI_LOAD_ERROR;
    }
	
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Set the text color.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	Print(banner, VERSION_MAJOR, VERSION_MINOR); // Print the welcome information.
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable display of the cursor.
	
	// Check to make sure that we have our configuration file and GRUB bootloader.
	if (!file_exists(root_dir, L"\\efi\\boot\\.MLUL-Live-USB")) {
		 display_error_text(L"Error: can't find configuration file.\n");
	}
	
	if (!file_exists(root_dir, L"\\efi\\boot\\boot.efi")) {
		 display_error_text(L"Error: can't find GRUB bootloader!.\n");
	}
	// Display the menu where the user can select what they want to do.
	display_menu();
	
	return EFI_SUCCESS;
}

EFI_STATUS boot_Linux_with_options(CHAR16 *params) {
	if (file_exists(root_dir, L"\\efi\\boot\\boot.iso")) {
		 display_error_text(L"Error: can't find ISO file to boot!\n");
		 display_error_text(L"It should be located under /efi/boot/ on this device.\n");
		 Print(L"Press any key to reboot.\n");
		 key_read(NULL, TRUE); // We don't care about the key, but we do need to wait.
		 uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
		 
		 return EFI_LOAD_ERROR;
	}
	
	return EFI_SUCCESS;
}

/* Try to open a file. If we can (i.e it exists) return TRUE. Otherwise, return FALSE. */
BOOLEAN file_exists(EFI_FILE_HANDLE dir, CHAR16 *name) {
	EFI_FILE_HANDLE handle;
	EFI_STATUS err;

	err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, 0ULL);
	if (EFI_ERROR(err)) {
		goto out;
	}

	uefi_call_wrapper(handle->Close, 1, handle);
	return TRUE;
out:
	return FALSE;
}


static EFI_STATUS console_text_mode(VOID) {
	#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID \
		{ 0xf42f7782, 0x12e, 0x4c12, { 0x99, 0x56, 0x49, 0xf9, 0x43, 0x4, 0xf7, 0x21 } };

	struct _EFI_CONSOLE_CONTROL_PROTOCOL;

	typedef enum {
		EfiConsoleControlScreenText,
		EfiConsoleControlScreenGraphics,
		EfiConsoleControlScreenMaxValue,
	} EFI_CONSOLE_CONTROL_SCREEN_MODE;

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		EFI_CONSOLE_CONTROL_SCREEN_MODE *Mode,
		BOOLEAN *UgaExists,
		BOOLEAN *StdInLocked
	);

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		EFI_CONSOLE_CONTROL_SCREEN_MODE Mode
	);

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		CHAR16 *Password
	);

	typedef struct _EFI_CONSOLE_CONTROL_PROTOCOL {
		EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE GetMode;
		EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE SetMode;
		EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN LockStdIn;
	} EFI_CONSOLE_CONTROL_PROTOCOL;

	EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	EFI_STATUS err;

	err = LibLocateProtocol(&ConsoleControlProtocolGuid, (VOID **)&ConsoleControl);
	if (EFI_ERROR(err))
		return err;
	return uefi_call_wrapper(ConsoleControl->SetMode, 2, ConsoleControl, EfiConsoleControlScreenText);
}
