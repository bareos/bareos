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
#include "resources.h"
#include <qnamespace.h>
#include "traymenu.h"
#include <QColor>
#include <QFile>
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
const int kAnimationIntervalMs = 120;
const int kAnimationFrameCount = 12;
const qreal kMinimumWidthScale = 0.15;
const int kMaximumSideShading = 72;
const int kShieldShadowAlpha = 72;
const int kShieldShadowYOffset = 5;
const int kShieldDepthAlpha = 44;
const int kShieldBevelAlpha = 34;
const int kShieldSpecularAlpha = 30;

struct palette {
  QRgb color;
  QRgb shadow;
  QRgb highlight;
};

static constexpr palette IdlePalette
    = {qRgb(18, 123, 202), qRgb(12, 74, 156), qRgb(48, 150, 230)};

static constexpr palette RunningPalette
    = {qRgb(0, 191, 17), qRgb(12, 120, 30), qRgb(72, 220, 96)};

static bool RenderIcon(QPixmap& output,
                       const QPixmap& logo,
                       const QPixmap& background,
                       QRgb color)
{
  output.fill(Qt::transparent);
  QPainter p(&output);

  p.drawPixmap(output.rect(), logo);

  // this colors everything that we drew in the given color
  p.setCompositionMode(QPainter::CompositionMode_SourceIn);
  p.fillRect(output.rect(), color);

  // the background should always be white and should ofc be drown _below_
  // our actual logo
  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.drawPixmap(output.rect(), background);

  p.end();

  return true;
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
  return QColor(
      std::clamp(static_cast<int>(std::round(color.red() * factor)), 0, 255),
      std::clamp(static_cast<int>(std::round(color.green() * factor)), 0, 255),
      std::clamp(static_cast<int>(std::round(color.blue() * factor)), 0, 255),
      alpha);
}

bool MakeRotatedIcon(QIcon& icon,
                     qreal angle,
                     QPixmap const& basePixmap,
                     palette ColorPalette)
{
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
    shadowPainter.fillRect(
        shadowOverlay.rect(),
        ScaledRgb(ColorPalette.shadow, 0.5, kShieldShadowAlpha));
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
    verticalGradient.setColorAt(
        0.0, WithAlpha(ColorPalette.highlight, kShieldDepthAlpha));
    verticalGradient.setColorAt(0.35, WithAlpha(ColorPalette.highlight, 8));
    verticalGradient.setColorAt(0.7, WithAlpha(ColorPalette.shadow, 0));
    verticalGradient.setColorAt(
        1.0, WithAlpha(ColorPalette.shadow, kShieldDepthAlpha));
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
      sideGradient.setColorAt(0.0, WithAlpha(ColorPalette.shadow, sideShading));
      sideGradient.setColorAt(0.35, WithAlpha(ColorPalette.shadow, 0));
      sideGradient.setColorAt(
          0.7, WithAlpha(ColorPalette.highlight, sideShading * 3 / 4));
      sideGradient.setColorAt(1.0,
                              WithAlpha(ColorPalette.highlight, sideShading));
    } else {
      sideGradient.setColorAt(0.0,
                              WithAlpha(ColorPalette.highlight, sideShading));
      sideGradient.setColorAt(
          0.3, WithAlpha(ColorPalette.highlight, sideShading * 3 / 4));
      sideGradient.setColorAt(0.65, WithAlpha(ColorPalette.shadow, 0));
      sideGradient.setColorAt(1.0, WithAlpha(ColorPalette.shadow, sideShading));
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
        0.0, AdjustedRgb(ColorPalette.highlight, 10, kShieldSpecularAlpha));
    specularGradient.setColorAt(
        0.35, WithAlpha(ColorPalette.highlight, kShieldBevelAlpha));
    specularGradient.setColorAt(1.0, WithAlpha(ColorPalette.highlight, 0));
    bevelPainter.fillRect(bevelOverlay.rect(), specularGradient);

    QLinearGradient rimGradient(0, 0, framePixmap.width(),
                                framePixmap.height());
    rimGradient.setColorAt(
        0.0, AdjustedRgb(ColorPalette.highlight, 12, kShieldBevelAlpha));
    rimGradient.setColorAt(0.45, QColor(0, 0, 0, 0));
    rimGradient.setColorAt(1.0,
                           WithAlpha(ColorPalette.shadow, kShieldBevelAlpha));
    bevelPainter.fillRect(bevelOverlay.rect(), rimGradient);

    bevelPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    bevelPainter.drawPixmap(0, 0, shieldPixmap);
  }
  painter.drawPixmap(0, 0, bevelOverlay);

  icon = framePixmap;
  return true;
}

