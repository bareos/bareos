<? require_once("inc/header.php"); ?>
<table>
  <tr>
    <td class="contentTopic">
      Rapport de Bug
    </td>
  </tr>
  <tr>
    <td class="content">
      <h3>Comment rapporter un bug?</h3>
      Avant de rapporter un bug, merci de v&eacute;rifier qu'il s'agisse bien d'un
      bug et non d'une demande de support (voir la rubrique support du menu).
      <p>
        Le projet Bacula poss&egrave;de maintenant un syst&egrave;me de rapport de bug
        impl&eacute;ment&eacute; par Dan Langille et h&eacute;berg&eacute; sur son serveur. Il s'agit d'une
        application web facile &aacute; utiliser que nous vous recommandons
        d'essayer. Vous pouvez soumettre un bug ou voir la liste des bugs
        ouverts ou ferm&eacute;s sur la page: 
      </p>
      <p style="text-align: center; font-size: 24px">
        <a href="http://bugs.bacula.org">http://bugs.bacula.org</a>
      </p>
      <p>
        Pour consulter les rapports de bugs, vous pouvez vous logguer en
        utilisant l'utilisateur <b>anonymous</b> et le mot de passe
        <b>anonymous</b>.
      </p>
      <p>
        Pour soumettre un bug, vous devez cr&eacute;er un compte.
      </p>
      <p>
        La plupart des probl&egrave;mes soulev&eacute;s &aacute; propos de Bacula sont des questions
        de support, donc si vous n'&ecirc;tes pas s&ucirc;r si votre probl&egrave;me est un bug,
        merci de consulter la page de support sur ce site afin d'obtenir
        les adresses des listes d'emails.
      </p>


      <h3>Informations n&eacute;cessaires &aacute; l'ouverture d'un bug</h3>
      <p>
        Pour que nous puissions r&eacute;pondre &aacute; un rapport de bug, nous avons
        normalement besoin d'un minimum d'informations, que vous pouvez entrer
        dans les champs appropri&eacute;s du syst&egrave;me de rapport de bug:
      </p>
      <ul>
        <li>votre syst&egrave;me d'exploitation</li>
        <li>la version de Bacula que vous utilisez</li>
        <li>une description <a
        href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">claire
        et concise</a> du probl&egrave;me (en anglais)</li>
        <li>Si vous avez un plantage, un disfonctionnement ou quelque chose
        de similaire, vous devez inclure dans votre description les messages
        de sorties de Bacula.</li>
      </ul>
      <p>
        Si vous avez des probl&egrave;me de bande, merci d'inclure la marque et le
        type de lecteur de bandes que vous utilisez. Assurez-vous &eacute;galement
        d'avoir ex&eacute;cut&eacute; la commande <b>btape</b> "test".
      </p>
      <p>
        Les 2 premiers items peuvent &ecirc;tre remplis en envoyant une copie de
        votre fichier <b>config.out</b>, qui est localis&eacute; dans le r&eacute;pertoire
        principal des sources de <b>Bacula</b>, apr&egrave;s avoir ex&eacute;cut&eacute; la
        commande <b>./configure</b>.
      </p>
      <p>
        Vous pouvez &eacute;galement ajouter une copie de vos fichiers de
        configuration (tout particuli&egrave;rement bacula-dir.conf) qui est
        n&eacute;cessaire parfois. Si vous pensez qu'il s'agit d'un probl&egrave;me de
        configuration, n'h&eacute;sitez pas &aacute; nous les envoyer si n&eacute;cessaire.
      </p>
    </td>
  </tr>
  <tr>
    <td style="font-size: 14px; padding: 0px 20px 0px 20px">
      Merci de lire &eacute;galement ce petit 
      <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs-fr.html">
        Bug-Report-HowTo</a> (en fran&ccedil;ais).
    </td>
  </tr>

</table>
<? require_once("inc/footer.php"); ?>
