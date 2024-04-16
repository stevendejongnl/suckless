#!/bin/bash
#
# handle_playerctl() {
#     if [ "$(playerctl status)" = "Playing" ]; then
#         metadata=$(playerctl metadata --format "Now playing: {{ artist }} - {{ title }}")
#
#         if [ -n "$metadata" ]; then
#             echo $metadata
#         fi
#     fi
# }
#
# handle_spt() {
#     echo "$(spt playback)"
# }
#
# update() {
#     if command -v playerctl &> /dev/null; then
#         handle_playerctl
#     elif command -v spt &> /dev/null; then
#         handle_spt
#     else
#         echo "Neither playerctl nor spt found. Please install either playerctl or spt (spotify-tui)."
#         exit 1
#     fi
# }
#
# while true; do
#     sleep 15
#
#     update
# done

echo "Flapper de flap"
