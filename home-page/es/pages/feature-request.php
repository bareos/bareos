<? require_once("inc/header.php"); ?>
<table>
<tr>
   <td class="contentTopic"> Feature Requests </td>
</tr>
<tr>
   <td class="content">

<h2>Desarrollo Financiado (Funded Development)</h2>
Ahora que existe <a href="http://www.baculasystems.com">Bacula Systems SA</a> es posible el desarrollo de proyectos financiados. Se trata de una relaci&oacute;n contractual donde los desarrolladores de Bacula Systems acuerdan implementar un proyecto espec&iacute;fico dentro de un marco de tiempo determinado. Todo el c&oacute;digo escrito por los desarrolladores de Bacula Systems va al repositorio de la versi&oacute;n comunitaria de Bacula, as&iacute; queda disponible para el uso de todos.


<h2>Desarrollo comunitario (Community Development)</h2>
Si no est&aacute; interesado en financiar un proyecto, de todas formas puede solicitar alguna caracter&iacute;stica nueva para que sea implementada (Incluso puede enviar su propio parche o <em>patch</em>).

<p>Antes, los usuarios solicitaban alguna caracter&iacute;stica por email. Nosotros las tom&aacute;bamos y public&aacute;bamos una lista para que los usuarios votaran cuando se lanzaba una versi&oacute;n nueva.</p>

<p>Ahora que el proyecto Bacula creci&oacute;, este proceso se formaliz&oacute; un poquito m&aacute;s. El cambio principal es para los usuarios, que ahora deben pensar detenidamente lo que quieren y realizar la petici&oacute;n en un formulario. Abajo se muestra un formulario vac&iacute;o junto con uno lleno de ejemplo. Una copia textual del formulario se puede encontrar en archivo <b>projects</b> en el directorio principal de la distribuci&oacute;n de Bacula. Ese archivo tiene, tambi&eacute;n, una lista de los proyectos aprobados junto con su estado.</p>

<p>El mejor momento para enviar una solicitud es justo despu&eacute;s de un lanzamiento de Bacula, momento en el cual solicitamos oficialmente el listado de caracter&iacute;sticas para la siguiente versi&oacute;n. El peor momento para enviar una solicitud es justo antes de un lanzamiento (estamos muy ocupados en ese momento). Para enviar el formulario, ll&eacute;nelo y env&iacute;elo tanto a la lista de <b>bacula-users</b> como de <b>bacula-devel</b>, que all&iacute; ser&aacute;a discutido abiertamente.</p>

<p>Una vez que se ha discutido adecuadamente las nuevas caracter&iacute;sticas solicitadas, el L&iacute;der del Proyecto Bacula (Kern) podr&aacute; aprobarlas, rechazarlas o, posiblemente, solicitar alguna modificaci&oacute;n. Si planea implementar una funcionalidad en particular o donar fondos para que se la implemente, es importante tener esto en cuenta. De otra forma, aunque se haya aprobado la implementaci&oacute;n de la funcionalidad, puede que pase un tiempo largo hasta que alguien la implemente.</p>

<p>Una vez que la nueva funcionalidad se aprueba, la a&ntilde;adiremos al archivo de proyectos, que contiene un listado de todas las solicitudes abiertas. Este archivo se actualiza de vez en cuando.</p>

<p>La lista actual de proyectos (aunque quiz&aacute;s algo desactualizada) se puede encontrar tambi&eacute;n en el sitio web, cliqueando en Proyectos, en el men&uacute; de la izquierda.</p>

<h3>Formulario para solicitar una caracter&iacute;stica o funcionalidad nueva (Feature Request Form)</h3>
<pre>
Item n:   Breve descripci&oacute;n en una linea ...
  Origin: Nombre o email del solicitante.
  Date:   Fecha de env&iacute;o (ej.: 28 de octubre de 2005)
  Status:

  What:   Explicaci&oacute;n m&aacute;s detallada ...

  Why:    Por qu&eacute; es importante ...

  Notes:  Comentarios adicionales ...

</pre>

<h3>Formulario de ejemplo</h3>
<p>Nota: Lo m&aacute;s probable es que el formulario tenga que estar escrito en ingl&eacute;s, as&iacute; que dejo el texto original.</p>
<pre>
Item 1:   Implementar la migraci&oacute;n de un job, que permita mover 
          los datos de un dispositivo a otro.
  Date:   28 de octubre de 2005
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
