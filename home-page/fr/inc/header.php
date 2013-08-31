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
    <title> Bacula, the Network Backup Tool for Linux, Unix, and Windows 
    </title>
    <link rel="stylesheet" type="text/css" href="bacula.css" title="blueish">
      <meta name="Description" content="Bacula un outil de sauvegarde et
         restauration par le r&eacute;seau">
      <meta name="Keywords" content="Bacula, backup, restore, file backup,
         sauvegarde, restauration, sauvegarde de fichiers">
      <meta name="Copyright" content="Copyright (C) 2000-2009 Kern Sibbald">
      <meta name="Trademark" content="Bacula (R) is a registered trademark of Kern Sibbald">
  </head>

  <body>

    <!-- Top Search Bar -->
    <div class="searchBar">
      <table class="searchBar">
        <tr>
          <td style="text-align: left; vertical-align: middle; width: 50%">
            Bacula, l'outil de sauvegarde r&eacute;seau pour Linux, Unix, Mac et Windows.
          </td>
          <td style="text-align: left; vertical-align: middle">
            <a href='/fr'>
              <img alt="Fran&ccedil;ais" src="../images/french-flag.jpg">
              </a>&nbsp;&nbsp;&nbsp;
            <a href='/en'>
              <img alt="English" src="../images/english-flag.jpg">
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
              <input id="text" class="searchBar" type="text" name="q" size=20
               maxlength=255 value="">
              <INPUT id="button" class="searchBar" type="submit" name="sa"
               value="Recherche">
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
            <a href='/fr'><img alt="Bacula Logo" src="../images/bacu_logo-red.jpg"></a>
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
      <div class="menuHead"> G&eacute;n&eacute;ral </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=news"> Actualit&eacute;s </a></li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=screenshot"> Captures d'&eacute;cran </a></li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=home"> Page d'accueil </a></li>
          <li class="menuItem"> <a href="dev-manual/Qu_est_ce_que_Bacula.html"> Qu'est-ce que Bacula? </a></li>
          <li class="menuItem"> <a href="dev-manual/etat_actuel_Bacula.html">
            L'&eacute;tat actuel de Bacula </a></li>
          <li class="menuItem"> <a href="dev-manual/Bacula_Copyrig_Tradema_and.html"> License </a></li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=testimonials"> T&eacute;moignages</a></li>
          <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula"> Lien du projet SF </a> </li>
          <li class="menuItem"> <a href="dev-manual/Caract_syst_ge_indisp.html"> Pr&eacute;-requis </a> </li>
          <li class="menuItem"> <a href="dev-manual/Systemese_supportes.html"> OS support&eacute;s  </a>
          </li>
          <li class="menuItem"> <a
            href="dev-manual/Lecteurs_bandes_supporte.html"> Lecteurs de bandes support&eacute;s</a> </li>
          <li class="menuItem"> <a href="dev-manual/Support_librairies.html"> Librairies support&eacute;es </a>
          </li>
        </ul>
      </div>
  
      <!-- Documentation -->
      <!-- files need a version -->
      <div class="menuHead"> Documentation </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> <a
            href="<? echo $spath ?>/?page=documentation"> Documentation </a></li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=presentations"> Presentations </a></li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=presskits"> Press Kits </a></li>
        </ul>
      </div>
  
      <!-- Downloads -->
      <div class="menuHead"> T&eacute;l&eacute;chargements </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> <a href="http://sourceforge.net/projects/bacula/files/bacula/5.2.6/"> Derni&egrave;re version </a> </li>
          <li class="menuItem"> <a
            href="http://sourceforge.net/project/showfiles.php?group_id=50727#files">
            Toutes les versions </a> </li>
           <li class="menuItem"> <a href="http://www.bacula.org/git/"> D&eacute;p&ocirc;t git
            </a></li>
        </ul>
      </div>
  
      <!-- Support -->
      <div class="menuHead"> Support </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=support">
            Obtenir du Support </a> </li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=maillists">
            Listes Email </a> </li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=bugs">
            Rapporter un Bug </a> </li>
          <li class="menuItem">
            <a href="<? echo $spath ?>/?page=professional"> Professionnels
            </a></li>
          <li class="menuItem"> <a
            href="<? echo $spath ?>/?page=feature-request"> Demandes de
            fonctionnalit&eacute;s </a></li>
        </ul>
      </div>

        <div class="menuHead"> Latest <a href="http://sourceforge.net/apps/wordpress/bacula/"><font color=white>Blog</font></a> Entries </div>
        <div class="menuItem">
<? require_once("../rss.html"); /* update it with rss_web.php in crontab */ ?>
        </div>
  
      <!-- Projects -->
      <div class="menuHead"> Projets </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> <a href="http://www.bacula.org/git/cgit.cgi/bacula/plain/bacula/projects?h=Branch-5.2"> Projects </a> </li>
          <li class="menuItem"> <a href="<? echo $spath ?>/?page=vote"> Projet Vote </a> </li>
        </ul>
      </div>
  
      <!-- Donations -->
      <div class="menuHead"> Donations </div>
      <div class="menuItem">
        <ul class="menuitem">
          <li class="menuItem"> 
              <a href="<? echo $spath ?>/?page=makedonation"> Faire un Don </a></li>
          <li class="menuItem">
              <a href="<? echo $spath ?>/?page=donations"> Dons Re&ccedil;us </a> </li>
        </ul>
      </div>
  
      <div class="menuHead"> </div>
      <div class="icons">
           <a href="http://www.ukfast.net"><img src="../images/bacula_ukfast_logo.gif" alt="www.ukfast.net"></a>
           <!-- a href="http://validator.w3.org/check?uri=referer"><img src="images/valid-xhtml10.png" alt="valix w3c logo"></a -->
           <!-- a href="http://jigsaw.w3.org/css-validator/validator?uri=<? echo $_SERVER['HTTP_REFERER']; ?>"><img src="images/vcss.png" alt="valid css logo"></a-->
      </div>

   </div>
   
   <div class="pageTopRight">
<!--  Il surgit de la nuit et absorbe l'essence vitale de vos ordinateurs. -->
      La Solution Leader dans Open Source Saufgard.
   </div>

   <div class="pageContent">
