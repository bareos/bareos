<? require_once("inc/header.php"); ?>
<table>
  <tr>
    <td class="contentTopic">Demande de fonctionnalit&eacute;</td>
  </tr>
  <tr>
    <td class="content">
      <p>
        Par le pass&eacute;, les utilisateurs envoyaient des demandes de
        fonctionnalit&eacute; de mani&egrave;re informelle par mail et je les
        collectais.  Puis d&egrave;s que la version courante &eacute;tait
        publi&eacute;e, je publiais la liste des nouvelles
        fonctionnalit&eacute;s pour que les utilisateurs votent pour les
        prochaines fonctionnalit&eacute;s &agrave; impl&eacute;menter.
      </p>
      <p>
        Maintenant que Bacula est devenu un plus gros projet, le
        proc&eacute;d&eacute; est devenu un peu plus formel. Le changement
        principal est pour les utilisateurs qui doivent penser avec attention
        &agrave; leur fonctionnalit&eacute;, et l'envoyer gr&acirc;ce &agrave;
        un formulaire de demande de fonctionnalit&eacute;. Un formulaire
        quasiment vide est montr&eacute; ci-dessous ainsi qu'un exemple d'un
        formulaire rempli. Une copie texte de ce formulaire peut &ecirc;tre
        trouv&eacute;e dans le fichiers <b>projects</b> dans le
        r&eacute;pertoire principale de Bacula release. Ce fichier contient
        &eacute;galement une liste de tous les projets actuellement
        approuv&eacute;s et leur status.
      </p>
      <p>
        Le meilleur moment pour soumettre une demande de fonctionnalit&eacute;
        est juste apr&egrave;s la sortie d'une version quand je demande
        officiellement les fonctionnalit&eacute;s que vous voulez voir pour la
        prochaine version. Le pire moment pour envoyer un demande de
        fonctionnalit&eacute; est juste avant la sortie d'une nouvelle version
        (nous sommes tr&egrave;s occup&eacute; &agrave; ce moment l&agrave;).
        Pour soumettre une demande de fonctionnalit&eacute;, remplissez le
        formulaire, et envoyez le aux 2 listes <em>bacula-users</em> et
        <em>bacula-devel</em>. Ceci permettra d'en discuter ouvertement.
      </p>

      <p>
        Lorsqu'une demande de fonctionnalit&eacute; a &eacute;t&eacute;
        &eacute;tudi&eacute;e, je vais soit la rejeter, soit l'approuver, ou
        possiblement demander des modifications. Si vous avez pr&eacute;vu
        d'impl&eacute;menter la fonctionnalit&eacute; ou de donner de l'argent
        pour la faire impl&eacute;menter, c'est important de l'indiquer,
        autrement, la fonctionnalit&eacute;, m&ecirc;me approuv&eacute;e, peut
        attendre quelques temps avant que quelqu'un l'impl&eacute;mente.
      </p>

      <p>
        D&egrave;s que la demande de fonctionnalit&eacute; est
        approuv&eacute;e, je l'ajoute au fichier des projets, qui contient la
        liste de toutes les demandes de fonctionnalit&eacute; ouvertes. Le
        fichier des projets est mis &agrave; jour de temps en temps.
      </p>

      <p>
        La liste courante (peut ne pas &ecirc;tre &agrave; jour) des projets
        peut &ecirc;tre trouv&eacute;e en cliquant sur le lien <b>Projets</b>
        du menu.
      </p>

      <p>
        Les demandes de fonctionnalit&eacute;s doivent r&eacute;dig&eacute;es
        en anglais.  </p>
      <h3>Feature Request Form</h3>
      <pre>
Item n:   One line summary ...
  Origin: Name and email of originator.
  Date:   Date submitted (e.g. 28 October 2005)
  Status:

  What:   More detailed explanation ...

  Why:    Why it is important ...

  Notes:  Additional notes or features ...
      </pre>

      <h3>An Example Feature Request</h3>
      <pre>
Item 1:   Implement a Migration job type that will move the job
          data from one device to another.
  Date:   28 October 2005
  Origin: Sponsored by Riege Sofware International GmbH. Contact:
          Daniel Holtkamp <holtkamp at riege dot com>
  Status: Partially coded in 1.37 -- much more to do. Assigned to
          Kern.

  What:   The ability to copy, move, or archive data that is on a
          device to another device is very important.

  Why:    An ISP might want to backup to disk, but after 30 days
          migrate the data to tape backup and delete it from
          disk.  Bacula should be able to handle this
          automatically.  It needs to know what was put where,
          and when, and what to migrate -- it is a bit like
          retention periods.  Doing so would allow space to be
          freed up for current backups while maintaining older
          data on tape drives.

  Notes:  Migration could be triggered by:
           Number of Jobs
           Number of Volumes
           Age of Jobs
           Highwater size (keep total size)
           Lowwater mark
      </pre>
    </td>
  </tr>

</table>
<? require_once("inc/footer.php"); ?>
