This directive is used to specify a list of Job resource names that can be accessed by the console. Without this directive, the console cannot access any of the Director’s Job resources. Multiple Job resource names may be specified by separating them with commas, and/or by specifying multiple :strong:`Job ACL` directives. For example, the directive may be specified as: 

.. literalinclude:: /include/config/DirConsoleJobACL1.conf

 With the above specification, the console can
access the Director’s resources for the jobs named on the :strong:`Job ACL` directives, but for no others.

