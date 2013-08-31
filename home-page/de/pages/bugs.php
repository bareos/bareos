<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
        Fehler melden
        </td>
</tr>
<tr>
    <td class="content">
    Bevor Sie einen Fehler melden, stellen Sie bitte sicher, dass es sich wirklich um 
    einen Programmfehler handelt und nicht um eine Anfrage nach Hilfe bei der Konfiguration
    (sehen Sie dazu bitte den Support-Abschnitt links im Men&uuml;).
    <p>
    Fehler werden durch freiwillige Bacula-Entwickler (sowie der Open Source
    Gemeinschaft) beseitigt. In dringenden F&auml;llen, wenn Sie
    schnelle Hilfe ben&ouml;tigen, lesen Sie bitte auch die Informationen
    &uuml;ber <a href="http://www.bacula.org/de/?page=professional">
    professionelle Hilfe</a>.
    <p>
    Bacula benutzt jetzt das, von Dan Langille implementierte,
    Mantis Fehlermeldesystem, das auch auf seiner Webseite l&auml;uft.
    Es ist webbasiert, einfach zu benutzen und wir empfehlen Ihnen es sich einmal anzusehen.
    Bevor Sie allerdings einen Fehler melden, sollte Sie die folgenden Informationen
    sorgf&auml;ltig lesen:

Um sich die Fehlerberichte anzusehen, k&ouml;nnen Sie sich mit dem
Benutzer <b>anonymous</b> und dem Passwort <b>anonymous</b> anmelden.
Wenn Sie sich auf der Seite registrieren, haben Sie allerdings den Vorteil, dass Sie
benachrichtigt werden, falls ein Fehler auftritt oder behoben wurde.

Um Fehler zu melden m&uuml;&szlig;en Sie sich ein Konto anlegen. Ihr Browser
muss einen US ASCII- oder UTF-8-Zeichensatz benutzen. Einige Benutzer, die den
Windows Internet Explorer mit osteurop&auml;ischen Zeichens&auml;tzen benutzen,
haben Problem bei der Benutzung des Systems gemeldet.

<p>
Die meisten Bacula-Fehlerberichte betreffen Probleme mit der Konfiguration.
Wenn Sie sich nicht sicher sind, ob Ihr Problem ein Programmfehler ist,
schauen Sie bitte zuerst auf den Support-Seiten nach, dort finden Sie
Links zu den E-Mail-Listen. Wie auch immer, wenn Sie wirklich ein Problem haben,
dass auf einem Programmfehler zur&uuml;ckzuf&uuml;hren ist,
m&uuml;ssen Sie entweder einen Fehlerbericht schreiben oder Ihr Problem
an die bacula-devel E-Mail-Liste schicken. Nur so k&ouml;nnen Sie sicher sein,
das die Entwickler von dem Problem erfahren und es beheben k&ouml;nnen.

Zwei Sachen unterscheiden sich in Baculas Fehlerbehandlung etwas von
anderen Open Source Projekten. Erstens: leider k&ouml;nnen wir keine Anfragen
nach Support oder Programmerweiterungen &uuml;ber das
Fehlermeldeprogramm entgegen nehmen. Und zweitens schlie&szlig;en wir
Fehlerberichte relativ schnell, um zu verhindert, dass sie sich h&auml;ufen und 
wir mit der Bearbeitung nicht hinterher kommen. Bitte nehmen Sie das nicht pers&ouml;nlich.
Wenn Sie einen Hinweis zu einem Fehlerbericht hinzuf&uuml;gen wollen nachdem er geschlossen wurde,
k&ouml;nnen Sie den Bericht erneut &ouml;ffnen, Ihren Hinweis eingeben und dann wieder schlie&szlig;en,
oder, f&uuml;r kurze Hinweise, einfach eine EMail an die bacula-devel-Liste schicken.
Wenn ein Entwickler einen Fehlerbericht schlie&szlig;t und Sie sich aufgrund neuer Informationen
sicher sind das es ein Programmfehler ist, k&ouml;nnen Sie den Fehlerbericht jederzeit wieder &ouml;ffnen.

<h3>in einem Fehlerbericht ben&ouml;tigte Informationen</h3>
Damit wir auf einen Fehlerbericht reagieren k&ouml;nnen , ben&ouml;tigen wir mindestens
die folgenden Informationen, die Sie in die entsprechenden Felder des Fehlerberichts
eingegeben m&uuml;ssen:
<ul>
<li>Ihr Betriebssystem</li>
<li>Die Bacula-Version die Sie benutzen</li>
<li>Einen <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs-de.html">klare und pr&auml;zise</a> Beschreibung des Problems</li>
<li>Wenn Sie sagen: &quot;es st&uuml;rzt ab&quot;, &quot;es funktioniert nicht&quot; oder &auml;hnliches,
sollten Sie Programmausgaben von Bacula mitschicken, die das auch best&auml;tigen.</li>
</ul>
Wenn Sie Problem mit einem Bandlaufwerk haben, teilen Sie uns bitte mit:
<ul>
 <li>Welches Bandlaufwerk Sie benutzen</li>
 <li>Haben Sie das <b>btape</b> &quot;Test&quot; Programm laufen lassen?</li>
</ul>

Den ersten Punkt k&ouml;nnen Sie erf&uuml;llen indem Sie eine Kopie der Datei <b>config.out</b>,
aus dem Bacula-Source-Verzeichnis mitschicken. Diese Datei wird w&auml;hrend der Ausf&uuml;hrung
von <b>./configure</b> erstellt.

<p>Zus&auml;tzlich ben&ouml;tigen wir manchmal eine Kopie Ihrer Bacula Konfigurations-Dateien
(speziell der bacula-dir.conf). Wenn Sie glauben, es ist ein Konfigurationsproblem, z&ouml;gern Sie nicht
diese Dateien mitzuschicken, falls n&ouml;tig.
</td>
</tr>
<tr>
   <td style="font-size: 14px; padding: 0px 20px 0px 20px">
   Bitte lesen Sie auch das kleine Fehlerberichte-<a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs-de.html">HowTo</a>.
   </td>
</tr>
<tr><td class="content">
    Sie k&ouml;nnen einen Fehler melden oder sich die Liste der offenen oder bereits
    behobenen Fehler ansehen, wenn Sie diese Seite aufrufen:

<p style="text-align: center; font-size: 24px">
        <a href="http://bugs.bacula.org">http://bugs.bacula.org</a>
</p>
</td></tr>

</table>
<? require_once("inc/footer.php"); ?>
