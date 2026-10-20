#!/bin/bash
# PortMaster launcher for the native Joust (SDL2).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/joust

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

cd $GAMEDIR

# settings and scores live with the port; the player's own ROM set sits beside it
mkdir -p "$GAMEDIR/conf"
export JOUST_SAVE_DIR="$GAMEDIR/conf"
export JOUST_ASSETS="$GAMEDIR/assets"
for rom in "$GAMEDIR/joust.zip" "$GAMEDIR/joust"; do
  [ -e "$rom" ] && export JOUST_ROM="$rom" && break
done

# gptokeyb only provides the exit hotkey; the game reads the pad itself
$GPTOKEYB "joust.${DEVICE_ARCH}" &

SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig" ./joust.${DEVICE_ARCH}

pm_finish
