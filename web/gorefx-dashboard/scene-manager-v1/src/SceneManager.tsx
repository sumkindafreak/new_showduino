import React, { useMemo, useState } from "react";
import { compileCueToMegaCommand, compileScene, compileSceneForDeployment, exportScene, importScene } from "./sceneCompiler";
import { starterScene } from "./starterScene";
import { CueType, PixelEffect, SceneCue, ShowduinoScene } from "./sceneTypes";
import "./sceneManager.css";

const cueTypes: CueType[] = ["audio", "pixel", "wait", "note", "relay_future", "dmx_future"];
const pixelEffects: PixelEffect[] = ["OFF", "SOLID", "PULSE", "FIRE", "STROBE", "LIGHTNING", "PORTAL_GLOW", "BLACKOUT"];

function makeId(prefix: string): string {
  return `${prefix}_${Date.now()}_${Math.floor(Math.random() * 9999)}`;
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, Math.round(Number.isFinite(value) ? value : min)));
}

function formatTime(ms: number): string {
  const seconds = Math.floor(ms / 1000);
  const millis = String(ms % 1000).padStart(3, "0");
  return `${seconds}.${millis}s`;
}

function cueColour(cue: SceneCue): string {
  if (cue.type === "audio") return "#38bdf8";
  if (cue.type === "pixel") return "#f97316";
  if (cue.type === "note") return "#a855f7";
  if (cue.type === "wait") return "#64748b";
  if (cue.type === "relay_future") return "#22c55e";
  if (cue.type === "dmx_future") return "#eab308";
  return "#94a3b8";
}

function createCue(type: CueType, trackId: string, startMs: number): SceneCue {
  const base = {
    id: makeId("cue"),
    name: `New ${type} cue`,
    type,
    trackId,
    startMs,
    durationMs: 1000,
    enabled: true,
    notes: "",
  } as const;

  if (type === "audio") {
    return { ...base, type: "audio", file: "001", volume: 80, action: "PLAY" };
  }

  if (type === "pixel") {
    return {
      ...base,
      type: "pixel",
      line: 1,
      effect: "PULSE",
      colour: { r: 255, g: 0, b: 0 },
      brightness: 160,
      speed: 40,
    };
  }

  if (type === "relay_future") {
    return { ...base, type: "relay_future", relay: 1, action: "PULSE" };
  }

  if (type === "dmx_future") {
    return { ...base, type: "dmx_future", channel: 1, value: 255 };
  }

  if (type === "wait") {
    return { ...base, type: "wait", name: "Wait" };
  }

  return { ...base, type: "note", name: "Note" };
}

function updateCue(scene: ShowduinoScene, cue: SceneCue): ShowduinoScene {
  return {
    ...scene,
    updatedAt: new Date().toISOString(),
    cues: scene.cues.map((item) => (item.id === cue.id ? cue : item)),
  };
}

