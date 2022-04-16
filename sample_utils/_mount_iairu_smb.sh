#!/bin/bash

# cifs/smb network share mount
# invoked in _ssh_to_debianvm.sh (placed in /root)
# makes sure /root/remote directory exists or creates it
# mount only if not mounted
if ! grep -qs "/root/remote" /proc/mounts; then
	mkdir -p /root/remote
	mount -t cifs "//INSERT_PC_IP_ADDRESS/## zSchool/SPAASM/_dev_asm_z2" /root/remote -o username=INSERT_USERNAME
fi
