/*
 * Tool intended to help facilitate the process of booting Linux on Intel Macintosh computers made by Apple from a USB 
 * stick or similar.
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

static EFI_STATUS console_text_mode(VOID);
static UINTN file_read(EFI_FILE_HANDLE dir, const CHAR16 *name, CHAR8 **content);

UINTN x_max = 80;
UINTN y_max = 25;

EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS err; // Define an error variable.
    EFI_FILE *root_dir;
    EFI_LOADED_IMAGE *loaded_image;
    CHAR8 *content = NULL;
    
    InitializeLib(image_handle, systab); // Initialize EFI.
    
    // Set the colors for the display and clear the display.
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    // Export the device path this image is started from.
    err = uefi_call_wrapper(BS->OpenProtocol, 6, image_handle, &LoadedImageProtocol, &loaded_image,
                            image_handle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(err)) {
        Print(L"Error getting a LoadedImageProtocol handle: %r ", err);
        uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
        return err;
    }
    
    root_dir = LibOpenRoot(loaded_image->DeviceHandle);
    if (!root_dir) {
        Print(L"Unable to open root directory: %r ", err);
        uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
        return EFI_LOAD_ERROR;
    }
    
    // Query the display.
    err = uefi_call_wrapper(ST->ConOut->QueryMode, 4, ST->ConOut, ST->ConOut->Mode->Mode, &x_max, &y_max);
    
    if (EFI_ERROR(err)) {
        uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
        Print(L"Error querying for display mode.\n");
        x_max = 80;
        y_max = 25;
        
        return EFI_SUCCESS;
    }
    
    console_text_mode(); // Set the console to text mode.
    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable the cursor.
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Text color.
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    Print(L"Welcome to Enterprise!\n");
    Print(L"Searching for distribution ISO file...");
    
    /*
     * Here we check if the ISO file we're going to boot (boot.iso) is present. If not, we
     * bail, as there is no reason to continue.
     */
    UINTN len = file_read(root_dir, L"\\boot.iso", &content);
    if (len == 0) { // Either ISO has 0 length or the file doesn't exist (more probable).
        Print(L"\nNo ISO file found with name boot.iso. Aborting!\n");
        uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE); // Enable the cursor.
        uefi_call_wrapper(BS->Stall, 1, 3000);
        
        BS->Exit(image_handle, err, sizeof err, NULL); // Quit!
        
        return err;
    } else {
        Print(L" done!\n");
        
        // Start boot procedure.
    }
    
    return EFI_SUCCESS;
}

/* Stolen (er.. borrowed) from gummiboot */
static EFI_STATUS console_text_mode(VOID) {
#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID \
{ 0xf42f7782, 0x12e, 0x4c12, { 0x99, 0x56, 0x49, 0xf9, 0x43, 0x4, 0xf7, 0x21 }};
    
    struct _EFI_CONSOLE_CONTROL_PROTOCOL;
    
    typedef enum {
        EfiConsoleControlScreenText,
        EfiConsoleControlScreenGraphics,
        EfiConsoleControlScreenMaxValue,
    } EFI_CONSOLE_CONTROL_SCREEN_MODE;
    
    typedef EFI_STATUS (*EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE)(
                                                                struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
                                                                EFI_CONSOLE_CONTROL_SCREEN_MODE *Mode,
                                                                BOOLEAN *UgaExists,
                                                                BOOLEAN *StdInLocked
                                                                );
    
    typedef EFI_STATUS (*EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE)(
                                                                struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
                                                                EFI_CONSOLE_CONTROL_SCREEN_MODE Mode
                                                                );
    
    typedef EFI_STATUS (*EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN)(
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

static UINTN file_read(EFI_FILE_HANDLE dir, const CHAR16 *name, CHAR8 **content) {
    EFI_FILE_HANDLE handle;
    EFI_FILE_INFO *info;
    CHAR8 *buf;
    UINTN buflen;
    EFI_STATUS err;
    UINTN len = 0;
    
    err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, 0ULL);
    if (EFI_ERROR(err)) { // If we can't open the file, i.e it doesn't exist.
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
