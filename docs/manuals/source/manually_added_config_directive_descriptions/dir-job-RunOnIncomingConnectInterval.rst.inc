.. uml::
  :caption: Timing example for Run On Incoming Connect Interval

  @startgantt
  [Run On Incoming Connect Interval = 35h] lasts 35 days and is colored in LightBlue
  [Run On Incoming Connect Interval starts again -->] lasts 18 days and is colored in LightBlue

  -- Backups --
  [Successful Backup] lasts 8 days
  [Successful Backup again] lasts 11 days

  -- Client connection status --
  [Client connected] lasts 10 days and is colored in Lime
  then [Client disconnected] lasts 10 days and is colored in DeepPink
  [Connect does not trigger] happens at [Client disconnected]'s end
  then [Client connected again] lasts 10 days and is colored in Lime
  then [Client disconnected again] lasts 13 days and is colored in DeepPink
  [Connect triggers backup] happens at [Client disconnected again]'s end
  then [Client connected again 2] lasts 11 days and is colored in Lime
  [Client disconnected again] -> [Client connected again 2]
  [Client disconnected again] -> [Successful Backup again]
  [Run On Incoming Connect Interval starts again -->] starts at [Successful Backup again]'s start

  @endgantt