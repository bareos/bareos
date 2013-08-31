         <div>
            <img src="../images/spacer.gif" alt="spacer" width="1px" height="50px"><br>
              <?
               if(isset($_SESSION['user']) && ($_SESSION['user'] == "bukdebug"))
                  print_r($_SERVER);
              ?>
         </div>
      </div>
   </body>
</html>
