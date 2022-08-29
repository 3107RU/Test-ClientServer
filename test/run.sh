#!/bin/bash -xe
cd "${BASH_SOURCE%/*}" || exit
gnome-terminal -- sh -c "bash -c \"./server; exec bash\""
gnome-terminal -- sh -c "bash -c \"./client localhost; exec bash\""