bool RotateIcon(QList<QIcon>& list,
                QPixmap const& base,
                size_t frameCount,
                palette ColorPalette)
{
  for (size_t frame = 0; frame < frameCount; ++frame) {
    const qreal angle = (static_cast<qreal>(frame) / frameCount) * 2.0 * M_PI;

    QIcon nextIcon;
    if (!MakeRotatedIcon(nextIcon, angle, base, ColorPalette)) { return false; }

    list << nextIcon;
  }

  return true;
}

bool LoadAnimationFromSpriteSheet(QList<QIcon>& icons,
                                  const char* spritePath,
                                  int frameCount,
                                  int frameWidth,
                                  int frameHeight)
{
  QPixmap sheet{spritePath};
  if (sheet.isNull() || frameCount <= 0 || frameWidth <= 0 || frameHeight <= 0
      || sheet.height() < frameHeight
      || sheet.width() < frameCount * frameWidth) {
    return false;
  }

  for (int frame = 0; frame < frameCount; ++frame) {
    QPixmap pixmapFrame = sheet.copy(frame * frameWidth, 0, frameWidth,
                                     frameHeight);
    if (pixmapFrame.isNull()) { return false; }
    icons << QIcon(pixmapFrame);
  }

  return true;
}

QPixmap LoadSvg(const char* path, int width, int height)
{
  QPixmap px{width, height};
  if (!px.load(path)) { return {}; }
  return px;
}

}  // namespace

SystemTrayIcon::SystemTrayIcon(QMainWindow* mainWindow)
    : QSystemTrayIcon(mainWindow), timer(std::make_unique<QTimer>(this))
{
  bool loaded_sprite = LoadAnimationFromSpriteSheet(
      animationIcons, kRes_RunningSpriteSheet, kRes_RunningFrameCount,
      kRes_RunningFrameWidth, kRes_RunningFrameHeight);
  Q_ASSERT(loaded_sprite || !animationIcons.size());

  if (loaded_sprite && !animationIcons.isEmpty()) {
    normalIcon = animationIcons[0];
  } else {
    constexpr int IconWidth = 128;
    constexpr int IconHeight = 128;

    auto logo = LoadSvg(kRes_LogoIcon, IconWidth, IconHeight);
    Q_ASSERT_X(!logo.isNull(), "Tray Logo", kRes_LogoIcon);

    auto bg = LoadSvg(kRes_LogoBGIcon, IconWidth, IconHeight);
    Q_ASSERT_X(!bg.isNull(), "Tray Logo Background", kRes_LogoBGIcon);

    QPixmap defaultIcon(IconWidth, IconHeight);
    bool default_icon_ok
        = RenderIcon(defaultIcon, logo, bg, IdlePalette.color);
    Q_ASSERT(default_icon_ok);

    QPixmap runningIcon(IconWidth, IconHeight);
    bool running_icon_ok
        = RenderIcon(runningIcon, logo, bg, RunningPalette.color);
    Q_ASSERT(running_icon_ok);

    bool running_animation_ok = RotateIcon(animationIcons, runningIcon,
                                           kAnimationFrameCount,
                                           RunningPalette);
    Q_ASSERT(running_animation_ok);
    normalIcon = std::move(defaultIcon);
  }

  errorIcon = QIcon(kRes_ErrorIcon);

  // this object name is used for auto-connection to the MainWindow
  setObjectName("SystemTrayIcon");

  TrayMenu* menu = new TrayMenu(mainWindow);
  setContextMenu(menu);
  updateIcon();
  setToolTip("Bareos Tray Monitor");

  timer->setInterval(kAnimationIntervalMs);
  connect(timer.get(), SIGNAL(timeout()), this, SLOT(setIconInternal()));
}

void SystemTrayIcon::setNewIcon(IconType type)
{
  currentIcon = type;
  if (currentIcon == IconType::Error) {
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
  if (currentIcon == IconType::Error || animationIcons.isEmpty()
      || !timer->isActive()) {
    return;
  }

  animationFrameIdx = (animationFrameIdx + 1) % animationIcons.size();
  if (!animationRequested && animationFrameIdx == 0) { timer->stop(); }
  updateIcon();
}

void SystemTrayIcon::animateIcon(bool on)
{
  if (animationRequested == on) {
    if (!animationRequested && !timer->isActive()) { updateIcon(); }
    return;
  }

  animationRequested = on;
  if (animationRequested && currentIcon != IconType::Error) {
    if (!timer->isActive()) {
      animationFrameIdx = 0;
      timer->start();
    }
  } else {
    if (!timer->isActive() || currentIcon == IconType::Error
        || animationFrameIdx == 0) {
      timer->stop();
      animationFrameIdx = 0;
    }
  }

  updateIcon();
}

void SystemTrayIcon::updateIcon()
{
  if (currentIcon == IconType::Error) {
    setIcon(errorIcon);
  } else if (timer->isActive() && !animationIcons.isEmpty()) {
    setIcon(animationIcons[animationFrameIdx]);
  } else {
    setIcon(normalIcon);
  }
}
