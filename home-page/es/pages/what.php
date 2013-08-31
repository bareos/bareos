<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
                &iquest;Qu&eacute; es Bacula?
        </td>
</tr>
<tr>
        <td class="content">

<b>Bacula</b> es un conjunto de programas que permite el manejo de backups, recuperaci&oacute;n y verificaci&oacute;n de datos a trav&eacute;s de una red de computadoras heterogenea. En t&eacute;rminos t&eacute;cnicos, se trata de un sistema cliente/servidor de backups en red. Bacula es relativamente f&aacute;cil de usar y eficiente, a la vez que ofrece una administraci&oacute;n avanzada con la funcionalidad necesaria para recuperar f&aacute;cilmente archivos perdidos o da&ntilde;ados. Gracias a su dise&ntilde;o Bacula escala bien, desde entornos peque&ntilde;os a sistemas con cientos de computadoras en un red grande.

<h3>Qui&eacute;n necesita Bacula?</h3>
Si en la actualidad usa un programa como <b>tar</b>, <b>dump</b>, o<b>bru</b> para hacer backups, y quisiera una soluci&oacute;n basada en red, m&aacute;s flexible, o con un servicios de cat&aacute;logo, Bacula seguro le dar&aacute; esta funcionalidad adicional que est&aacute; buscando.

Sin embargo, si es nuevo en Unix o no tiene experiencia con sistemas de backup sofisticados, no recomendamos que use Bacula, porque le puede resultar muy dif&iacute;cil su configuraci&oacute;n y uso comparado con <b>tar</b> o <b>dump</b>.

<p>Si usa <b>Amanda</b> y quisiera un programa de backup capaz de escribir a varios vol&uacute;menes (es decir, no estar limitado al tama&ntilde;o de la cinta), Bacula seguro puede servirle. Adem&aacute;s, varios de nuestros usuarios dicen que Bacula es m&aacute;s f&aacute;cil de usar que otros programas equivalentes.</p>

<p>Si se encuentra usando un paquete de backup comercial como Legato Networker, ARCserveIT, Arkeia o PerfectBackup+, puede interesarle Bacula, que da las mismas funcionalidades bajo una licencia libre (GNU Version 2).

<h3>Componentes o servicios de Bacula</h3>
Bacula est&aacute; conformado por estas cinco componenes o servicios:
<p style="text-align: center; font-size: small">
        <img src="images/manual/bacula-applications.png" alt="" width="576" height="734"><br>
        gracias a Aristedes Maniatis por este gr&aacute;fico y el de abajo.
</p>
<p>
<ul>
<li><a name="DirDef"></a>
    <b>Bacula Director</b> es el programa que supervisa las operaciones de backup, restauraci&oacute;n, verificaci&oacute;n y almacenamiento. El administrador utiliza Bacula Director para programar backups y restaurar archivos. Para m&aacute;s informaci&oacute;n vea <a href="rel-manual/director.html">Director Services Daemon Design Document (En ingl&eacute;s)</a>. El Director corre como daemon o servicio (en background).
</li>
<li><a name="UADef"></a>
    <b>Bacula Console</b> es el programa que le permite al administrador u operador comunicarse con el <b>Bacula Director</b>. Al momento hay tres versiones de la consola. La primera es el comando bconsole, una consola por linea de comandos. La segunda, una consola gr&aacute;fica basada en las librer&iacute;as QT que tiene casi toda la funcionalidad de la consola textual. La tercera consola est&aacute; basada en wxWidgets, que tambi&eacute;n brinda casi todas las funciones de la consola textual, as&iacute; como autocompletado de comandos con TAB y mensajes de ayuda instant&aacute;neos sobre el comando que se escribe. Para m&aacute;s informaci&oacute;n vea <a href="rel-manual/console.html">Bacula Console Design Document (En ingl&eacute;s)</a>.
</li>
<li><a name="FDDef"></a>
    <b>Bacula File</b> es el programa que se instala en la m&aacute;quina que estar&aacute; resguardada por Bacula, y es espec&iacute;fico al sistema operativo que corra esa m&aacute;quina. Es responsable de dar los atributos de los archivos y datos pedidos por el Director. Tambi&eacute;n es se encarga de recibir los datos de una restauraci&oacute;n y los atributos de esos datos. Para m&aacute;s informaci&oacute;n vea <a href="rel-manual/file.html">File Services Daemon Design Document (En inglk&eacute;s)</a>. 

    Este programa corre como un daemon en la m&aacute;quina a resguardar, y en la documentaci&oacute;n tambi&eacute;n recibe el nombre de Cliente (Client en ingl&eacute;s). Adem&aacute;s de las versiones para Unix/Linux, hay una versi&oacute;n para Windows (Generalmente distribu&iacute;da en un binario). Las versiones para Windows corren en NT, 2000, XP, 2003, y posiblemente en Me y 98.
