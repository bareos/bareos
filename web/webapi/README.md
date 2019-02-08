
ISSUES
======

1.

[nodemon] Internal watch failed: ENOSPC: System limit for number of file watchers reached, watch '/xyz/bareos/web/webapi'

Solution:

Increase the number of watches allowed for a single user. By the default the number can be low (8192 for example). When nodemon tries to watch large numbers of directories for changes it has to create several watches, which can surpass that limit.

```bash
sudo sysctl fs.inotify.max_user_watches=582222 && sudo sysctl -p
```
