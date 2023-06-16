.. _sec:perf:

Performance Tracking
====================

General
-------
It is possible to track how much time a daemon spends inside predefined blocks
of code.  For the following sections we will use the following imaginary
function inside the file daemon:

.. code-block:: c++

   void some_function(JobControlRecord* jcr, ...)
   {
      ...
   }

Timing a block of code
----------------------
We can tell bareos to track how much time it spends inside a block by notifying it
whenever we enter and leave that particular block.  Let us look at how one might
do that for the given example function.

First of we need some way to for bareos to distingiush which block we are
talking about.  This is done by the way of BlockIdentity objects.  These objects
just contain a pointer their display name.

.. code-block:: c++

   void some_function(JobControlRecord* jcr, ...)
   {
     static const BlockIdentity my_timed_block{"some_function"};
     ...
   }

Bareos does not care about the actual contents of BlockIdentity; it just uses
its address to distinguish between blocks.  Thus it is extremely important to
create them in such a way that their address does not change.  This can be done
by defining them as static inside a function or just using a global variable.

At the moment nothing will happen still since we did not tell bareos that we
entered or left my_timed_block.

To submit the enter/leave events we first of we need to get a handle to the
job-local event stream

.. code-block:: c++

   void some_function(JobControlRecord* jcr, ...)
   {
     static const BlockIdentity my_timed_block{"some_function"};
     auto handle = jcr->get_thread_local_timer();
     ...
   }

As can be seen in the function name, events are buffered thread locally and then
later on given to the per-job processor.

.. note::
   Since each handle retrieved from the jcr implictly depends on that jcr it is
   of utmost importance that the handle does not outlive that jcr.


Now that we can submit events it becomes possible to tell bareos that we
entered/left our block.  There are two ways to do this.
We can create a RAII handle which will enter the given block on construction and
leave it on destruction like so:

.. code-block:: c++

   void some_function(JobControlRecord* jcr, ...)
   {
     static const BlockIdentity my_timed_block{"some_function"};
     auto handle = jcr->get_thread_local_timer();

     TimedBlock raii{handle, my_timed_block}; // automatically tells bareos that
                                              // we entered
     ...

     // raii gets destroyed here and as such tells bareos that we left our block
   }

This is the recommended way to time your code since it becomes very hard to
leave the event stream in an inconsistent way.  If you want to repeatedly switch
between two blocks you can use the switch_to member function like so:

.. code-block:: c++

   bool pipe_some_data(JobControlRecord* jcr, source in, sink out)
   {
     static const BlockIdentity reading{"reading from source"};
     static const BlockIdentity writing{"writing to sink"};

     auto handle = jcr->get_thread_local_timer();
     TimedBlock current_block{handle, reading};
     Buffer buf;
     for (;;)
     {
       if (!source.read(buf)) {
         return false;
       }

       // this will tell bareos that we are leaving the reading block
       // and are entering the writing block
       current_block.switch_to(writing);
       if (!sink.write(buf)) {
         return false;
       }

       if (source.at_end()) {
         return true;
       }
       current_block.switch_to(reading);
     }
   }

There is also a second, manual way to submit such events:


.. code-block:: c++

   void some_function(JobControlRecord* jcr, ...)
   {
     ...
     static const BlockIdentity unsafe{"this is not recommended"};
     handle.enter(unsafe);
     do_work();
     handle.exit(unsafe);
     ...
   }

Sometimes such manual code might be necessary but in general it is not
recommended since it is very easy to make mistakes; especially when another
developer needs to edit the code.  Any missed enter() or exit() calls will lead
to bad results.  In the best case the event system will detect the error and
discard any data for that thread, and in the worst case two errors will cancel
out and we just silently produce bad data.

Displaying gathered performance statistics
------------------------------------------

If performance tracking was enabled for a particular job and the debug was was
set to at least 500 a final report will be printed to the debug log. See
:ref:`here <bcommandSetdebug>` for more details.


bconsole interface
------------------

It is possible to query the daemons for a live snapshot of the processed
performance events and display them in various ways.  This can be done with the
performance command.

In total we currently have three different ways to view that snapshot from the
bconsole:

.. code-block:: bconsole
   :caption: possible performance report invocations

   report about=perf <report options> [style=callstack depth=all|<max_depth> relative[=yes|no]]
   report about=perf <report options> style=overview [show=all|<top_n> relative[=yes|no]]
   report about=perf <report options> style=collapsed [depth=all|<max_depth>]

.. seealso:: :ref:`the bconsole report command <bcommandReport>`

callstack style
^^^^^^^^^^^^^^^

This is the default style that gets selected when no style is specified.  It
displays a sorted tree of blocks in which each parent called its children.
Each line contains one block, the time spent inside that block when called from
its parents and what percentage this constitutes of the parents time if relative
is set to yes -- which is the default -- or of the total time otherwise.

Blocks may appear mulitple times in this tree since the blocks can be entered
from different contexts.

You can restrict the depth of this tree with the depth parameter.  If depth is
set to all, the whole tree will be displayed otherwise.

overview style
^^^^^^^^^^^^^^

When the overview style is selected a sorted list of blocks -- one per line --
gets printed.  If relative is no -- which is the default for this style -- the
time associated with each block is simply the duration that bareos was inside
this block.  Otherwise, if relative is yes, the time will instead be the
duration bareos was inside this block, but not inside any other block.

The number of lines may be restricted with the show parameter.  If its set to
all instead all blocks get printed.

collapsed style
^^^^^^^^^^^^^^^

The output of this can be piped into `flamegraph.pl <https://www.brendangregg.com/flamegraphs.html>`_
to create a flamegraph representation in svg form.  It is not very enlightening on its own.

.. limitation:: live viewing: buffering

   Due to the buffered nature of the event stream you can run into unexpected
   results when running the report command.  For accurate reporting it is
   necessary to create enough events that the buffers are regularly flushed.
   The debug message at the end of each job should contain the correct data in
   any case.

Some text
