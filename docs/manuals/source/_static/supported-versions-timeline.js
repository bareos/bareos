//   BAREOS - Backup Archiving REcovery Open Sourced
//
//   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
//
//   This program is Free Software; you can redistribute it and/or
//   modify it under the terms of version three of the GNU Affero General Public
//   License as published by the Free Software Foundation and included
//   in the file LICENSE.
//
//   This program is distributed in the hope that it will be useful, but
//   WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//   Affero General Public License for more details.
//
//   You should have received a copy of the GNU Affero General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
//   02110-1301, USA.

"use strict";

function parseIsoDate(value) {
  return new Date(`${value}T00:00:00Z`);
}

function parseTranslate(value) {
  const match = value.match(/^translate\(([-0-9.]+),([-0-9.]+)\)$/);
  if (!match) {
    return null;
  }
  return {
    x: Number(match[1]),
    y: Number(match[2]),
  };
}

function clamp(value, minimum, maximum) {
  return Math.min(Math.max(value, minimum), maximum);
}

function createSvgElement(document, name, attributes) {
  const element = document.createElementNS("http://www.w3.org/2000/svg", name);
  Object.entries(attributes).forEach(([key, value]) => {
    element.setAttribute(key, value);
  });
  return element;
}

function addTodayMarker(svg) {
  const timeline = svg.querySelector("#timeline");
  const legend = svg.querySelector("#timeline-legend");
  const axis = timeline?.querySelector(".axis");
  const windowStart = svg.dataset.windowStart;
  const windowEnd = svg.dataset.windowEnd;
  if (!timeline || !legend || !axis || !windowStart || !windowEnd) {
    return;
  }

  const timelineTransform = parseTranslate(timeline.getAttribute("transform") || "");
  const legendTransform = parseTranslate(legend.getAttribute("transform") || "");
  if (!timelineTransform || !legendTransform) {
    return;
  }

  const axisWidth = Number(axis.getAttribute("x2"));
  const axisY = Number(axis.getAttribute("y1"));
  const start = parseIsoDate(windowStart);
  const end = parseIsoDate(windowEnd);
  const today = new Date();
  const position =
    ((today.getTime() - start.getTime()) / (end.getTime() - start.getTime())) *
    axisWidth;
  const x = clamp(position, 0, axisWidth);

  const line = createSvgElement(document, "line", {
    x1: `${x}`,
    x2: `${x}`,
    y1: "0",
    y2: `${axisY}`,
    stroke: "#123047",
    "stroke-width": "2",
    "stroke-dasharray": "6 4",
  });
  timeline.appendChild(line);

  const label = createSvgElement(document, "text", {
    x: `${timelineTransform.x + x}`,
    y: `${legendTransform.y - 12}`,
    "text-anchor": "middle",
    fill: "#123047",
    "font-size": "13",
    "font-weight": "600",
  });
  label.textContent = `Today \u00b7 ${today.toISOString().slice(0, 10)}`;
  svg.appendChild(label);
}

async function inlineTimelineFigure(figure) {
  const image = figure.querySelector(
    'img[src$="bareos-supported-versions-timeline.svg"]',
  );
  if (!image || figure.dataset.timelineEnhanced === "true") {
    return;
  }

  const response = await fetch(image.currentSrc || image.src);
  if (!response.ok) {
    return;
  }

  const parser = new DOMParser();
  const text = await response.text();
  const svg = parser.parseFromString(text, "image/svg+xml").documentElement;
  svg.style.width = "100%";
  svg.style.height = "auto";
  svg.style.maxWidth = "100%";
  svg.setAttribute("role", "img");
  if (image.alt) {
    svg.setAttribute("aria-label", image.alt);
  }

  addTodayMarker(svg);
  image.replaceWith(svg);
  figure.dataset.timelineEnhanced = "true";
}

document.addEventListener("DOMContentLoaded", () => {
  const figures = document.querySelectorAll(".supported-versions-timeline");
  figures.forEach((figure) => {
    inlineTimelineFigure(figure).catch(() => {});
  });
});
