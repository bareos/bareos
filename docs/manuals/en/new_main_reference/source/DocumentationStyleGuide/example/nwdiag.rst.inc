.. nwdiag::

   {
     network dmz {
         address = "210.x.x.x/24"

         web01 [address = "210.x.x.1"];
         web02 [address = "210.x.x.2"];
     }
     network internal {
         address = "172.x.x.x/24";

         web01 [address = "172.x.x.1"];
         db01;
         app01;
     }
   }

.. nwdiag::

   nwdiag {
     inet [shape = cloud];
     inet -- router;

     network {
       router;
       web01;
       web02;
     }
   }
