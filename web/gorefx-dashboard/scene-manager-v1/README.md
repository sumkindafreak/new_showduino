# GoreFX Scene Manager v1

This is the first complex Scene Manager / Scene Creator foundation for Showduino v1.

## Purpose

The Scene Manager runs on the CYD-hosted WebUI side of the system.

It is used to create scenes, edit timeline cues, preview commands, export `.shdo` scene files, and eventually deploy compiled scenes to the Mega Stage Engine.

## Current Features

- Scene metadata editor
- Multi-track timeline
- Cue list
- Cue inspector
- Audio cues
- Pixel cues
- Relay expansion cue type reserved for later
- DMX expansion cue type reserved for later
- Notes/wait cue types
- Command compiler for Mega serial commands
- Export scene JSON
- Import scene JSON
- Live preview command output
- Deploy command output

## Architecture

```text
CYD Director / WebUI
        |
        | creates and stores scenes
        v
Scene Manager
        |
        | compiles scene to serial commands
        v
CYD serial bridge
        |
        v
Mega Stage Engine
```

## Scene Storage

Scenes belong on the CYD SD card.

Recommended CYD SD layout:

```text
/scenes/
/shows/
/projects/
/assets/
/logs/
/settings.json
```

## Mega Runtime Storage

The Mega SD card stores runtime assets, mainly:

```text
/audio/
/pixels/
/runtime/
/logs/
```

## Next Integration Step

The current app can generate commands. The next step is wiring those generated commands into the CYD HTTP endpoint, for example:

```text
POST /api/command
POST /api/deploy-scene
GET /api/mega/status
```

## Local Development

This folder is intended to become a Vite React app. If a package file is not present yet, create one with Vite/React/TypeScript dependencies and run:

```text
npm install
npm run dev
```
