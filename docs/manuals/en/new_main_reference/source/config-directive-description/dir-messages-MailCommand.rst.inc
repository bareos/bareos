

.. _mailcommand:

 In the absence of this resource, Bareos will send all mail using the following command:

/usr/lib/sendmail -F BAREOS <recipients>

In many cases, depending on your machine, this command may not work. However, by using the :strong:`Mail Command`, you can specify exactly how to send the mail. During the processing of the command part, normally specified as a quoted string, the following substitutions will be used:

-  %% = %

-  %c = Client’s name

-  %d = Director’s name

-  %e = Job Exit code (OK, Error, ...)

-  %h = Client address

-  %i = Job Id

-  %j = Unique Job name

-  %l = Job level

-  %n = Job name

-  %r = Recipients

-  %s = Since time

-  %t = Job type (e.g. Backup, ...)

-  %v = Read Volume name (Only on director side)

-  %V = Write Volume name (Only on director side)

Please note: any :strong:`Mail Command` directive must be specified in the Messages resource before the desired :config:option:`dir/messages/Mail`\ , :config:option:`dir/messages/MailOnSuccess`\  or :config:option:`dir/messages/MailOnError`\  directive. In fact, each of those directives may be preceded by a different :strong:`Mail Command`.

A default installation will use the program bsmtp as :strong:`Mail Command`. The program :command:`bsmtp` is provided by Bareos and unifies the usage of a mail client to a certain degree:



   .. literalinclude:: /include/config/MessagesMailCommand1.conf



The :command:`bsmtp` program is provided as part of Bareos. For additional details, please see the :ref:`bsmtp` section. Please test any :strong:`Mail Command` that you use to ensure that your smtp gateway accepts the addressing form that you use. Certain programs such as Exim can be very selective as to what forms are permitted particularly in the from part.

