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
#include "utils.h"
#define banner L"Welcome to Enterprise! - Version %d.%d\n"

static const EFI_GUID enterprise_variable_guid = {0x4a67b082, 0x0a4c, 0x41cf, {0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f}};
static const EFI_GUID grub_variable_guid = {0x8BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B,0x8C}};

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

static VOID ReadConfigurationFile(const CHAR16 *name);
static CHAR8* KernelLocationForDistributionName(const CHAR8 *name);
static CHAR8* InitRDLocationForDistributionName(const CHAR8 *name);
static EFI_STATUS console_text_mode(VOID);

static EFI_LOADED_IMAGE *this_image = NULL;
static EFI_FILE *root_dir;

static EFI_HANDLE global_image;

EFI_DEVICE_PATH *first_new_option = NULL; // The path to the GRUB image we want to load.

/* entry function for EFI */
EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {
	EFI_STATUS err; // Define an error variable.
	
	InitializeLib(image_handle, systab); // Initialize EFI.
	console_text_mode(); // Put the console into text mode. If we don't do that, the image of the Apple
						 // boot manager will remain on the screen and the user won't see any output
						 // from the program.
	global_image = image_handle;
	
	err = uefi_call_wrapper(BS->HandleProtocol, 3, image_handle, &LoadedImageProtocol, (void *)&this_image);
	if (EFI_ERROR(err)) {
		Print(L"Error: could not find loaded image: %d\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return err;
	}
	
	root_dir = LibOpenRoot(this_image->DeviceHandle);
    if (!root_dir) {
		Print(L"Unable to open root directory.\n");
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return EFI_LOAD_ERROR;
    }
	
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Set the text color.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	Print(banner, VERSION_MAJOR, VERSION_MINOR); // Print the welcome information.
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable display of the cursor.
	
	BOOLEAN can_continue = TRUE;
	
	// Check to make sure that we have our configuration file and GRUB bootloader.
	if (!FileExists(root_dir, L"\\efi\\boot\\.MLUL-Live-USB")) {
		 DisplayErrorText(L"Error: can't find configuration file.\n");
	} else {
		ReadConfigurationFile(L"\\efi\\boot\\.MLUL-Live-USB");
	}
	
	if (!FileExists(root_dir, L"\\efi\\boot\\boot.efi")) {
		 DisplayErrorText(L"Error: can't find GRUB bootloader!.\n");
		 can_continue = FALSE;
	}
	
	if (!FileExists(root_dir, L"\\efi\\boot\\boot.iso")) {
		 DisplayErrorText(L"Error: can't find ISO file to boot!.\n");
		 can_continue = FALSE;
	}
	
	// Display the menu where the user can select what they want to do.
	if (can_continue) {
		DisplayMenu();
	} else {
		Print(L"Cannot continue because core files are missing. Restarting...\n");
		uefi_call_wrapper(BS->Stall, 1, 1000 * 1000);
	}
	
	return EFI_SUCCESS;
}

EFI_STATUS BootLinuxWithOptions(CHAR16 *params) {
	EFI_STATUS err;
	EFI_HANDLE image;
	EFI_DEVICE_PATH *path;
	
	//CHAR8 *sized_str = (CHAR8 *)L"nomodeset";
	CHAR8 *sized_str = UTF16toASCII(params, StrLen(params) + 1);
	efi_set_variable(&grub_variable_guid, L"Enterprise_LinuxBootOptions", sized_str, sizeof(sized_str) * strlena(sized_str) + 1, FALSE);
	
	// Load the EFI boot loader image into memory.
	path = FileDevicePath(this_image->DeviceHandle, L"\\efi\\boot\\boot.efi");
	err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, global_image, path, NULL, 0, &image);
	if (EFI_ERROR(err)) {
		DisplayErrorText(L"Error loading image: ");
		Print(L"%r\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		FreePool(path);
		
		return EFI_LOAD_ERROR;
	}
	
	// Start the EFI boot loader.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		DisplayErrorText(L"Error starting image: ");
		Print(L"%r\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		FreePool(path);
		
		return EFI_LOAD_ERROR;
	}
	
	return EFI_SUCCESS;
}

static VOID ReadConfigurationFile(const CHAR16 *name) {
	LinuxBootOption *boot_options = AllocateZeroPool(sizeof(LinuxBootOption));
	
	CHAR8 *contents;
	UINTN read_bytes = FileRead(root_dir, name, &contents);
	if (read_bytes == 0) {
		DisplayErrorText(L"Error: Couldn't read configuration information.");
	}
	
	UINTN position = 0;
	CHAR8 *key, *value;
	while ((GetConfigurationKeyAndValue(contents, &position, &key, &value))) {
		if (strcmpa((CHAR8 *)"kernel", key) == 0) {
			boot_options->kernel_path = KernelLocationForDistributionName(value);
		} else if (strcmpa((CHAR8 *)"inird", key) == 0) {
			boot_options->initrd_path = InitRDLocationForDistributionName(value);
		}
	}
}

static CHAR8* KernelLocationForDistributionName(const CHAR8 *name) {
	return (CHAR8 *)"";
}

static CHAR8* InitRDLocationForDistributionName(const CHAR8 *name) {
	return (CHAR8 *)"";
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
