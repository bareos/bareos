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
#include "traymenu.h"
#include <QMainWindow>
#include <QPixmap>
#include <QTimer>

namespace {
const int kAnimationIntervalMs = 120;

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

}  // namespace

SystemTrayIcon::SystemTrayIcon(QMainWindow* mainWindow)
    : QSystemTrayIcon(mainWindow), timer(std::make_unique<QTimer>(this))
{
  bool loaded_sprite = LoadAnimationFromSpriteSheet(
      animationIcons, kRes_RunningSpriteSheet, kRes_RunningFrameCount,
      kRes_RunningFrameWidth, kRes_RunningFrameHeight);
  Q_ASSERT_X(loaded_sprite && !animationIcons.isEmpty(), "Tray animation",
             kRes_RunningSpriteSheet);
  normalIcon = animationIcons.isEmpty() ? QIcon() : animationIcons[0];

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
