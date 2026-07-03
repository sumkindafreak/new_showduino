import React from "react";
import { createRoot } from "react-dom/client";
import SceneManager from "./SceneManager";

const rootElement = document.getElementById("root");

if (!rootElement) {
  throw new Error("Root element not found. Add a div with id='root' to the app shell.");
}

createRoot(rootElement).render(
  <React.StrictMode>
    <SceneManager />
  </React.StrictMode>,
);
