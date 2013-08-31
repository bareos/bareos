<? require_once("inc/header.phi"); ?>
<table>
<tr>
   <td class="contentTopic"> Support f&uuml;r Bacula </td>
</tr>
<tr>
   <td class="content">
	<h3>Professioneller Support</h3>
	Falls Sie professionelle Unterst&uuml;tzung suchen, 
	lesen Sie bitte auch diese Seite: <a href="http://www.bacula.org/de/?page=professional">
	http://www.bacula.org/de/?page=professional</a>.<br>

	<h3>Support durch die Open Source Gemeinschaft</h3>
   <p>Bitte bedenken Sie immer, dass niemand f&uuml;r diese Hilfe bezahlt wird.
	Nichtsdestotrotz, unser Wunsch ist es, dass so viele Leute wie m&ouml;glich <b>Bacula</b> benutzen.
	Eine Anzahl von Freiwilligen, mit sehr gro&szlig;em Fachwissen,
	sind bereit einen zumutbaren Support auf E-Mail-Basis anzubieten.

   <p>
	Bevor Sie nach Hilfe fragen, lesen Sie bitte den Abschnitt
	<b>ben&ouml;tigte Informationen</b> weiter unten auf dieser Seite.
	Zudem kann es n&uuml;tzlich sein die E-Mail-Archive zu lesen,
	und nachzusehen ob das Problem schon durch Hilfe anderer Benutzer oder durch einen
	Patch gel&ouml;st wurde. Sehen Sie hierzu bitte:
	<a href="http://news.gmane.org/search.php?match=bacula">http://news.gmane.org/search.php?match=bacula</a>.

   <p>
	Wenn Sie Bacula in produktiven Umgebungen einsetzen, ist es h&ouml;chst empfehlenswert,
	dass Sie sich bei der Bug-Datenbank unter <a href="http://bugs.bacula.org">http://bugs.bacula.org</a> registrieren,
	damit Sie im Falle eines Bugs oder neuen Patches informiert werden. Zur Informationen &uuml;ber professionelle
	Hilfe lesen Sie bitte auch die Seite <a href="http://www.bacula.org/de/?page=professional">
	http://www.bacula.org/de/?page=professional</a>.
   <p>
	Bitte senden Sie keine Support-Anfragen an die Bacula Bug-Datenbank.
	F&uuml;r Informationen &uuml;ber das Fehlermeldesystem lesen Sie bitte die Seite
	<a href=http://www.bacula.org/de/?page=bugs>Fehler melden</a>.
   <p>
	F&uuml;r Support durch die Open Source Gemeinschaft, senden Sie eine E-Mail an
	<b>bacula-users at lists.sourceforge.net</b> und wenn Ihre Angaben genau
	genug sind, wird sich bestimmt ein Bacula-Benutzer finden, der Ihnen hilft.
	Bitte denken Sie daran, dass Sie mindestens auch die Bacula-Version und das von Ihnen verwendete Betriebssystem 
	angeben. In der E-Mail-Adresse oben muss das <b>at</b> durch @ ersetzt werden.
	Aufgrund des vermehrten SPAM-Aufkommens, m&uuml;ssen Sie sich bei der E-Mail-Liste registrieren,
	damit Sie E-Mails an die Liste schicken d&uuml;rfen.
	Der Link der links im Men&uuml; mit "E-Mail Listen" bezeichnet ist, hilf Ihnen dabei weiter.
	Die Benutzer die die Liste regelm&auml;&szlig;ig lesen, werden ihre Hilfe anbieten.
	Bitte lesen Sie auch "ben&ouml;tigte Informationen" weiter unten, damit Sie wissen, was Sie alles in Ihrer Anfrage angeben m&uuml;ssen.
	Wenn Sie diese Informationen nicht mitteilen, kann es erstens l&auml;ger dauern, ehe Sie eine Antwort erhalten, oder die Benutzer
	trauen sich nicht zu antworten, weil Ihre Anfrage z.B. zu kompliziert klingt oder schlecht formuliert ist

   <p>
	Ich (Kern) bekomme eine ganze Anzahl an &quot;off-list&quot; E-Mails, die direkt an mich adressiert sind.
	Bedauerlicherweise bin ich nicht mehr in der Lage direkten Support anzubieten.
	Allerdings lese ich alle diese E-Mails und gelegentlich antworte ich mit einem Tipp oder zwei.
	Wenn Sie mir eine E-Mail schicken, setzen Sie bitte immer die entsprechende E-Mail-Liste auf CC.
	Falls Sie das nicht tun, werde ich Ihnen eventuell nicht antworten, oder ich setze bei meiner Antwort die Liste auf CC.
	Falls Sie etwas <em>wirklich</em> vertrauliches schicken, heben Sie es daher bitte deutlich hervor.
   </p>
   <p>  Bitte senden Sie keine allgemeinen Support-Anfragen an die Bacula-Entwickler E-Mail-Liste.
	Wenn Sie einen Fehler melden oder eine kleine Erweiterung vorschlagen wollen
	und daf&uuml;r eine E-Mail an die Bacula-Entwickler E-Mail-Liste schicken, kann es 
	f&uuml;r uns <b>sehr</b> frustrierend sein, wenn Sie dabei nicht die unten genannten
	Informationen mitliefern. Eventuell ist der Fehler von der benutzten Version abh&auml;ngig
	und in einer neueren bereits behoben. In jedem Fall werden wir uns das Problem ansehen, aber
	wahrscheinlich werden Sie keine Antwort bekommen, besonders wenn wir gerade sehr besch&auml;ftigt
	sind, weil wir erst nachfragen m&uuml;ssten welche Version (oder andere Informationen) Sie einsetzen und
	dann Ihre Antwort bearbeiten m&uuml;ssten, dass verdoppelt den Aufwand f&uuml;r uns.
	Wenn wir nach weiteren Informationen fragen, f&uuml;gen Sie bitte immer <b>alle</b> vorhergehenden
	E-Mails mit an, sonst m&uuml;ssen wir erst in den E-Mail-Archive nachsehen, was Sie uns vorher geschrieben
	haben. Kurz gesagt, wenn >Sie eine Antwort haben m&ouml;chten, lesen Sie den folgenden Abschnitt
	&quot; ben&ouml;tigte Informationen&quot;.

   <p>  Falls Sie Sofort-Hilfe suchen, schauen Sie sich bitte unseren IRC-Channel auf
	<a href="http://www.freenode.net">Freenode</a> an, er hei&szlig;t #bacula.

<h3>ben&ouml;tigte Informationen</h3>
	Damit wir eine Antwort auf einen Bug-Report senden k&ouml;nnen bn&ouml;tigen wir mindestens
	die folgenden Informationen die Sie in die entsprechenden Felder des Bug-Reporting-Systems eingeben
	k&ouml;nnen:
<ul>
<li>Ihr Betriebssystem</li>
<li>Die Bacula-Version die Sie benutzen</li>
<li>Eine <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">klare und pr&auml;zise</a> Beschreibung des Problems</li>
<li>Wenn Sie sagen: &quot;es st&uuml;rzt ab&quot;, &quot;es funktioniert nicht&quot; oder &auml;hnliches,
	sollten Sie Programmausgaben von Bacula mitschicken, die das auch best&auml;tigen.</li>
<li>Wenn wir nach weiteren Informationen fragen, f&uuml;gen Sie bitte immer <b>alle</b> vorhergehenden
	E-Mails mit an, so haben wir alle Information an einem Ort.</li>
 </ul>

Wenn Sie Probleme mit einem Bandlaufwerk haben, teilen Sie uns bitte mit:
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

</table>
<? require_once("inc/footer.php"); ?>
