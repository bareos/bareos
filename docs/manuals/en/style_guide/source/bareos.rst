Bareos specific formatting
##########################

   * Bareos Config
   
      * resource types
      * resource names

         * general resource name
         * File Set
         * Job Name
         * Pool Name
         * Volume Name

      * linkResource
      * linkResourceValue

    * Bareos Bconsole (command, session, output)
    * Messages (Logging?)
    * Volume
    
       * Status
       * Volume Paramter ??
      
    * database
       * ?
    * Plugin Events
    * Limitation
    * Since Version
    * Releases


Bareos Configuration
--------------------

Bareos Configuration Resource
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To display a Bareos specific resource configuration file, use a code block:

.. ${PERL} 's#\{bareosConfigResource\}\{(.*?)\}\{(.*?)\}\{(.*?)\}#\n.. code-block:: sh\n    :caption: \1 \2 \3\n#g'   ${DESTFILE}
   perl -i -pe 's#\{bareosConfigResource\}\{(.*)\}\{(.*)\}\{(.*)\}#\n.. code-block:: sh\n    :caption: daemon:\1 resource:\2 name:\3\n#g' ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
      :caption: bareos-dir.d/job/consolidate.conf
      
      Job {
          Name = "consolidate"
          Type = "Consolidate"
          Accurate = "yes"
          JobDefs = "DefaultJob"
      }
  
The *caption* shows the relevant path where to expect the configuration resource:

   #. <daemon>\ :file:`.d/` (bareos-dir.d, bareos-sd.d, bareos-fd.d, ...)
   #. <resource type>\ :file:`/`
   #. <resource name>\ :file:`.conf`

The prefix path is not shown, as it is platform specific.

The output should look like this:

.. code-block:: sh
   :caption: bareos-dir.d/job/consolidate.conf
    
   Job {
     Name = "consolidate"
     Type = "Consolidate"
     Accurate = "yes"
     JobDefs = "DefaultJob"
   }



Use ``...`` to indicate left out directives not relevant for the example:


.. code-block:: sh
   :caption: bareos-sd.d/device/FileStorage.conf
    
   Device {
     Name = FileStorage
     ...
     Archive Device = /var/lib/bareos/storage
     ...
   }
    
TODO
""""

* There should be a specific highlighting for Bareos configurations.
  It should be named *BareosConfig*, that can be (at least for the moment) be identical with the sh highlighting.

.. note::

   Remember to start each code-block line by 3 indenting spaces. However, the code itself is indented by the rules of the resource (2 spaces for Bareos configuration resources).


    
Include Bareos Configuration Examples
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to display the contents of a Bareos configuration file and it is present as extra file, the following formatting should be used:

.. \newcommand{\bconfigInput}[1]{
    {
        \footnotesize
   %     \lstinputlisting[backgroundcolor=\color{colorConfig},frame=single,framerule=0pt,basicstyle=\ttfamily,keepspaces=true,columns=fullflexible,moredelim=[is][\it]{<input>}{</input>},moredelim=[is][\underline]{<strong>}{</strong>}]{#1}
        \lstinputlisting[
                backgroundcolor=\color{colorConfig},
                frame=single,framerule=0pt,
                basicstyle=\ttfamily,
                columns=fullflexible,
                keepspaces=true,
                literate={-}{-}1
            ]{#1}
    }
    ${PERL} 's#\\bconfigInput\{(.*?)\}#.. literalinclude:: ../../main/\1#g' ${DESTFILE}

::

   .. literalinclude:: config/SdDeviceDeviceOptionsRados1.conf
   
All configuration snippets should be located in the :file:`config/` subdirectory of the documentation.

Normally, these snippets contain a complete Bareos configuration resource.

The output should look like this:

.. literalinclude:: config/SdDeviceDeviceOptionsRados1.conf

TODO
""""

* Who to show the configuration filename?


Resource Type
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to display a resource type, the following formatting should be used:

.. \newcommand{\resourcetype}[2]{\path|#2|$^{\mbox{\tiny #1}}$}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\resourcetype\{(.*?)\}\{(.*?)\}`#:sup:`\1`\ :strong:`\2`#g'   ${DESTFILE}

.. code-block:: sh

   :sup:`Sd` :strong:`Storage`

* Superscript: The resource type of the resource name mentioned. There can be 3 types of resources: 'Dir', 'Fd', 'Sd'

The output should look like this:

:sup:`Sd` :strong:`Storage`


Resource Name
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to display a resource name, the following formatting should be used:


.. \newcommand{\resourcename}{[...]}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\resourcename\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\resourcename\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}

.. code-block:: sh

   **Standard**:sup:`Fd`:sub:`Messages`

* Superscript: The daemon to which the resource belongs to. There can be 3 types of daemons: 'Dir', 'Fd', 'Sd'
* Subscript: The resource type of the resource.

The output should look like this:

**Standard**:sup:`Fd`:sub:`Messages`


Resource Directive
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to display a resource directive name, the following formatting should be used:

.. \makeatletter
   \newcommand{\linkResourceDirective}{%
   \@ifstar{%
   \linkResourceDirectiveStar%
   }{%
   \linkResourceDirectiveNostar%
   }%
   }
   \makeatother
    \newcommand{\linkResourceDirectiveStar}[3]{%
   \linkResourceDirectiveOpt{#1}{#2}{#3}{#3}%
   }
   \newcommand{\linkResourceDirectiveNostar}[3]{%
   \linkResourceDirectiveOpt{#1}{#2}{#3}{#3 $^{\mbox{\tiny #1}}_{\mbox{\tiny #2}}$}%
   }

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}

.. code-block:: sh

   **Always Incremental Max Full Age**:sup:`Dir`:sub:`Job`


* Superscript: The daemon to which the resource directive belongs to. There can be 3 types of daemons: 'Dir', 'Fd', 'Sd'
* Subscript: The resource type of the directive.


**Always Incremental Max Full Age**:sup:`Dir`:sub:`Job`


Resource Directive Value
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to display a resource directive along with its value, the following formatting should be used:

.. \newcommand{\linkResourceDirectiveValue}[4]{%
   \linkResourceDirective{#1}{#2}{#3} \path|= #4|%
   }

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\linkResourceDirectiveValue\{(.*?)\}\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ = **\4**#g' ${DESTFILE}

.. code-block:: sh

   **Device Type**:sup:`Sd`:sub:`Device` = **File**

The output should look like this:

**Device Type**:sup:`Sd`:sub:`Device` = **File**
