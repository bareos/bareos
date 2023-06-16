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
developer needs to edit the code.  Any missed enter or exit will lead to bad
results.  In the best case the event system will detect the error and discard
any data for that thread, and in the worst case two errors will cancel out
and we just silently produce bad data.

Displaying gathered performance statistics
------------------------------------------

Using the report command
------------------------

.. limitation:: live viewing: buffering

   Due to the buffered nature of the event stream you can run into unexpected
   results when running the report command.  For accurate reporting it is
   necessary to create enough events that the buffers are regularly flushed.
   The debug message at the end of each job should contain the correct data in
   any case.

Some text
