/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "systemtrayicon.h"
#include "traymenu.h"
#include <QColor>
#include <QDebug>
#include <QImage>
#include <QLinearGradient>
#include <QMainWindow>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QTimer>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace {
const int kNormalIcon = 0;
const int kErrorIcon = 2;
const int kAnimationIntervalMs = 120;
const int kAnimationFrameCount = 12;
const qreal kMinimumWidthScale = 0.15;
const qreal kPi = 3.14159265358979323846;
const int kMaximumSideShading = 44;
const int kShieldShadowAlpha = 72;
const int kShieldShadowYOffset = 5;
const int kShieldShadowXOffset = 3;
const int kShieldDepthAlpha = 44;
const int kShieldBevelAlpha = 34;
const int kShieldSpecularAlpha = 30;
const int kShieldGreenHue = 125;
const int kShieldSaturationThreshold = 12;
const int kShadowGreenRed = 12;
const int kShadowGreenGreen = 120;
const int kShadowGreenBlue = 30;
const int kHighlightGreenRed = 72;
const int kHighlightGreenGreen = 220;
const int kHighlightGreenBlue = 96;
const char* kTrayAnimationIcon = ":/images/bareos-logo_128x128.png";
}  // namespace

SystemTrayIcon::SystemTrayIcon(QMainWindow* mainWindow)
    : QSystemTrayIcon(mainWindow)
    , iconIdx(kNormalIcon)
    , animationFrameIdx(0)
    , animationRequested(false)
    , normalIcon(QIcon(createShieldPixmap(0.0, 1.0)))
    , errorIcon(":/images/W.png")
    , animationIcons(createAnimationIcons())
    , timer(new QTimer(this))
{
  // this object name is used for auto-connection to the MainWindow
  setObjectName("SystemTrayIcon");

  TrayMenu* menu = new TrayMenu(mainWindow);
  setContextMenu(menu);
  updateIcon();
  setToolTip("Bareos Tray Monitor");

  timer->setInterval(kAnimationIntervalMs);
  connect(timer, SIGNAL(timeout()), this, SLOT(setIconInternal()));
}

SystemTrayIcon::~SystemTrayIcon() { timer->stop(); }

void SystemTrayIcon::setNewIcon(int icon)
{
  iconIdx = icon;
  if (iconIdx == kErrorIcon) {
    timer->stop();
  } else if (animationRequested && !timer->isActive()) {
    timer->start();
  }

  updateIcon();
}

void SystemTrayIcon::setIconInternal()
{
  if (!animationRequested || iconIdx == kErrorIcon
      || animationIcons.isEmpty()) {
    return;
  }

  animationFrameIdx = (animationFrameIdx + 1) % animationIcons.size();
  updateIcon();
}

void SystemTrayIcon::animateIcon(bool on)
{
  animationRequested = on;
  if (animationRequested && iconIdx != kErrorIcon) {
    animationFrameIdx = 0;
    if (!timer->isActive()) { timer->start(); }
  } else {
    timer->stop();
    animationFrameIdx = 0;
  }

  updateIcon();
}

void SystemTrayIcon::updateIcon()
{
  if (iconIdx == kErrorIcon) {
    setIcon(errorIcon);
  } else if (animationRequested && !animationIcons.isEmpty()) {
    setIcon(animationIcons[animationFrameIdx]);
  } else {
    setIcon(normalIcon);
  }
}

QList<QIcon> SystemTrayIcon::createAnimationIcons() const
{
  QList<QIcon> frames;
  QPixmap basePixmap(createBaseShieldPixmap());

  if (basePixmap.isNull()) { return frames; }

  for (int frame = 0; frame < kAnimationFrameCount; ++frame) {
    const qreal angle
        = (static_cast<qreal>(frame) / kAnimationFrameCount) * 2.0 * kPi;
    const qreal cosine = std::cos(angle);
    const qreal widthScale = std::max(kMinimumWidthScale, std::abs(cosine));
    frames << QIcon(createShieldPixmap(angle, widthScale));
  }

  return frames;
}

QPixmap SystemTrayIcon::createBaseShieldPixmap() const
{
  QPixmap basePixmap(kTrayAnimationIcon);
  if (basePixmap.isNull()) { return basePixmap; }

  QImage image = basePixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < image.height(); ++y) {
    QRgb* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      QColor color = QColor::fromRgba(scanLine[x]);
      if (color.alpha() == 0
          || color.hsvSaturation() <= kShieldSaturationThreshold) {
        continue;
      }

      QColor recolored;
      recolored.setHsv(kShieldGreenHue, color.hsvSaturation(), color.value(),
                       color.alpha());
      scanLine[x] = recolored.rgba();
    }
  }

  return QPixmap::fromImage(image);
}

