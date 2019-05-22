.. seqdiag::
   :desctable:

   seqdiag {
      A -> B -> C;
      A [description = "browsers in each client"];
      B [description = "web server"];
      C [description = "database server"];
   }

.. seqdiag::

   seqdiag {
     # define order of elements
     # seqdiag sorts elements by order they appear
     browser; database; webserver;

     browser  -> webserver [label = "GET /index.html"];
     browser <-- webserver;
     browser  -> webserver [label = "POST /blog/comment"];
                 webserver  -> database [label = "INSERT comment"];
                 webserver <-- database;
     browser <-- webserver;
   }
