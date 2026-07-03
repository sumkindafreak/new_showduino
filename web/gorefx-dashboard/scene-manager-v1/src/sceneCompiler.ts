import { CompiledCommand, PixelCue, SceneCue, ShowduinoScene } from "./sceneTypes";

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, Math.round(value)));
}

function colourToCommand(cue: PixelCue): string {
  return `${clamp(cue.colour.r, 0, 255)},${clamp(cue.colour.g, 0, 255)},${clamp(cue.colour.b, 0, 255)}`;
}

export function compileCueToMegaCommand(cue: SceneCue): string | null {
  if (!cue.enabled) return null;

  if (cue.type === "audio") {
    if (cue.action === "PLAY") return `AUDIO:PLAY:${cue.file}`;
    if (cue.action === "STOP") return "AUDIO:STOP";
    if (cue.action === "VOLUME") return `AUDIO:VOLUME:${clamp(cue.volume, 0, 100)}`;
  }

  if (cue.type === "pixel") {
    if (cue.effect === "BLACKOUT" || cue.effect === "OFF") {
      return cue.line === "ALL" ? "PIXEL:ALL:BLACKOUT" : `PIXEL:${cue.line}:OFF`;
    }

    if (cue.effect === "SOLID") {
      return `PIXEL:${cue.line}:COLOR:${colourToCommand(cue)}`;
    }

    return `PIXEL:${cue.line}:EFFECT:${cue.effect}:COLOR:${colourToCommand(cue)}:BRIGHTNESS:${clamp(cue.brightness, 0, 255)}:SPEED:${clamp(cue.speed, 1, 255)}:DURATION:${clamp(cue.durationMs, 0, 600000)}`;
  }

  if (cue.type === "relay_future") {
    if (cue.action === "PULSE") return `RELAY:${cue.relay}:PULSE:${clamp(cue.durationMs, 1, 30000)}`;
    return `RELAY:${cue.relay}:${cue.action}`;
  }

  if (cue.type === "dmx_future") {
    return `DMX:${clamp(cue.channel, 1, 512)}:${clamp(cue.value, 0, 255)}`;
  }

  return null;
}

export function compileScene(scene: ShowduinoScene): CompiledCommand[] {
  return scene.cues
    .map((cue) => {
      const command = compileCueToMegaCommand(cue);
      if (!command) return null;
      return {
        cueId: cue.id,
        timeMs: clamp(cue.startMs, 0, scene.durationMs),
        command,
        label: cue.name,
      } as CompiledCommand;
    })
    .filter((item): item is CompiledCommand => Boolean(item))
    .sort((a, b) => a.timeMs - b.timeMs);
}

export function compileSceneForDeployment(scene: ShowduinoScene): string[] {
  const safeName = scene.name.replace(/[^a-zA-Z0-9_-]/g, "_");
  const commands = compileScene(scene);

  return [
    `SCENE:BEGIN:${safeName}:${scene.durationMs}`,
    ...commands.map((item) => `CUE:${item.timeMs}:${item.command}`),
    "SCENE:END",
  ];
}

export function exportScene(scene: ShowduinoScene): string {
  return JSON.stringify(
    {
      ...scene,
      updatedAt: new Date().toISOString(),
    },
    null,
    2,
  );
}

export function importScene(json: string): ShowduinoScene {
  const parsed = JSON.parse(json) as ShowduinoScene;

  if (parsed.format !== "showduino-scene") {
    throw new Error("This is not a Showduino scene file.");
  }

  if (!Array.isArray(parsed.tracks) || !Array.isArray(parsed.cues)) {
    throw new Error("Scene file is missing tracks or cues.");
  }

  return parsed;
}
