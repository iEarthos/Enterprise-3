ARCH     = $(shell uname -m | sed s,i[3456789]86,ia32,)
EFIROOT  = /usr/local
HDRROOT  = $(EFIROOT)/include/efi
INCLUDES = -I. -I$(HDRROOT) -I$(HDRROOT)/$(ARCH) -I$(HDRROOT)/protocol

CRTOBJS  = $(EFIROOT)/lib/crt0-efi-$(ARCH).o
CFLAGS   = -O2 -fpic -fshort-wchar -fno-strict-aliasing \
           -fno-merge-constants
CPPFLAGS = -DCONFIG_$(ARCH)
FORMAT   = efi-app-$(ARCH)
INSTALL  = install
LDFLAGS  = -nostdlib
LDSCRIPT = $(EFIROOT)/lib/elf_$(ARCH)_efi.lds
LDFLAGS += -T $(LDSCRIPT) -shared -Bsymbolic -L$(EFIROOT)/lib $(CRTOBJS)
LOADLIBS = -lefi -lgnuefi $(shell $(CC) -print-libgcc-file-name)

# Toolchain prefix
#
prefix      = 
CC          = $(prefix)gcc
AS          = $(prefix)as
LD          = $(prefix)ld
AR          = $(prefix)ar
RANLIB      = $(prefix)ranlib
OBJCOPY     = $(prefix)objcopy

# Rules
#
enterprise.efi: *.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
                   -j .rela -j .reloc --target=$(FORMAT) *.so $@

*.so: *.o
	$(LD) $(LDFLAGS) $^ -o $@ $(LOADLIBS)

*.o: *.c
	$(CC) $(INCLUDES) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Targets
#
TARGETS = enterprise.efi

all: $(TARGETS)

clean:
	rm -f $(TARGETS) *.o *.so
