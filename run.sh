#!/bin/bash -xe
cd "${BASH_SOURCE%/*}" || exit
gnome-terminal -- sh -c "bash -c \"./build/server/server; exec bash\""
gnome-terminal -- sh -c "bash -c \"./build/client/client localhost; exec bash\""
