This plugin implements a statistics sender to Graphite.

Usage

In bareos-dir.conf enable director plugins and load the Python plugin:

    Director {
      Plugin Directory = /usr/lib64/bareos/plugins
      Plugin Names = "python"
      # ...
    }

In your JobDefs or Job Definition configure the plugin itself:

    Job {
      Name = "BackupClient1"
      DIR Plugin Options ="python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-dir-graphite-sender:collectorHost=graphite:collectorPort=2003:metricPrefix=app"
      JobDefs = "DefaultJob"
    }

* collectorHost (default graphite): IP our resolvable address of your graphite host
* collectorPort (default 2003): graphite server port
* metricPrefix (default apps) : prefix, added to all metric names

Metrics

* <metricPrefix>.bareos.jobs.<jobName>.status.(error|warning|success)
* <metricPrefix>.bareos.jobs.<jobName>.jobbytes
* <metricPrefix>.bareos.jobs.<jobName>.jobfiles
* <metricPrefix>.bareos.jobs.<jobName>.runningtime
* <metricPrefix>.bareos.jobs.<jobName>.throughput