QPixmap SystemTrayIcon::createShieldPixmap(qreal angle, qreal widthScale) const
{
  QPixmap basePixmap(createBaseShieldPixmap());
  if (basePixmap.isNull()) { return QPixmap(); }

  QPixmap scaled = basePixmap.scaled(
      std::max(1, static_cast<int>(basePixmap.width() * widthScale)),
      basePixmap.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

  if (std::cos(angle) < 0) {
    scaled = scaled.transformed(QTransform().scale(-1, 1));
  }

  QPixmap shieldPixmap(basePixmap.size());
  shieldPixmap.fill(Qt::transparent);

  {
    QPainter shieldPainter(&shieldPixmap);
    shieldPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int xOffset = (shieldPixmap.width() - scaled.width()) / 2;
    shieldPainter.drawPixmap(xOffset, 0, scaled);
  }

  QPixmap framePixmap(basePixmap.size());
  framePixmap.fill(Qt::transparent);

  QPainter painter(&framePixmap);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  QPixmap shadowOverlay(framePixmap.size());
  shadowOverlay.fill(Qt::transparent);
  {
    QPainter shadowPainter(&shadowOverlay);
    const int shadowX
        = static_cast<int>(std::round(std::sin(angle) * kShieldShadowXOffset));
    shadowPainter.drawPixmap(shadowX, kShieldShadowYOffset, shieldPixmap);
    shadowPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadowPainter.fillRect(shadowOverlay.rect(),
                           QColor(kShadowGreenRed / 2, kShadowGreenGreen / 2,
                                  kShadowGreenBlue / 2, kShieldShadowAlpha));
  }
  painter.drawPixmap(0, 0, shadowOverlay);
  painter.drawPixmap(0, 0, shieldPixmap);

  QPixmap depthOverlay(framePixmap.size());
  depthOverlay.fill(Qt::transparent);
  {
    QPainter depthPainter(&depthOverlay);
    QLinearGradient verticalGradient(0, 0, 0, framePixmap.height());
    verticalGradient.setColorAt(0.0,
                                QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                       kHighlightGreenBlue, kShieldDepthAlpha));
    verticalGradient.setColorAt(0.35,
                                QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                       kHighlightGreenBlue, 8));
    verticalGradient.setColorAt(
        0.7, QColor(kShadowGreenRed, kShadowGreenGreen, kShadowGreenBlue, 0));
    verticalGradient.setColorAt(
        1.0, QColor(kShadowGreenRed, kShadowGreenGreen, kShadowGreenBlue,
                    kShieldDepthAlpha));
    depthPainter.fillRect(depthOverlay.rect(), verticalGradient);
    depthPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    depthPainter.drawPixmap(0, 0, shieldPixmap);
  }
  painter.drawPixmap(0, 0, depthOverlay);

  const int sideShading
      = static_cast<int>((1.0 - widthScale) * kMaximumSideShading);
  if (sideShading > 0) {
    QPixmap sideOverlay(framePixmap.size());
    sideOverlay.fill(Qt::transparent);

    QPainter sidePainter(&sideOverlay);
    QLinearGradient sideGradient(0, 0, framePixmap.width(), 0);
    if (std::sin(angle) >= 0) {
      sideGradient.setColorAt(0.0, QColor(kShadowGreenRed, kShadowGreenGreen,
                                          kShadowGreenBlue, sideShading));
      sideGradient.setColorAt(0.45, QColor(kShadowGreenRed, kShadowGreenGreen,
                                           kShadowGreenBlue, 0));
      sideGradient.setColorAt(0.75,
                              QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                     kHighlightGreenBlue, sideShading / 2));
      sideGradient.setColorAt(1.0,
                              QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                     kHighlightGreenBlue, sideShading));
    } else {
      sideGradient.setColorAt(0.0,
                              QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                     kHighlightGreenBlue, sideShading));
      sideGradient.setColorAt(0.25,
                              QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                     kHighlightGreenBlue, sideShading / 2));
      sideGradient.setColorAt(0.55, QColor(kShadowGreenRed, kShadowGreenGreen,
                                           kShadowGreenBlue, 0));
      sideGradient.setColorAt(1.0, QColor(kShadowGreenRed, kShadowGreenGreen,
                                          kShadowGreenBlue, sideShading));
    }
    sidePainter.fillRect(sideOverlay.rect(), sideGradient);
    sidePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    sidePainter.drawPixmap(0, 0, shieldPixmap);
    painter.drawPixmap(0, 0, sideOverlay);
  }

  QPixmap bevelOverlay(framePixmap.size());
  bevelOverlay.fill(Qt::transparent);
  {
    QPainter bevelPainter(&bevelOverlay);
    QRadialGradient specularGradient(framePixmap.width() * 0.35,
                                     framePixmap.height() * 0.22,
                                     framePixmap.width() * 0.7);
    specularGradient.setColorAt(
        0.0, QColor(kHighlightGreenRed + 10, kHighlightGreenGreen + 10,
                    kHighlightGreenBlue + 10, kShieldSpecularAlpha));
    specularGradient.setColorAt(0.35,
                                QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                       kHighlightGreenBlue, kShieldBevelAlpha));
    specularGradient.setColorAt(1.0,
                                QColor(kHighlightGreenRed, kHighlightGreenGreen,
                                       kHighlightGreenBlue, 0));
    bevelPainter.fillRect(bevelOverlay.rect(), specularGradient);

    QLinearGradient rimGradient(0, 0, framePixmap.width(),
                                framePixmap.height());
    rimGradient.setColorAt(
        0.0, QColor(kHighlightGreenRed + 12, kHighlightGreenGreen + 12,
                    kHighlightGreenBlue + 12, kShieldBevelAlpha));
    rimGradient.setColorAt(0.45, QColor(0, 0, 0, 0));
    rimGradient.setColorAt(1.0, QColor(kShadowGreenRed, kShadowGreenGreen,
                                       kShadowGreenBlue, kShieldBevelAlpha));
    bevelPainter.fillRect(bevelOverlay.rect(), rimGradient);

    bevelPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    bevelPainter.drawPixmap(0, 0, shieldPixmap);
  }
  painter.drawPixmap(0, 0, bevelOverlay);

  return framePixmap;
}
