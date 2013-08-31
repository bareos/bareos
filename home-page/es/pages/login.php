<? require_once("inc/header.php"); ?>

<?
?>

<div class="login">
<form action="/?page=login" method="post">
	<table cellpadding="5px" style="background: #749aba; color:white; border: 15px #002244 solid; padding: 15px" align="center">
	<tr>
		<td>Usuario:</td>
		<td><input type="text" name="username" size="10" maxsize="10"></td>
	</tr>
	<tr>
		<td>Contrase&ntilde;a: </td>
		<td><input type="password" name="password" size="10" maxsize="10"></td>
	</tr>
	<tr>
		<td style="text-align: right" colspan="2"> <input style="border: 1px dotted white" type="submit" value="Login"> </td>
	</tr>
	</table>
</form>
</div>

<? require_once("inc/footer.php"); ?>
