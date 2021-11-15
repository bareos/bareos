If a :config:option:`fd/client/PluginDirectory`\  is specified :strong:`Plugin Names`\  defines, which :ref:`fdPlugins` get loaded.

If :strong:`Plugin Names`\  is not defined, all plugins get loaded, otherwise the defined ones.


.. warning::

   It is highly recommended to always specify the :strong:`Plugin Names` directive
   to keep control about what plugins will be loaded by the filedaemon.
   Some plugins cannot be loaded at the same time, like the python2 and python3 plugins.
