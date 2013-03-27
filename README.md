Enterprise
==========

This is a tool intended to help facilitate the process of booting Linux on Intel Macintosh computers made by Apple from a USB stick or similar. It is written in C, using gnu-efi, and is currently compiling for 32-bit (64-bit support is coming very soon!). It is _not_ stable, and should really _not_ be used, except for testing with VirtualBox or QEMU, of course. :)

This program is in an __extremely__ early stage. Here's a list of tasks that need to be done:

- [x] detects ISO file (named boot.iso)
- [ ] detects system configuration (firmware version, etc)
- [ ] integration with [Mac Linux USB Loader](https://github.com/SevenBits/Mac-Linux-USB-Loader)
- [ ] boots Linux from ISO
- [ ] boot option customization, troubleshooting, etc

Now, with that out the way (_phew!_), let's get to the interesting stuff, shall we?

### LICENSE ###

This thing is currently under the Lesser GPL, version 2. It may some day upgrade to the full GPL, but for now, the LGPL serves our purposes well. Why is it under the LGPL? Well, out of the goodness of my heart, of course! (And Enterprise steals -- read, borrows -- code from gummiboot, which is under the LGPL.

### CODING STYLE ###

I've decided to establish, up front, a coding style for Enterprise for anyone that wants to contribute. Most important: braces and if-statements. I'll sum up my policy in one code block:

    if (true)
        bob();
    else
        sam();

is _not_ complaint. This is how I prefer it:

    if (true) {
        bob();
    } else {
        sam();
    }

Same goes with functions, other blocks like `while`, `do`, and pretty much anything else. Please realize that not everything will be under this style (particularly code I've pulled in from elsewhere) and while, yes, that makes me a hypocrite, I don't care. I'll do my best to follow this style, and if you contribute, I expect you to do yours.

### PULL REQUESTS ###

I will accept pull requests on two conditions:

1. Your code follows my coding styles (mentioned above) and
2. Your code is licensed under an LGPL-compatible license and is readable and decently commented.

Assuming that your code is in compliance with these rules, I usually will accept pull requests that either: add a feature/fix a bug that is listed via the checkboxes above or fix an issue filed through GitHub.

### FINAL THOUGHTS ###

That's about it, I think. Hope this helps someone.