export default function SceneManager() {
  const [scene, setScene] = useState<ShowduinoScene>(starterScene);
  const [selectedCueId, setSelectedCueId] = useState<string>(starterScene.cues[0]?.id ?? "");
  const [zoom, setZoom] = useState<number>(1);
  const [commandLog, setCommandLog] = useState<string[]>(["Scene Manager ready."]);
  const [importText, setImportText] = useState<string>("");

  const selectedCue = scene.cues.find((cue) => cue.id === selectedCueId) ?? null;
  const compiled = useMemo(() => compileScene(scene), [scene]);
  const deployment = useMemo(() => compileSceneForDeployment(scene), [scene]);

  const timelineWidth = Math.max(900, Math.round((scene.durationMs / 1000) * 120 * zoom));

  function log(message: string) {
    setCommandLog((old) => [message, ...old].slice(0, 16));
  }

  function setMeta<K extends keyof ShowduinoScene>(key: K, value: ShowduinoScene[K]) {
    setScene((old) => ({ ...old, [key]: value, updatedAt: new Date().toISOString() }));
  }

  function addCue(type: CueType) {
    const defaultTrack = scene.tracks.find((track) => track.type === type || track.type === "mixed") ?? scene.tracks[0];
    const cue = createCue(type, defaultTrack.id, 0);
    setScene((old) => ({ ...old, cues: [...old.cues, cue], updatedAt: new Date().toISOString() }));
    setSelectedCueId(cue.id);
    log(`Added ${type} cue.`);
  }

  function duplicateCue() {
    if (!selectedCue) return;
    const duplicate = {
      ...selectedCue,
      id: makeId("cue"),
      name: `${selectedCue.name} Copy`,
      startMs: selectedCue.startMs + 500,
    } as SceneCue;
    setScene((old) => ({ ...old, cues: [...old.cues, duplicate], updatedAt: new Date().toISOString() }));
    setSelectedCueId(duplicate.id);
    log(`Duplicated ${selectedCue.name}.`);
  }

  function deleteCue() {
    if (!selectedCue) return;
    setScene((old) => ({ ...old, cues: old.cues.filter((cue) => cue.id !== selectedCue.id), updatedAt: new Date().toISOString() }));
    setSelectedCueId("");
    log(`Deleted ${selectedCue.name}.`);
  }

  function patchSelectedCue(patch: Partial<SceneCue>) {
    if (!selectedCue) return;
    setScene((old) => updateCue(old, { ...selectedCue, ...patch } as SceneCue));
  }

  function previewSelectedCue() {
    if (!selectedCue) return;
    const command = compileCueToMegaCommand(selectedCue);
    if (!command) {
      log("Selected cue does not compile to a Mega command.");
      return;
    }
    log(`PREVIEW -> ${command}`);
  }

  function deployScene() {
    deployment.forEach((line) => log(`DEPLOY -> ${line}`));
  }

  function exportCurrentScene() {
    const text = exportScene(scene);
    navigator.clipboard?.writeText(text).catch(() => undefined);
    setImportText(text);
    log("Scene exported to text box and clipboard where supported.");
  }

  function importFromText() {
    try {
      const next = importScene(importText);
      setScene(next);
      setSelectedCueId(next.cues[0]?.id ?? "");
      log(`Imported scene: ${next.name}`);
    } catch (error) {
      log(error instanceof Error ? error.message : "Import failed.");
    }
  }

  return (
    <div className="gorefx-app">
      <header className="topbar">
        <div>
          <p className="eyebrow">Showduino v1</p>
          <h1>GoreFX Scene Manager</h1>
        </div>
        <div className="top-actions">
          <button onClick={previewSelectedCue}>Live Preview Cue</button>
          <button onClick={deployScene}>Deploy Scene</button>
          <button className="danger" onClick={() => log("SEND -> EMERGENCY:STOP")}>Emergency Stop</button>
        </div>
      </header>

      <main className="workspace">
        <section className="panel scene-panel">
          <h2>Scene</h2>
          <label>
            Name
            <input value={scene.name} onChange={(event) => setMeta("name", event.target.value)} />
          </label>
          <label>
            Description
            <textarea value={scene.description} onChange={(event) => setMeta("description", event.target.value)} />
          </label>
          <label>
            Duration ms
            <input type="number" value={scene.durationMs} onChange={(event) => setMeta("durationMs", clamp(Number(event.target.value), 1000, 600000))} />
          </label>
          <label>
            Zoom
            <input type="range" min="0.5" max="2.5" step="0.1" value={zoom} onChange={(event) => setZoom(Number(event.target.value))} />
          </label>
          <div className="cue-buttons">
            {cueTypes.map((type) => (
              <button key={type} onClick={() => addCue(type)}>{type.replace("_future", "")}</button>
            ))}
          </div>
        </section>

        <section className="panel timeline-panel">
          <div className="panel-title-row">
            <h2>Timeline</h2>
            <span>{formatTime(scene.durationMs)}</span>
          </div>
          <div className="timeline-scroll">
            <div className="timeline" style={{ width: timelineWidth }}>
              <div className="ruler">
                {Array.from({ length: Math.ceil(scene.durationMs / 1000) + 1 }).map((_, second) => (
                  <div key={second} className="tick" style={{ left: `${(second * 1000 / scene.durationMs) * 100}%` }}>{second}s</div>
                ))}
              </div>
              {scene.tracks.map((track) => (
                <div key={track.id} className="track-row">
                  <div className="track-label" style={{ borderColor: track.colour }}>{track.name}</div>
                  <div className="track-lane">
                    {scene.cues.filter((cue) => cue.trackId === track.id).map((cue) => (
                      <button
                        key={cue.id}
                        className={`cue-block ${cue.id === selectedCueId ? "selected" : ""}`}
                        style={{
                          left: `${(cue.startMs / scene.durationMs) * 100}%`,
                          width: `${Math.max(2, (cue.durationMs / scene.durationMs) * 100)}%`,
                          background: cueColour(cue),
                        }}
                        onClick={() => setSelectedCueId(cue.id)}
                        title={`${cue.name} @ ${formatTime(cue.startMs)}`}
                      >
                        {cue.name}
                      </button>
                    ))}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </section>

        <section className="panel inspector-panel">
          <div className="panel-title-row">
            <h2>Cue Inspector</h2>
            <div>
              <button onClick={duplicateCue} disabled={!selectedCue}>Duplicate</button>
              <button onClick={deleteCue} disabled={!selectedCue}>Delete</button>
            </div>
          </div>

          {!selectedCue && <p>Select a cue to edit it.</p>}

          {selectedCue && (
            <div className="inspector-grid">
              <label>Name<input value={selectedCue.name} onChange={(event) => patchSelectedCue({ name: event.target.value })} /></label>
              <label>Track
                <select value={selectedCue.trackId} onChange={(event) => patchSelectedCue({ trackId: event.target.value })}>
                  {scene.tracks.map((track) => <option key={track.id} value={track.id}>{track.name}</option>)}
                </select>
              </label>
              <label>Start ms<input type="number" value={selectedCue.startMs} onChange={(event) => patchSelectedCue({ startMs: clamp(Number(event.target.value), 0, scene.durationMs) })} /></label>
              <label>Duration ms<input type="number" value={selectedCue.durationMs} onChange={(event) => patchSelectedCue({ durationMs: clamp(Number(event.target.value), 0, scene.durationMs) })} /></label>
              <label className="checkbox"><input type="checkbox" checked={selectedCue.enabled} onChange={(event) => patchSelectedCue({ enabled: event.target.checked })} /> Enabled</label>

              {selectedCue.type === "audio" && (
                <>
                  <label>Action
                    <select value={selectedCue.action} onChange={(event) => patchSelectedCue({ action: event.target.value as never })}>
                      <option value="PLAY">PLAY</option>
                      <option value="STOP">STOP</option>
                      <option value="VOLUME">VOLUME</option>
                    </select>
                  </label>
                  <label>File / Track<input value={selectedCue.file} onChange={(event) => patchSelectedCue({ file: event.target.value } as never)} /></label>
                  <label>Volume<input type="number" value={selectedCue.volume} onChange={(event) => patchSelectedCue({ volume: clamp(Number(event.target.value), 0, 100) } as never)} /></label>
                </>
              )}

              {selectedCue.type === "pixel" && (
                <>
                  <label>Line
                    <select value={selectedCue.line} onChange={(event) => patchSelectedCue({ line: event.target.value === "ALL" ? "ALL" : Number(event.target.value) } as never)}>
                      <option value="1">1</option><option value="2">2</option><option value="3">3</option><option value="4">4</option><option value="ALL">ALL</option>
                    </select>
                  </label>
                  <label>Effect
                    <select value={selectedCue.effect} onChange={(event) => patchSelectedCue({ effect: event.target.value as PixelEffect } as never)}>
                      {pixelEffects.map((effect) => <option key={effect} value={effect}>{effect}</option>)}
                    </select>
                  </label>
                  <label>Red<input type="number" value={selectedCue.colour.r} onChange={(event) => patchSelectedCue({ colour: { ...selectedCue.colour, r: clamp(Number(event.target.value), 0, 255) } } as never)} /></label>
                  <label>Green<input type="number" value={selectedCue.colour.g} onChange={(event) => patchSelectedCue({ colour: { ...selectedCue.colour, g: clamp(Number(event.target.value), 0, 255) } } as never)} /></label>
                  <label>Blue<input type="number" value={selectedCue.colour.b} onChange={(event) => patchSelectedCue({ colour: { ...selectedCue.colour, b: clamp(Number(event.target.value), 0, 255) } } as never)} /></label>
                  <label>Brightness<input type="number" value={selectedCue.brightness} onChange={(event) => patchSelectedCue({ brightness: clamp(Number(event.target.value), 0, 255) } as never)} /></label>
                  <label>Speed<input type="number" value={selectedCue.speed} onChange={(event) => patchSelectedCue({ speed: clamp(Number(event.target.value), 1, 255) } as never)} /></label>
                </>
              )}

              <label className="wide">Notes<textarea value={selectedCue.notes} onChange={(event) => patchSelectedCue({ notes: event.target.value })} /></label>
              <div className="wide compiled-box">Mega command: {compileCueToMegaCommand(selectedCue) ?? "No command"}</div>
            </div>
          )}
        </section>

        <section className="panel command-panel">
          <h2>Compiled Scene</h2>
          <pre>{compiled.map((item) => `${item.timeMs}ms  ${item.command}`).join("\n")}</pre>
        </section>

        <section className="panel import-panel">
          <div className="panel-title-row">
            <h2>Import / Export .shdo</h2>
            <div>
              <button onClick={exportCurrentScene}>Export</button>
              <button onClick={importFromText}>Import</button>
            </div>
          </div>
          <textarea value={importText} onChange={(event) => setImportText(event.target.value)} placeholder="Exported .shdo JSON appears here." />
        </section>

        <section className="panel log-panel">
          <h2>Command Log</h2>
          <pre>{commandLog.join("\n")}</pre>
        </section>
      </main>
    </div>
  );
}
