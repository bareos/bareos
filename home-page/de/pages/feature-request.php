<? require_once("inc/header.php"); ?>
<table>
<tr>
   <td class="contentTopic"> Feature Requests<br>(Anfrage einer Programmerweiterung) </td>
</tr>
<tr>
   <td class="content">
+<h2>Funded Development</h2>
+Now that <a href="http://www.baculasystems.com">Bacula Systems SA</a> exists,
+it is possible to sponsor funded development projects. This is a contractual
+relationship where the Bacula Systems developers agree to implement
+a specific project within a specific time frame. All code that is developed
+by Bacula Systems developers, goes into the Bacula community source
+repository, so is available for everyone to use.
+
+<h2>Community Development</h2>
+If you are not interested in sponsoring a development project, you can
+nevertheless submit a feature request to have a favorite feature
+implemented (and even submit your own patch for it).  
+
<p>
In der Vergangenheit haben die Benutzer solche Anfragen formlos per E-Mail geschickt
und wir haben sie gesammelt. Nachdem eine neue Bacula-Version fertiggestellt war,
haben wir die Liste der Feature Requests ver&ouml;ffentlicht und die Benutzer konnten
dar&uuml;ber abstimmen.
<p>
Jetzt, da Bacula ein immer gr&ouml;&szlig;eres Projekt geworden ist,
wurde dieser Prozess etwas formeller gestaltet. Die wichtigste &Auml;nderung f&uuml;r
die Benutzer ist, dass Sie sorgf&auml;ltig &uuml;ber ihre Anfrage nachdenken sollten,
bevor sie sie in Form eines Feature Requests abschicken. Ein Beispiel eines leeren
und eines ausgef&uuml;llten Feature Requests finden Sie weiter unten.
Eine Kopie dieses Formulars finden Sie auch am Ende der Datei <b>projects</b>
im Hauptverzeichnis des Bacula-Quelltextes. Diese Datei enth&auml;lt zudem eine
Liste aller momentan angenommenen Programmerweiterungen, sowie auch den Stand
ihrer Entwicklung.
<p>
Der beste Zeitpunkt nach einer Erweiterung zu fragen, ist nachdem eine neue Bacula-Version
freigegeben wurde und wir &ouml;ffentlich nachfragen, welche Erweiterungen sich die Benutzer f&uuml;r
die n&auml;chste Version ~w&uuml;nschen~. Der schlechteste Zeitpunkt f&uuml;r einen
Feature Request ist kurz vor der Ver&ouml;ffentlichung einer neuen Bacula-Version
(wo wir die meiste Zeit sehr besch&auml;ftigt sind). Zum tats&auml;chlichen  Anfragen
einer Erweiterung von Bacula, f&uuml;llen Sie bitte das Formular aus und schicken es
sowohl an die bacula-user- als auch an die bacula-devel-E-Mail-Liste. Dort kann Ihr Vorschlag
dann &ouml;ffentlich diskutiert werden.
<p>
Nach einer angemessene Diskussion &uuml;ber den Feature Request,
wird der Bacula Projekt Manager (Kern) die Anfrage entweder ablehnen, akzeptieren oder eventuell
nach einigen Nachbesserungen fragen. Falls Sie planen das Feature selbst zu
implementieren oder etwas spenden m&ouml;chten damit es eingebaut wird,
ist dies ein wichtiger Punkt, andernfalls kann es sein, dass obwohl Ihr Feature
Request angenommen wurden ist, es ziemlich lange dauert, bevor jemand es in
Bacula implementiert.
<p>
Wenn der Feature Request angenommen wurde, f&uuml;gen wir ihn der projects-Datei hinzu,
die eine Liste aller offenen Requests beinhaltet. Diese Datei wird von Zeit zu Zeit aktualisiert.
<p>
Die aktuelle Version der laufenden Projekte (eventuell nicht ganz auf dem neusten Stand)
kann auch auf der Webseite, im Men&uuml; unter <b>Projekte</b>, gefunden werden.
<p>
Da sowohl die E-Mail-Listen, als auch die meisten Benutzer, englisch sprechen, sollte der
Feature Request selbstverst&auml;ndlich auf Englisch verfasst werden.
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

<h3>Beispiel eines Feature Request</h3>
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
