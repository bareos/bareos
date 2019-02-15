:index:`[TAG=Simultaneous Jobs] <single: Simultaneous Jobs>` :index:`[TAG=Concurrent Jobs] <single: Concurrent Jobs>` This directive specifies the maximum number of total Director Jobs that should run concurrently.

The Volume format becomes more complicated with multiple simultaneous jobs, consequently, restores may take longer if Bareos must sort through interleaved volume blocks from multiple simultaneous jobs. This can be avoided by having each simultaneous job write to a different volume or by using data spooling, which will first spool the data to disk simultaneously, then write one spool file at a time to the volume thus avoiding excessive interleaving of the different job blocks.

See also the section about :ref:`Concurrent Jobs <ConcurrentJobs>`.

