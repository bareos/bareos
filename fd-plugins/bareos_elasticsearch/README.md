# The Bareos Elasticsearch Plugin 

This plugin uses Apache Tika to parse documents and sends the information including Bareos jobId to
an Elasticsearch database. 

This is Proof of Concept (PoC) code.

## Prerequisites

You need 
* a running Elasticsearch server
* Apache Tika jarfile from http://tika.apache.org/
* Python tika-app https://pypi.org/project/tika-app/
* Python Elasticsearch: https://pypi.org/project/elasticsearch/
* Bareos FD Plugin Baseclass installed: bareos-filedaemon-python-plugin from http://download.bareos.org

## Compatibility

Tested with Elasticsearch 6.5, tika-app-1.20.jar, Python elasticsearch 6.3.1, Python tika-app 1.5.0 on Bareos 17.2.

## Installation ##

1. Make sure you have met the prerequisites.
2. Install the files *BareosFdPluginElasticsearch.py* and *bareos-fd-elasticsearch.py* in your Bareos plugin directory (usually */usr/lib64/bareos/plugins*)


## Configuration ##

You have to adjust
* the location of your Tika-Jar
* Elasticsearch server

in *BareosFdPluginElasticsearch.py*:

```
es = Elasticsearch([{'host': '192.168.17.2', 'port': 9200}])
#...
tika_client = TikaApp(file_jar="/usr/local/bin/tika-app-1.20.jar")
```

The Python Tika module supports parsing of metadata only or metadata and full-text parsing.
Full text parsing is enabled by default:
```
result_payload=tika_client.extract_all_content(savepkt.fname)
# If you want meta-data only, change this to:
result_payload=tika_client.extract_only_metadata(savepkt.fname)
```

### Activate your plugin directory in the fd resource conf on the client
```
FileDaemon {                          
  Name = client-fd
  ...
  Plugin Directory = /usr/lib64/bareos/plugins
}
```

### Include the Plugin in the fileset definition as Option on the director
```
FileSet {
    Name = "client-data"
       Include  {
                Options {
                        compression=GZIP
                        signature = MD5
			Plugin = "python:module_path=/usr/lib64/bareos/plugins:module_name=bareos-fd-elasticsearch"
                }
                File = /etc
        }
}
```

#### Options ####

Not implemented, yet.


## Troubleshooting ##

Support is available here: https://www.bareos.com
