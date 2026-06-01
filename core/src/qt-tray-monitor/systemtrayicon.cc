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
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
const int kNormalIcon = 0;
const int kErrorIcon = 2;
const int kAnimationIntervalMs = 120;
const int kAnimationFrameCount = 12;
const qreal kMinimumWidthScale = 0.15;
const int kMaximumSideShading = 72;
const int kShieldShadowAlpha = 72;
const int kShieldShadowYOffset = 5;
const int kShieldDepthAlpha = 44;
const int kShieldBevelAlpha = 34;
const int kShieldSpecularAlpha = 30;
const int kShieldGreenHue = 125;
const int kShieldSaturationThreshold = 12;
const QRgb kDefaultShadowColor = qRgb(12, 74, 156);
const QRgb kDefaultHighlightColor = qRgb(48, 150, 230);
const QRgb kGreenShadowColor = qRgb(12, 120, 30);
const QRgb kGreenHighlightColor = qRgb(72, 220, 96);
const char* kTrayAnimationIcon = ":/images/bareos-logo_128x128.png";
const char* kTrayErrorIcon = ":/images/W.png";

QPixmap LoadPixmapOrAssert(const char* resourcePath)
{
  QPixmap pixmap(resourcePath);
  Q_ASSERT_X(!pixmap.isNull(), "SystemTrayIcon", resourcePath);
  return pixmap;
}

QColor WithAlpha(const QColor& color, int alpha)
{
  QColor withAlpha(color);
  withAlpha.setAlpha(alpha);
  return withAlpha;
}

QColor AdjustedRgb(const QColor& color, int delta, int alpha)
{
  return QColor(std::clamp(color.red() + delta, 0, 255),
                std::clamp(color.green() + delta, 0, 255),
                std::clamp(color.blue() + delta, 0, 255), alpha);
}

QColor ScaledRgb(const QColor& color, qreal factor, int alpha)
{
  return QColor(std::clamp(static_cast<int>(std::round(color.red() * factor)),
                           0, 255),
                std::clamp(static_cast<int>(std::round(color.green() * factor)),
                           0, 255),
                std::clamp(static_cast<int>(std::round(color.blue() * factor)),
                           0, 255),
                alpha);
}

QPixmap createGreenShieldPixmap(const QPixmap& basePixmap)
{
  Q_ASSERT(!basePixmap.isNull());

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
}  // namespace

SystemTrayIcon::SystemTrayIcon(QMainWindow* mainWindow)
    : QSystemTrayIcon(mainWindow)
    , iconIdx(kNormalIcon)
    , animationFrameIdx(0)
    , animationRequested(false)
    , baseShieldPixmap(LoadPixmapOrAssert(kTrayAnimationIcon))
    , greenShieldPixmap(createGreenShieldPixmap(baseShieldPixmap))
    , normalIcon(QIcon(createShieldPixmap(0.0, getShieldPalette(false))))
    , errorIcon(kTrayErrorIcon)
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
    animationFrameIdx = 0;
  } else if (animationRequested && !timer->isActive()) {
    animationFrameIdx = 0;
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
  if (animationRequested == on) {
    if (!animationRequested && !timer->isActive()) { updateIcon(); }
    return;
  }

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
  const ShieldPalette palette = getShieldPalette(true);

  for (int frame = 0; frame < kAnimationFrameCount; ++frame) {
    const qreal angle
        = (static_cast<qreal>(frame) / kAnimationFrameCount) * 2.0 * M_PI;
    frames << QIcon(createShieldPixmap(angle, palette));
  }

  return frames;
}

SystemTrayIcon::ShieldPalette SystemTrayIcon::getShieldPalette(
    bool use_green_palette) const
{
  if (use_green_palette) {
    return {&greenShieldPixmap, QColor::fromRgb(kGreenShadowColor),
            QColor::fromRgb(kGreenHighlightColor)};
  }

  return {&baseShieldPixmap, QColor::fromRgb(kDefaultShadowColor),
          QColor::fromRgb(kDefaultHighlightColor)};
}

