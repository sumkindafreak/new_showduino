import React, { useMemo, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { Activity, AlertTriangle, AudioLines, CircuitBoard, Download, Flame, Gauge, Play, Radio, Save, Square, Zap } from 'lucide-react';
import './styles.css';
import { demoProject, makeCommand, serializeProject, ShowduinoCommand } from './models/showduino';

const commandButtons = [
  { label: 'Load Demo', type: 'SHOW_LOAD' as const, raw: 'SHOW:LOAD:ZombieBurst', icon: Save },
  { label: 'Start Show', type: 'SHOW_START' as const, raw: 'SHOW:START', icon: Play },
  { label: 'Stop Show', type: 'SHOW_STOP' as const, raw: 'SHOW:STOP', icon: Square },
  { label: 'Relay 1 ON', type: 'RELAY_SET' as const, raw: 'RELAY:1:ON', icon: Zap },
  { label: 'Relay 1 OFF', type: 'RELAY_SET' as const, raw: 'RELAY:1:OFF', icon: Zap },
  { label: 'Thunder Track', type: 'AUDIO_PLAY' as const, raw: 'AUDIO:1:PLAY:014', icon: AudioLines },
  { label: 'DMX Full', type: 'DMX_SET' as const, raw: 'DMX:10:255', icon: Gauge },
  { label: 'Hellfire', type: 'PIXEL_EFFECT' as const, raw: 'PIXEL:HELLFIRE', icon: Flame },
  { label: 'Status', type: 'STATUS_REQUEST' as const, raw: 'STATUS:REQUEST', icon: Activity },
  { label: 'Emergency Stop', type: 'EMERGENCY_STOP' as const, raw: 'EMERGENCY:STOP', icon: AlertTriangle }
];

function App() {
  const [commands, setCommands] = useState<ShowduinoCommand[]>([
    makeCommand('STATUS_REQUEST', 'Boot status check', 'STATUS:REQUEST')
  ]);

  const projectJson = useMemo(() => serializeProject(demoProject), []);

  const sendCommand = (button: (typeof commandButtons)[number]) => {
    const command = makeCommand(button.type, button.label, button.raw, 'mock');
    setCommands((current) => [command, ...current].slice(0, 14));
  };

  const downloadProject = () => {
    const blob = new Blob([projectJson], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = 'zombie-burst.showduino.json';
    anchor.click();
    URL.revokeObjectURL(url);
  };

  return (
    <main className="app-shell">
      <aside className="sidebar">
        <div className="brand-block">
          <CircuitBoard size={34} />
          <div>
            <p className="eyebrow">Showduino v1</p>
            <h1>GoreFX Studio</h1>
          </div>
        </div>

        <nav className="nav-list" aria-label="Dashboard sections">
          <a href="#dashboard">Dashboard</a>
          <a href="#live">Live Controls</a>
          <a href="#timeline">Timeline</a>
          <a href="#flow">Flow Designer</a>
          <a href="#library">Show Library</a>
          <a href="#commands">Command Log</a>
        </nav>
      </aside>

      <section className="workspace">
        <header className="hero-panel" id="dashboard">
          <div>
            <p className="eyebrow">Foundation build</p>
            <h2>WebUI first, CYD field panel second.</h2>
            <p>
              This starter dashboard proves the shared command language, show file model and
              live-control workflow before we add the heavy drag-and-drop editors.
            </p>
          </div>
          <button className="danger-button" onClick={() => sendCommand(commandButtons[9])}>
            <AlertTriangle size={18} /> Emergency Stop
          </button>
        </header>

        <section className="card-grid" id="live">
          <article className="panel wide">
            <div className="panel-title-row">
              <div>
                <p className="eyebrow">Live Controls</p>
                <h3>Shared Showduino commands</h3>
              </div>
              <Radio className="muted-icon" />
            </div>
            <div className="control-grid">
              {commandButtons.map((button) => {
                const Icon = button.icon;
                return (
                  <button key={button.raw} className="command-button" onClick={() => sendCommand(button)}>
                    <Icon size={18} />
                    <span>{button.label}</span>
                    <code>{button.raw}</code>
                  </button>
                );
              })}
            </div>
          </article>

          <article className="panel" id="library">
            <div className="panel-title-row">
              <div>
                <p className="eyebrow">Show Library</p>
                <h3>{demoProject.showName}</h3>
              </div>
              <button className="small-button" onClick={downloadProject}>
                <Download size={16} /> Export
              </button>
            </div>
            <p>{demoProject.description}</p>
            <div className="stats-row">
              <span>{demoProject.steps.length} steps</span>
              <span>{demoProject.version}</span>
            </div>
          </article>
        </section>

        <section className="panel" id="timeline">
          <div className="panel-title-row">
            <div>
              <p className="eyebrow">Timeline Mode</p>
              <h3>Zombie Burst demo sequence</h3>
            </div>
          </div>
          <div className="timeline-list">
            {demoProject.steps.map((step) => (
              <div className="timeline-row" key={step.id}>
                <span className="time-chip">{(step.timeMs / 1000).toFixed(1)}s</span>
                <strong>{step.name}</strong>
                <code>{step.command}</code>
                <p>{step.notes}</p>
              </div>
            ))}
          </div>
        </section>

        <section className="card-grid">
          <article className="panel" id="flow">
            <p className="eyebrow">Flow Designer</p>
            <h3>Next interactive layer</h3>
            <p>
              Flow blocks will sit on top of the same command model: wait for PIR, play audio,
              trigger relay, branch, loop and reset. This keeps puzzle props and scare scenes using
              one language.
            </p>
          </article>

          <article className="panel" id="commands">
            <p className="eyebrow">Command Log</p>
            <h3>Latest outgoing commands</h3>
            <div className="command-log">
              {commands.map((command) => (
                <div key={command.id}>
                  <span>{new Date(command.createdAt).toLocaleTimeString()}</span>
                  <strong>{command.label}</strong>
                  <code>{command.raw}</code>
                </div>
              ))}
            </div>
          </article>
        </section>
      </section>
    </main>
  );
}

createRoot(document.getElementById('root') as HTMLElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
