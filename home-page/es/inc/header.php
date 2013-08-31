<?
   /**
    * grab parms from URL
    *
    */
   parse_str($_SERVER['QUERY_STRING']);

   /**
    * some settings
    *
    */
   isset($page) || $page   = "home";               // default page to show
   if (!preg_match("/^[a-zA-Z0-9_\-]+$/", $page)) {
       sleep(5);
       header("HTTP/1.0 404 Not Found");
       exit;
   }

   $page_directory  = "pages";              // directory with pages
   $page_current    = "$page_directory/$page.php";


   /**
    * Login
    *
    */
   session_start();
   if(isset($_POST['username']) and isset($_POST['password'])) {
           $user = $_POST['username'];
           $pass = $_POST['password'];

           if($user == $pass) {
                   $_SESSION['user'] = $user;
                   $_SESSION['logged_in'] = true;
           }
   }

   /**
    * Prepare links
    */
   $spath = dirname($_SERVER['SCRIPT_NAME']);
   if(strlen($spath) < 2)
           $spath = "";

?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Strict //EN" "http://www.w3.org/TR/html4/strict.dtd">

<html>
   <head>
      <title> Bacula, la soluci&oacute;n de Backup Open Source, basada en red, lista para la empresa, sobre Linux, Unix, Mac y Windows. </title>
      <link rel="stylesheet" type="text/css" href="bacula.css" title="blueish">
      <meta name="Description" content="Bacula an Open Source network backup and restore solution">
      <meta name="Keywords" content="Bacula, backup, restore, file backup, Open Source Backup, network backup, enterprise backup">
      <meta name="Copyright" content="Copyright (C) 2000-2011 Kern Sibbald">
      <meta name="Trademark" content="Bacula (R) is a registered trademark of Kern Sibbald">
   </head>

   <body>

   <!-- Top Search Bar -->
   <div class="searchBar">
   <table class="searchBar">
   <tr>
       <td style="text-align: left; vertical-align: middle; width: 50%">
            Bacula, la soluci&oacute;n de Backup Open Source, basada en red, lista para la empresa, sobre Linux, Unix, Mac y Windows.
       </td>
       <td style="text-align: left; vertical-align: middle">
         <a href='/en'>
           <img alt="English" src="../images/english-flag.jpg">
           </a>&nbsp;&nbsp;&nbsp;
         <a href='/fr'>
           <img alt="Fran&ccedil;ais" src="../images/french-flag.jpg">
         </a>&nbsp;&nbsp;&nbsp;
         <a href='/de'>
           <img alt="Deutsch" src="../images/german-flag.jpg">
         </a>&nbsp;&nbsp;&nbsp;
         <a href='/es'>
           <img alt="Espa&ntilde;ol" src="../images/spanish-flag.jpg">
         </a>


       </td>
       <FORM method=GET  target="_blank" action=http://www.google.com/search>
       <td style="text-align: right; vertical-align: middle">
           <INPUT id="text" class="searchBar" type="text" name="q" size=20
             maxlength=255 value="">
          <INPUT id="button" class="searchBar" type="submit" name="sa" VALUE="Search">
          <input type="hidden" name="domains" value="www.bacula.org">
          <input type="hidden" name="sitesearch" value="www.bacula.org">
       </td>
       </FORM>
   </tr>
   </table>
   </div>

   <!-- Logo Bar -->
   <div class="pageLogo">
       <table class="pageLogo">
         <tr>
           <td style="text-align: left; vertical-align: middle">
               <a href='/es'><img alt="Bacula Logo" src="../images/bacu_logo-red.jpg"></a>
           </td>
           <td style="text-align: right; vertical-align: middle">
              <a href='http://www.baculasystems.com/index.php/products/bee-selective-mig-plan.html'> <img alt="Bacula Logo" src="../images/bacula-banner03-500x80.gif"> </a>
         </td>
         </tr>
       </table>
   </div>

   <!-- User Bar - if logged in -->
   <?
   if(isset($_SESSION['logged_in'])) {
      printf('<div class="userBar">');
      printf('Welcome %s, <a style="color: white; text-decoration: none" href="/?page=logout">logout here</a>.', $_SESSION['user']);
      printf('</div>');
   }
   ?>

   <!-- Menu Left -->
   <div class="menuLeft">

        <!-- General -->
        <div class="menuHead"> General </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=news"> Noticias </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=screenshot"> Capturas de pantalla </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=home"> Bacula Home </a></li>
           <li class="menuItem"> <a href="dev-manual/What_is_Bacula.html"> &iquest;Qu&eacute; es Bacula? </a> </li>
           <li class="menuItem"> <a href="dev-manual/Current_State_Bacula.html"> Estado actual de Bacula </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=testimonial"> Testimonios </a> </li>
           <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula"> P&aacute;gina del proyecto en SF </a> </li>
           <li class="menuItem"> <a href="dev-manual/System_Requirements.html"> Requerimientos </a> </li>
           <li class="menuItem"> <a href="dev-manual/Supported_Operating_Systems.html"> Sistemas operativos </a> </li>
           <li class="menuItem"> <a href="dev-manual/Supported_Tape_Drives.html"> Tapedrives </a> </li>
           <li class="menuItem"> <a href="dev-manual/Supported_Autochangers.html"> Autocambiadores </a> </li>
           <li class="menuItem"> <a href="dev-manual/Bacula_Copyri_Tradem_Licens.html"> Licencia </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=fsfe"> Licencia FSFE </a></li>
           </ul>
        </div>

        <!-- Documentation -->
        <!-- files need a version -->
        <div class="menuHead"> Documentaci&oacute;n </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=documentation"> Documentaci&oacute;n </a></li>
           <li class="menuItem"> <a href="http://wiki.bacula.org"> Wiki </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=presentations"> Presentaciones </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=presskits"> Press Kits </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=articles"> Art&iacute;culos </a></li>
           </ul>
        </div>

        <!-- Downloads -->
        <div class="menuHead"> Descargas </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula/files/bacula/5.2.6/"> Archivos actuales </a> </li>
