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
      <title> Bacula,die einsatzbereite Open Source Netzwerk-Backup-L&ouml;sung f&uuml;r Linux, Unix, Mac und Windows </title>
      <link rel="stylesheet" type="text/css" href="bacula.css" title="blueish">
      <meta name="Description" content="Bacula das Netzwerk Backup Programm">
      <meta name="Keywords" content="Bacula, Backup, Open Source, restore, Wiederherstellung, Daten Sicherung, enterprise,einsatzbereit">
      <meta name="Copyright" content="Copyright (C) 2000-2009 Kern Sibbald">
      <meta name="Trademark" content="Bacula (R) is a registered trademark of Kern Sibbald">
   </head>

   <body>

   <!-- Top Search Bar -->
   <div class="searchBar">
   <table class="searchBar">
   <tr>
       <td style="text-align: left; vertical-align: middle; width:50%">
               Bacula, die einsatzbereite Open Source Netzwerk-Backup-L&ouml;sung f&uuml;r Linux, Unix, Mac und Windows.
       </td>
       <td style="text-align: left; vertical-align: middle">
         <a href='/de'>
           <img alt="Deutsch" src="/images/german-flag.jpg">
         </a>&nbsp;&nbsp;&nbsp;
         <a href='/en'>
           <img alt="English" src="/images/english-flag.jpg">
         </a>&nbsp;&nbsp;&nbsp;
         <a href='/fr'>
           <img alt="Fran&ccedil;ais" src="/images/french-flag.jpg">
         </a>&nbsp;&nbsp;&nbsp;
         <a href='/es'>
           <img alt="Espa&ntilde;ol" src="../images/spanish-flag.jpg">
         </a>
       </td>
       <FORM method=GET  target="_blank" action=http://www.google.de/search>
       <td style="text-align: right; vertical-align: middle">
           <INPUT id="text" class="searchBar" type="text" name="q" size=20
             maxlength=255 value="">
          <INPUT id="button" class="searchBar" type="submit" name="sa" VALUE="Suchen">
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
               <a href='/de'><img alt="Bacula Logo" src="/images/bacu_logo-red.jpg"></a>
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
        <div class="menuHead"> allgemein </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=news"> Neuigkeiten </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=screenshot"> Screenshots </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=home"> Bacula Homepage </a></li>
           <li class="menuItem"> <a href="dev-manual/Was_ist_Bacula.html"> Was ist Bacula? </a> </li>
           <li class="menuItem"> <a href="dev-manual/Baculas_Stand.html"> aktueller Stand </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=testimonials"> Empfehlungen </a> </li>
           <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula"> SF Projektseite </a> </li>
           <li class="menuItem"> <a href="dev-manual/Systemvoraussetzungen.html"> Voraussetzungen </a> </li>
           <li class="menuItem"> <a href="dev-manual/Unterstut_Betriebssyste.html"> Betriebssysteme</a> </li>
           <li class="menuItem"> <a href="dev-manual/Unterstut_Bandlaufwerke.html"> Tape-Laufwerke </a> </li>
           <li class="menuItem"> <a href="dev-manual/Unterstut_Bandlaufwerke.html#SECTION00830000000000000000"> Autochanger </a> </li>
           <li class="menuItem"> <a href="dev-manual/Bacula_Copyri_Tradem_Licens.html"> Lizenz </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=fsfe"> FSFE Lizenz </a></li>
           </ul>
        </div>

        <!-- Documentation -->
        <!-- files need a version -->
        <div class="menuHead"> Dokumentation </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=documentation"> Dokumentation </a></li>
           <li class="menuItem"> <a href="http://wiki.bacula.org"> Wiki </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=presentations"> Pr&auml;sentationen </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=presskits"> Presse </a></li>
           </ul>
        </div>

        <!-- Downloads -->
        <div class="menuHead"> Downloads </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula/files/bacula/5.2.6/"> aktuelle Version </a> </li>
<!--       <li class="menuItem"> <a href="http://sourceforge.net/project/showfiles.php?group_id=50727&package_id=93946"> Patches </a> </li> -->
           <li class="menuItem"> <a href="http://sourceforge.net/project/showfiles.php?group_id=50727#files"> alle Versionen </a> </li>
           <li class="menuItem"> <a href="http://www.bacula.org/git/"> git Repository</a> </li>
           </ul>
        </div>

        <!-- Support -->
        <div class="menuHead"> Support </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=support"> Support </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=maillists"> E-Mail Listen </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=bugs"> Fehler-Berichte </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=professional"> professionelle Hilfe </a></li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=feature-request"> Feature Requests </a></li>
           </ul>
        </div>

        <div class="menuHead"> Latest <a href="http://sourceforge.net/apps/wordpress/bacula/"><font color=white>Blog</font></a> Entries </div>
        <div class="menuItem">
<? require_once("../rss.html"); /* update it with rss_web.php in crontab */ ?>
        </div>

        <!-- Projects -->
        <div class="menuHead"> Projekte </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="http://www.bacula.org/git/cgit.cgi/bacula/plain/bacula/projects?h=Branch-5.2"> Projekte </a> </li>
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=vote"> Project Vote </a> </li>
           </ul>
        </div>

        <div class="menuHead"> Spenden </div>
        <div class="menuItem">
           <ul class="menuitem">
           <li class="menuItem"> <a href="<? echo $spath ?>/?page=makedonation">Spenden</a> </li>
           </ul>
        </div>
        <div class="menuHead">  </div>
        <div class="icons">
           <a href="http://www.ukfast.net"><img src="/images/bacula_ukfast_logo.gif" alt="www.ukfast.net"></a>
        </div>

   </div>
   
   <div class="pageTopRight">
<!-- Es kommt bei Nacht und saugt die lebenswichtigen Daten aus Ihren Computern -->
    Die f&uuml;hrende Open Source Backup L&ouml;sung.
   </div>

   <div class="pageContent">
