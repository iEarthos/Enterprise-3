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

#include "main.h"
#define banner L"Welcome to Enterprise! - Version %d.%d\n"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

#define DEFAULT_LOADER L"\\grub.efi"
#define FALLBACK L"\\fallback.efi"
#define MOK_MANAGER L"\\MokManager.efi"

typedef struct {
    CHAR16 *file;
    CHAR16 *title_show;
    CHAR16 *title;
    CHAR16 *version;
    CHAR16 *machine_id;
    EFI_HANDLE *device;
//    enum loader_type type;
    CHAR16 *loader;
    CHAR16 *options;
    EFI_STATUS (*call)(void);
    BOOLEAN no_autoselect;
    BOOLEAN non_unique;
} ConfigEntry;

typedef struct {
    ConfigEntry **entries;
    UINTN entry_count;
    INTN idx_default;
    INTN idx_default_efivar;
    UINTN timeout_sec;
    UINTN timeout_sec_config;
    INTN timeout_sec_efivar;
    CHAR16 *entry_default_pattern;
    CHAR16 *entry_oneshot;
    CHAR16 *options_edit;
    CHAR16 *entries_auto;
} Config;

static CHAR16 *second_stage;
static void *load_options;
static UINT32 load_options_size;

static EFI_STATUS console_text_mode(VOID);
static UINTN file_read(EFI_FILE_HANDLE dir, const CHAR16 *name, CHAR8 **content);
static UINTN file_exists(EFI_FILE_HANDLE dir, const CHAR16 *name);

EFI_STATUS set_second_stage(EFI_HANDLE image_handle);
EFI_STATUS init_grub(EFI_HANDLE image_handle);

UINTN x_max = 80;
UINTN y_max = 25;

/* entry function for EFI */
EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {
    EFI_STATUS err; // Define an error variable.
    EFI_FILE *root_dir;
    EFI_LOADED_IMAGE *loaded_image;
    //CHAR8 *content = NULL;
    
    InitializeLib(image_handle, systab); // Initialize EFI.
    set_second_stage(image_handle); // Setup the second stage loader.
    
    // Set the colors for the display and clear the display.
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    // Export the device path this image is started from.
    err = uefi_call_wrapper(BS->OpenProtocol, 6, image_handle, &LoadedImageProtocol, &loaded_image,
                            image_handle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(err)) {
        Print(L"Error getting a LoadedImageProtocol handle: %r ", err);
        uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
        return EFI_LOAD_ERROR;
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
        
        return EFI_LOAD_ERROR;
    }
    
    console_text_mode(); // Set the console to text mode.
    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable the cursor.
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Text color.
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    Print(banner, VERSION_MAJOR, VERSION_MINOR);
    Print(L"Searching for distribution ISO file...");
    
    /*
     * Here we check if the ISO file we're going to boot (boot.iso) is present. If not, we
     * bail, as there is no reason to continue.
     */
    if (!file_exists(root_dir, L"\\boot.iso")) { // ISO file doesn't exist.
        Print(L"\nNo ISO file found with name boot.iso. Aborting!\n");
        uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE); // Enable the cursor.
        uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
        
        BS->Exit(image_handle, err, sizeof err, NULL); // Quit!
        
        return EFI_SUCCESS; // Should never get here.
    } else {
        Print(L" done!\n");
        
        err = EFI_SUCCESS; // Reset error status code.
        
        ConfigEntry* entry;
        entry->device = root_dir;
        entry->loader = L"hanoi.efi"; // Name of program to run?
        
        Config* config;
        //config->entries = entry;
        
        // Start boot procedure.
        Print(L"Starting boot process now...\n");
        err = load_image(image_handle, config, entry) != EFI_SUCCESS; // Test by trying to run a sample EFI program I have.
        if (err != EFI_SUCCESS) {
            Print(L"Couldn't load boot loader! Aborting!\n");
            
            uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE); // Enable the cursor.
            uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
            
            BS->Exit(image_handle, err, sizeof err, NULL); // Quit!
            
            return err; // Should never get here.
        }
    }
    
    return EFI_SUCCESS;
}

/*
* Check the load options to specify the second stage loader
*/
EFI_STATUS set_second_stage (EFI_HANDLE image_handle)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE *li;
    CHAR16 *start = NULL, *c;
    int i, remaining_size = 0;
    CHAR16 *loader_str = NULL;
    int loader_len = 0;
    
    second_stage = DEFAULT_LOADER;
    load_options = NULL;
    load_options_size = 0;
    
    status = uefi_call_wrapper(BS->HandleProtocol, 3, image_handle, &LoadedImageProtocol, (void **) &li);
    if (status != EFI_SUCCESS) {
        Print (L"Failed to get load options\n");
        return status;
    }

    /* Expect a CHAR16 string with at least one CHAR16 */
    if (li->LoadOptionsSize < 4 || li->LoadOptionsSize % 2 != 0) {
        return EFI_BAD_BUFFER_SIZE;
    }
    c = (CHAR16 *)(li->LoadOptions + (li->LoadOptionsSize - 2));
    if (*c != L'\0') {
        return EFI_BAD_BUFFER_SIZE;
    }

    /*UEFI shell copies the whole line of the command into LoadOptions.
    * We ignore the string before the first L' ', i.e. the name of this
    * program.
    */
    for (i = 0; i < li->LoadOptionsSize; i += 2) {
        c = (CHAR16 *)(li->LoadOptions + i);
        if (*c == L' ') {
            *c = L'\0';
            start = c + 1;
            remaining_size = li->LoadOptionsSize - i - 2;
            break;
        }
    }

    if (!start || remaining_size <= 0)
        return EFI_SUCCESS;

    for (i = 0; start[i] != '\0'; i++) {
        if (start[i] == L' ' || start[i] == L'\0')
            break;
        loader_len++;
    }

    /*
    * Setup the name of the alternative loader and the LoadOptions for
    * the loader
    */
    if (loader_len > 0) {
        loader_str = AllocatePool((loader_len + 1) * sizeof(CHAR16));
        if (!loader_str) {
            Print(L"Failed to allocate loader string\n");
            return EFI_OUT_OF_RESOURCES;
        }
        for (i = 0; i < loader_len; i++)
            loader_str[i] = start[i];
        
        loader_str[loader_len] = L'\0';
        second_stage = loader_str;
        load_options = start;
        load_options_size = remaining_size;
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
    if (EFI_ERROR(err)) {
        return err;
    }
    
    return uefi_call_wrapper(ConsoleControl->SetMode, 2, ConsoleControl, EfiConsoleControlScreenText);
}

/* See if we can open a file. If we can, than it exists. 
 * @dir = directory
 * @name = filename
 */
static UINTN file_exists(EFI_FILE_HANDLE dir, const CHAR16 *name) {
    EFI_FILE_HANDLE handle;
    EFI_FILE_INFO *info;
    EFI_STATUS err;
    
    err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, 0ULL);
    if (EFI_ERROR(err)) { // If we can't open the file, i.e it doesn't exist.
        goto out;
    }
    
    FreePool(info);
    uefi_call_wrapper(handle->Close, 1, handle);
    return TRUE;
out:
    return FALSE;
}

/* Open a file and copy its contents into @content. */
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
    if (EFI_ERROR(err) == EFI_SUCCESS) { // If read completed successfully..
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

EFI_STATUS init_grub(EFI_HANDLE image_handle) {
    EFI_STATUS efi_status;

    
    efi_status = start_image(image_handle, second_stage);

    if (efi_status != EFI_SUCCESS) {
        Print(L"Error: Failed to start second stage bootloader.");
    }

    return efi_status;
}
