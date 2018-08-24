# Bareos FileDaemon Tasks Plugins

These plugins provide specific application's backup for Bareos FileDaemon.

1. XenServer Host, Pool and VM backup [bareos-fd-xenserver.py](xenserver/README.md)
2. PostgreSQL Database backup [bareos-fd-pgsql.py](psql/README.md)
3. MySQL Database backup [bareos-fd-mysql.py](mysql/README.md)
4. Oracle Database backup [bareos-fd-oracle.py](oracle/README.md)

**Warning:** These plugins comes with no warranty. They are in early stage of development.
Verify your backup before use it in production.

## Getting Start

Installation and configuration instructions are documented in each plugins.

## Deployment

I start to develop a generic base plugin class *BareosFdTaskClass.py* with the aim to create a fast and
simple way to write new plugins.

### Backup

The plugin handles a list of tasks. A task is responsible of a backup operation such as database dump,
system state or a vm export and could store one or more files. A couple of base tasks are available and
should be overridden to implement the backup operation wanted.

So, to write a new plugin you have to follow these steps:

1. Override *BareosFdTaskClass.py* class.
2. Define a list of tasks in the *prepare_tasks* function.
3. Customize the task to execute the specific backup procedure and return data to plugin.

Source code should be quite easy to understand and provided plugins could be easily used as template.

### Restore

The restore operation, at least at the moment, is not managed by this class.
File are restored in the filesystem as regular file as it is implemented in *BareosFdPluginBaseClass,py*.
Anyway, the plugin needs to be installed on the FileDaemon used as destination.

## Contributing

Contributions are welcome.

## Authors

* [**Marco Lertora**](https://github.com/marcolertora/) -  <marco.lertora@gmail.com>

## License

This project is licensed under the GNU Affero General Public License v3.0 License - see the [LICENSE](LICENSE.md) file for details
