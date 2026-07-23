#ifndef SHOWDUINO_OS_MODULES_H
#define SHOWDUINO_OS_MODULES_H

/*
 * Showduino OS — logical module map (Stage 6 foundation).
 * Do not treat this as a forced folder refactor; it documents ownership.
 *
 * Runtime        — ShowRuntime + state machine (protocol/showduino_show_runtime.h)
 *                  Stage: ShowRuntimeOwner.h (authority)
 *                  Director: gShowMirror (read-only)
 * Timeline       — TimelineEngine (Stage owns playback; Director parses SD for upload)
 * Storage        — Director SD packages (/showduino/shows/packages) unchanged
 * Communications — ESP-NOW desk packets + UART link (existing)
 * Emergency      — Stage safety routing + Director overlay UX
 * Logging        — existing logEvent / Serial
 * Nodes          — Relay via ROUTE:RELAY (existing)
 * UI             — Director LVGL desk (operator console only)
 */

#endif
