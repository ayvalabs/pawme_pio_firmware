# PlatformIO CLI Commands Cheat Sheet

## Project
pio project init
pio project init --board <board>
pio project metadata

## Build
pio run
pio run -e <env_name>
pio run -t clean
pio run --list-targets

## Upload
pio run -t upload
pio run -e <env_name> -t upload
pio run -t upload --upload-port <port>

## Serial Monitor
pio device monitor
pio device monitor -p <port> -b <baud>

## Common Workflows
pio run -e <env> -t upload && pio device monitor
pio run -t clean && pio run -e <env> -t upload

## Platforms / Packages / Libs
pio platform list
pio platform install <platform>
pio pkg update
pio lib search <keyword>
pio lib install <name_or_id>
pio lib list

## Devices / Env Info
pio device list
pio run -t envdump