QPixmap SystemTrayIcon::createShieldPixmap(qreal angle,
                                           const ShieldPalette& palette) const
{
  const QPixmap& basePixmap = *palette.basePixmap;
  Q_ASSERT(!basePixmap.isNull());
  const qreal widthScale
      = std::max(kMinimumWidthScale, std::abs(std::cos(angle)));

  // Simulate a 3D flip by squashing the shield horizontally. When the back
  // side comes into view, mirror the squashed image so the rotation direction
  // stays visually consistent across the full turn.
  QPixmap scaled = basePixmap.scaled(
      std::max(1, static_cast<int>(basePixmap.width() * widthScale)),
      basePixmap.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  const int xOffset = (basePixmap.width() - scaled.width()) / 2;

  QPixmap shadowPixmap(basePixmap.size());
  shadowPixmap.fill(Qt::transparent);
  {
    QPainter shadowShapePainter(&shadowPixmap);
    shadowShapePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    shadowShapePainter.drawPixmap(xOffset, 0, scaled);
  }

  if (std::cos(angle) < 0) {
    scaled = scaled.transformed(QTransform().scale(-1, 1));
  }

  QPixmap shieldPixmap(basePixmap.size());
  shieldPixmap.fill(Qt::transparent);

  {
    QPainter shieldPainter(&shieldPixmap);
    shieldPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    shieldPainter.drawPixmap(xOffset, 0, scaled);
  }

  QPixmap framePixmap(basePixmap.size());
  framePixmap.fill(Qt::transparent);

  QPainter painter(&framePixmap);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Offset a tinted copy to fake a soft drop shadow behind the rotated shield.
  QPixmap shadowOverlay(framePixmap.size());
  shadowOverlay.fill(Qt::transparent);
  {
    QPainter shadowPainter(&shadowOverlay);
    shadowPainter.drawPixmap(0, kShieldShadowYOffset, shadowPixmap);
    shadowPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadowPainter.fillRect(shadowOverlay.rect(),
                           ScaledRgb(palette.shadow, 0.5, kShieldShadowAlpha));
  }
  painter.drawPixmap(0, 0, shadowOverlay);
  painter.drawPixmap(0, 0, shieldPixmap);

  // Add top-to-bottom highlight and shadow so the flattened shield still keeps
  // some depth while it spins.
  QPixmap depthOverlay(framePixmap.size());
  depthOverlay.fill(Qt::transparent);
  {
    QPainter depthPainter(&depthOverlay);
    QLinearGradient verticalGradient(0, 0, 0, framePixmap.height());
    verticalGradient.setColorAt(0.0,
                                WithAlpha(palette.highlight, kShieldDepthAlpha));
    verticalGradient.setColorAt(0.35, WithAlpha(palette.highlight, 8));
    verticalGradient.setColorAt(0.7, WithAlpha(palette.shadow, 0));
    verticalGradient.setColorAt(1.0, WithAlpha(palette.shadow, kShieldDepthAlpha));
    depthPainter.fillRect(depthOverlay.rect(), verticalGradient);
    depthPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    depthPainter.drawPixmap(0, 0, shieldPixmap);
  }
  painter.drawPixmap(0, 0, depthOverlay);

  // Strengthen the leading and trailing edges when the shield is most squashed
  // so the sideways phase of the rotation stays readable.
  const int sideShading
      = static_cast<int>((1.0 - widthScale) * kMaximumSideShading);
  if (sideShading > 0) {
    QPixmap sideOverlay(framePixmap.size());
    sideOverlay.fill(Qt::transparent);

    QPainter sidePainter(&sideOverlay);
    QLinearGradient sideGradient(0, 0, framePixmap.width(), 0);
    if (std::sin(angle) >= 0) {
      sideGradient.setColorAt(0.0, WithAlpha(palette.shadow, sideShading));
      sideGradient.setColorAt(0.35, WithAlpha(palette.shadow, 0));
      sideGradient.setColorAt(0.7,
                              WithAlpha(palette.highlight, sideShading * 3 / 4));
      sideGradient.setColorAt(1.0,
                              WithAlpha(palette.highlight, sideShading));
    } else {
      sideGradient.setColorAt(0.0, WithAlpha(palette.highlight, sideShading));
      sideGradient.setColorAt(
          0.3, WithAlpha(palette.highlight, sideShading * 3 / 4));
      sideGradient.setColorAt(0.65, WithAlpha(palette.shadow, 0));
      sideGradient.setColorAt(1.0, WithAlpha(palette.shadow, sideShading));
    }
    sidePainter.fillRect(sideOverlay.rect(), sideGradient);
    sidePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    sidePainter.drawPixmap(0, 0, shieldPixmap);
    painter.drawPixmap(0, 0, sideOverlay);
  }

  // Add a light rim and specular highlight so the shield keeps a glossy look
  // after the rotation and depth overlays have been applied.
  QPixmap bevelOverlay(framePixmap.size());
  bevelOverlay.fill(Qt::transparent);
  {
    QPainter bevelPainter(&bevelOverlay);
    QRadialGradient specularGradient(framePixmap.width() * 0.35,
                                     framePixmap.height() * 0.22,
                                     framePixmap.width() * 0.7);
    specularGradient.setColorAt(
        0.0, AdjustedRgb(palette.highlight, 10, kShieldSpecularAlpha));
    specularGradient.setColorAt(0.35,
                                WithAlpha(palette.highlight, kShieldBevelAlpha));
    specularGradient.setColorAt(1.0, WithAlpha(palette.highlight, 0));
    bevelPainter.fillRect(bevelOverlay.rect(), specularGradient);

    QLinearGradient rimGradient(0, 0, framePixmap.width(),
                                framePixmap.height());
    rimGradient.setColorAt(
        0.0, AdjustedRgb(palette.highlight, 12, kShieldBevelAlpha));
    rimGradient.setColorAt(0.45, QColor(0, 0, 0, 0));
    rimGradient.setColorAt(1.0, WithAlpha(palette.shadow, kShieldBevelAlpha));
    bevelPainter.fillRect(bevelOverlay.rect(), rimGradient);

    bevelPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    bevelPainter.drawPixmap(0, 0, shieldPixmap);
  }
  painter.drawPixmap(0, 0, bevelOverlay);

  return framePixmap;
}
