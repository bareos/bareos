#!/bin/sh

echo "Bareos file daemon  uninstaller"

# Remove startup item
echo "* Bareos startup item... "
if [ -f /Library/LaunchDaemons/org.bareos.bareos-fd.plist ]; then
	sudo launchctl unload /Library/LaunchDaemons/org.bareos.bareos-fd.plist
	sudo rm /Library/LaunchDaemons/org.bareos.bareos-fd.plist
	echo "  + removed successfully"
else
  echo "  - not found, nothing to remove"
fi

echo "* Bareos file daemon... "
if [ -d "/usr/local/bareos-" ]; then
  sudo rm -r "/usr/local/bareos-"
	echo "  + removed successfully"
else
  echo "  - not found, nothing to remove"
fi

echo "* Installer receipt... "
if [ -d "/Library/Receipts/Bareos File Daemon .pkg" ]; then
  sudo rm -r "/Library/Receipts/Bareos File Daemon .pkg"
	echo "  + removed successfully"
else
  echo "  - not found, nothing to remove"
fi