<!--       <li class="menuItem"> <a href="http://sourceforge.net/project/showfiles.php?group_id=50727&package_id=93946"> Patches</a> </li> -->
           <li class="menuItem"> <a href="http://sourceforge.net/project/showfiles.php?group_id=50727#files"> Todos los archivos </a> </li>
           <li class="menuItem"> <a href="http://www.bacula.org/git/"> Repositorio de Git </a> </li>
           </ul>
        </div>

        <!-- Support -->
        <div class="menuHead"> Soporte </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=support"> Soporte </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=maillists"> Listas de Email </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=bugs"> Reportes de Bugs </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=professional"> Profesional </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=feature-request"> Feature Requests </a></li>
           </ul>
        </div>

        <div class="menuHead"> &Uacute;ltimas entradas en el  <a href="http://sourceforge.net/apps/wordpress/bacula/"><font color=white>blog</font></a> </div>
        <div class="menuItem">
<? require_once("../rss.html"); /* update it with rss_web.php in crontab */ ?>
        </div>

        <!-- Projects -->
        <div class="menuHead"> Proyectos </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="http://www.bacula.org/git/cgit.cgi/bacula/plain/bacula/projects?h=Branch-5.2"> Proyectos </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=vote"> Project Vote </a> </li>
           </ul>
        </div>

        <div class="menuHead"> Donaciones </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=makedonation"> Hacer una Donaci&oacute;n </a> </li>
  <!--
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=donations"> Donations Received </a> </li>
  -->
           </ul>
        </div>
        <div class="menuHead">  </div>
        <div class="icons">
           <a href="http://www.ukfast.net"><img src="../images/bacula_ukfast_logo.gif" alt="www.ukfast.net"></a>
        </div>

   </div>
   
   <div class="pageTopRight">
   <!--        Llega por la noche y chupa la esencia vital de sus computadoras. -->
   The Leading Open Source Backup Solution.
   </div>

   <div class="pageContent">
