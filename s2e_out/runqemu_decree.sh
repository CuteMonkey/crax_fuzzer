#!/bin/bash

/home/chengte/SCraxF/build/qemu-release/i386-softmmu/qemu-system-i386 /home/chengte/disk_images/decree.raw -m 1024 \
-redir tcp:2222::22