</li>
<li><a name="SDDef"></a>
    <b>Bacula Storage</b> es el programa que realiza el almacenamiento y restauraci&oacute;n de los archivos y metadatos en los medios f&iacute;sicos. En otras palabras, es quien lee y escribe en las cintas (u otro medio de almacenamiento). Para m&aacute;s informaci&oacute;n vea <a href="rel-manual/storage.html">Storage Services Daemon Design Document (En ingl&eacute;s)</a>. Este programa corre como daemon en la m&aacute;quina que tiene conectado el dispositivo de backup (Generalmente una cinta). 
</li>
<li><a name="DBDefinition"></a>
    El <b>Catalog</b> mantiene los &iacute;ndices de archivos y la informaci&oacute;n sobre los vol&uacute;menes para todos los archivos en el backup. El cat&aacute;logo permite al administrador u operador encontrar y restaurar r&aacute;pidamente cualquier archivo. El cat&aacute;logo hace &uacute;nico a Bacula en comparaci&oacute;n a herramientas como tar y bru, porque el cat&aacute;logo mantiene registro de todos los vol&uacute;menes usados, todos los Jobs (trabajos) ejecutados, y todos los archivos guardados, permitiendo as&iacute; una restauraci&oacute;n y manejo de vol&uacute;menes m&aacute;s eficientes.

Al momento Bacula soporta tres bases de datos diferentes: MySQL, PostgreSQL, y SQLite, una de las cuales debe elegirse al momento de compilar <b>Bacula</b>.
</li>
i
<li><a name="MonDef"></a>
    <b>Bacula Monitor</b> es el programa que le permite al administrador u operador ver el estado actual de los <b>Bacula Directors</b>, <b>Bacula File Daemons</b> y <b>Bacula Storage Daemons</b>. Al momento hay s&oacute;lo una versi&oacute;n en GTK+, que funciona con Gnome y KDE (y cualquier otro manejador de ventanas que soporte el est&aacute;ndar de FreeDesktop.org para los "system tray").
</li>
</ul>

Para correr Bacula con &eacute;xito se deben configurar y correr: el Director, el File daemon, el Storage daemon, y MySQL, PostgreSQL o SQLite.


<h3>Lo que Bacula NO es</h3>

<b>Bacula</b> es un sistema de backup, restouraci&oacute;n y verificaci&oacute;n. No es un sistema completo para la recuperaci&oacute;n de desastres, aunque puede ser una parte integrante de uno si planea con cuidado y sigue las instrucciones en el cap&iacute;tulo <a href="rel-manual/rescue.html"> Disaster Recovery (En ingl&eacute;s)</a> del manual.
<p>
Con la planifiacaci&oacute;n apropiada, <b>Bacula</b> puede ser una parte central en su sistema de recuperaci&oacute;n de desastres. Por ejemplo, si ha creado un disco para booter de emergencia, un disco de Rescate Bacula para guardar la informaci&oacute;n de la tabla de particiones, y mantener un backup completo, es posible recuperar completamente su sistema.
<p>
Si hautilizado el registro <b>WriteBootstrap</b> en el job o guardado el archivo de bootstrap de otra manera, podr&aacute; usarlo para extraer los archivos necesarios (Sin utilizar el cat&aacute;logo o buscarlos de forma manual).

<h3>Interacci&oacute;n entre los servicios de Bacula</h3>
El siguiente diagrama muestra la interacci&oacute;n t&iacute;pica entre los servicios de Bacula para hacer un backup. Cada parte representa en general un proceso separado (daemon). En general es el Director el que supervisa el flujo de la informaci&oacute;n. Tambi&eacute;n mantiene el cat&aacute;logo.
<p style="text-align: center">
        <img src="images/manual/flow.jpeg" border="0" alt="Interactions between Bacula Services" width="480" height="370">
</p>
        </td>
</tr>
</table>

<? require_once("inc/header.php"); ?>
