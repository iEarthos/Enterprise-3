#!/bin/sh
make -C src >> /dev/null # Hide regular build messages, we only want errors.
mkdir bin >> /dev/null 2> /dev/null # Make a new folder if we need to.
mv src/enterprise.efi bin/enterprise.efi
if make -C src clean >> /dev/null
then
	echo Done!
	return 0
else
	return 1
fi
