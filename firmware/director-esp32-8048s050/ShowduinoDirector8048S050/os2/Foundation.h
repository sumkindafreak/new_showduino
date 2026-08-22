#pragma once

/**
 * Showduino OS -- CONSTITUTION
 *
 * This file is no longer a living design draft.
 * It is the platform constitution: the rules do not change.
 *
 * Status: OS 2.0 Complete
 * Phase: Application Development
 * Architecture: Frozen
 *
 * Read this before writing code.
 * If a feature requires a new architectural rule, redesign the feature.
 *
 * Identity: a platform for live entertainment control.
 * Director = one client. Stage Runtime = one execution engine.
 *
 * =============================================================================
 * PLATFORM vs STAGE RUNTIME
 * =============================================================================
 *
 * Platform (os2/) owns:
 *   Shell, Apps, Services, Commands, Events, Theme, Session,
 *   Asset model, Production model, Compatibility contracts
 *
 * Stage Runtime owns:
 *   Cue execution, Audio, DMX, GPIO, Effects, Timing, Safety execution
 *
 *   Operator -> Showduino OS -> Stage Runtime API -> Execution Engine
 *
 * =============================================================================
 * STACK
 * =============================================================================
 *
 *   Shell -> Apps -> Event Bus -> Commands -> Services -> Communication -> Stage Runtime
 */

#include "Compatibility.h"

#define SHOWDUINO_OS2_VERSION     "2.0.0"
#define SHOWDUINO_OS2_MILESTONE   "OS 2.0 Complete"
#define SHOWDUINO_OS2_PHASE       "Application Development"
#define SHOWDUINO_OS2_STATUS      "complete"
#define SHOWDUINO_OS2_CODENAME    "Operator Workspace"

/*
 * =============================================================================
 * THE TEN LAWS -- Showduino OS Constitution
 * Every future contributor must read these before writing code.
 * =============================================================================
 *
 * LAW 1 -- TRUTH EXISTS ONCE
 *   Every runtime fact has exactly one owner.
 *   If two services can answer the same question, the architecture is wrong.
 *
 * LAW 2 -- INTENT IS NEVER TRUTH
 *   Commands request. Services report. Events announce.
 *   Those three concepts never merge.
 *
 * LAW 3 -- APPS NEVER OWN STATE
 *   Apps are disposable. Closing an app changes nothing.
 *   Opening an app restores everything from the platform.
 *
 * LAW 4 -- SERVICES NEVER RENDER
 *   Services have no knowledge of LVGL, widgets, colours, or layouts.
 *
 * LAW 5 -- SHELL NEVER KNOWS DOMAIN LOGIC
 *   The shell knows apps. Nothing else.
 *   It never asks "Is the show running?"
 *   It asks the active app what to display.
 *
 * LAW 6 -- STAGE RUNTIME IS REMOTE
 *   Even when running on the same hardware.
 *   The platform always behaves as if the runtime lives somewhere else.
 *
 * LAW 7 -- HARDWARE IS AN ADAPTER
 *   Every board implements a platform interface.
 *   Replacing hardware never changes the OS contracts.
 *
 * LAW 8 -- EVENTS DESCRIBE THE PAST
 *   An event means something has already happened.
 *   Not StartShow (Command). Instead: ShowStarted (Event).
 *   Commands describe the future. Events describe the past.
 *
 * LAW 9 -- EVERY FEATURE FITS THE PLATFORM
 *   If a feature requires a new architectural rule, the feature is probably wrong.
 *   Not the platform.
 *
 * LAW 10 -- THE OPERATOR COMES FIRST
 *   Every decision serves: clearer, faster, or safer live operation.
 *   If not, it does not belong in Showduino OS.
 *
 * =============================================================================
 * COMPATIBILITY
 *   Public interfaces are versioned APIs (Compatibility.h).
 *   Breaking change -> v2. Never silent semantic drift.
 *
 * VOCABULARY (frozen)
 *   Production, Library, Stage Runtime, Director, Service, Command,
 *   Event, Workspace, App, Shell
 *
 * ADRs: docs/adr/
 * ROADMAP: docs/showduino-os-roadmap.md
 * PRODUCT VISION: docs/showduino-product-vision.md
 *   Phase A Core Apps -> B Workflow -> C Ecosystem -> D Clients -> E Plugins
 *
 * Success metric: operator tasks (Law 10), not LOC or screen count.
 *
 * From OS 2.0 Complete onward, changes primarily add:
 *   Apps, domain Services, Commands, Production capabilities
 * They do not redefine the platform.
 */