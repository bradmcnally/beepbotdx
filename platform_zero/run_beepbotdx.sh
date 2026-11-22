#!/bin/sh

state_dir=${XDG_STATE_HOME:-"$HOME/.local/state"}/beepbotdx
if mkdir -p "$state_dir"; then
    exec >"$state_dir/launch.log" 2>&1
fi
exec /usr/share/APPLaunch/apps/beepbotdx/beepbotdx "$@"
