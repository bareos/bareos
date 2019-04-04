This directive allows you to override at runtime if the Batch insert should be enabled or disabled. Normally this is determined by querying the database library if it is thread-safe. If you think that disabling Batch insert will make your backup run faster you may disable it using this option and set it to :strong:`Yes`.

