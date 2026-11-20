<!DOCTYPE html>
<html lang="en-US">
<head>
<meta name="generator" content=
"HTML Tidy for HTML5 for Linux version 5.9.8">
<meta charset="UTF-8">
<title>Control remoto Web  pianod</title>
<meta name="viewport" content="width=device-width">
<meta name="application-name" content="pianod">
<link href="client.css" rel="stylesheet" type="text/css">
<link rel="shortcut icon" href="pianod-icon.gif">
<link rel="apple-touch-icon-precomposed" href="pianod-button.gif">
<script src=
"//ajax.googleapis.com/ajax/libs/jquery/2.2.0/jquery.min.js"></script>
<script src="translate.js"></script>
<script src="ratings.js"></script>
</head>
<body>
<noscript>
<h1>Control remoto Web  pianod</h1>
sentimos, se requiere Javascript para utilizar esta página.</noscript>
<div id="switcher" style="display:none;"><span class=
"trackcontrols"><a class="playpause privuser" onclick=
"client.execute ('PAUSE TOGGLE');" title=
"Pausar o reanudar la reproducción.">▶</a></span> <select id=
"sourcepicker" onchange="client.sourceview.selectSource();">
<option value="1">encargado de los media</option>
</select> <a class="handle" onclick=
"client.toggleMenu();">≣ Elija la visión</a>
<div class="menu"><a onclick=
"client.show ('source')">Fuentes</a> <a onclick=
"client.show ('track')">Track</a> <a onclick=
"client.show ('queue')">Queue</a> <a onclick=
"client.show ('search')">Buscar</a> <a onclick=
"client.show ('playlist')">Listas</a> <a onclick=
"client.show ('seed')">Semillas</a> <a onclick=
"client.show ('activity')">Actividad</a> <a onclick=
"client.show ('user')">Usuarios</a> <a onclick=
"client.execute ('QUIT');">Salir</a></div>
</div>
<div id="diagnostics" class="popup">
<div><a class="close button" onclick=
"$('#diagnostics').hide();">✖</a> <span id="diagmain">&nbsp;</span>
<div id="diagdetails">&nbsp;</div>
</div>
</div>
<div id="views">
<div id="loginview" style="display:none;">
<h1>Control remoto Web  pianod</h1>
<p>Derechos de Autor © 2011-2021 Devious Fish<br>
Todos los derechos reservados.<br>
Publicado bajo la licencia MIT.</p>
<div id="serverinput"><label for=
"server">servidor</label> <input id="server" type="url"
maxlength="80" required="required" value=
"house.perette.barella.org:4446"> <input id="secureserver" type=
"checkbox" onchange="client.loginview.toggleSecurity()">
<label for="secureserver">Conexión segura</label></div>
<label for="username">Nombre de usuario</label> <input id=
"username" type="text" maxlength="20"> <label for=
"password">contraseña</label> <input id="password" type=
"password" maxlength="20"> <input id="storepassword" type=
"checkbox"> <label for=
"storepassword">Contraseña  tienda</label>
<p><a id="loginbutton" class="button" onclick=
"client.loginview.connect(false);">Entrar</a>
<a class="button" onclick=
"client.loginview.connect(true);">Invitado</a></p>
<p><a href="console.html">Consola</a> • <a href=
"viewer.html">Visor</a></p>
<p class="note">Las conexiones seguras requieren certificados y claves TLS crearse.</p>
</div>
<div id="sourceview" style="display:none;">
<h2>Fuentes </h2>
<div class="accordion">
<h3 class="privservice">Añadir una nueva fuente</h3>
<div id="addsourceparameters"><label for=
"addsourcetype">Tipo de fuente:</label> <select id=
"addsourcetype" onchange="client.sourceview.selectAddFields();">
<option value="">Seleccione el tipo de ...</option>
</select> <!-- Network media parameters -->
 <label for="addsourceuser">Servicio nombre de usuario</label> <input id=
"addsourceuser" type="text" placeholder=
"Nombre de usuario"> <label for=
"addsourcepassword">Contraseña Servicio</label> <input id=
"addsourcepassword" type="password" placeholder=
"Contraseña"> 
<!-- Local media parameters -->
 <label for="addsourcepath">Directorio para los medios</label> <input id=
"addsourcepath" type="text" placeholder=
"Carpeta de música arrastre aquí"> <!-- Generic parameters -->
<div><select id="addsourcepandoraplus">
<option value="">Select account type...</option>
<option value="automatic">Automatic</option>
<option value="standard">Standard</option>
<option value="plus">Pandora Plus</option>
<option value="premium">Pandora Premium</option>
</select></div>
<div>
<div><input id="addsourcechannelize" type="checkbox"> <label for=
"addsourcechannelize">Include left &amp; right tones</label></div>
<input id="addsourceratingbias_enable" type="checkbox" onchange=
"client.sourceview.selectAddFields();"> <label for=
"addsourceratingbias_enable">Nivel diagonal del grado</label>
<input id="addsourceratingbias" type="range" min="1" max="100"
step="1" title="Controle cuánto efectúan los grados selecciones de la canción.  En la izquierda, ningunos; en la derecha, cada estrella predispone probabilidad de la selección por alrededor 3x (canciones sin calificar = 3 estrellas)."></div>
<div><input id="addsourcerecentbias_enable" type="checkbox"
onchange="client.sourceview.selectAddFields();"> <label for=
"addsourcerecentbias_enable">Nivel diagonal recientemente jugado</label>
<input id="addsourcerecentbias" type="range" min="1" max="100"
step="1" title="Controle cuánto juego reciente efectúa selecciones de la canción.  En la izquierda, ningunos; en la derecha, la probabilidad de la selección es áspero linear predispuesto con tiempo desde el juego pasado."></div>
<div><select id="addsourcecachesize">
<option value="">Default cache size</option>
<option value="800" title="Remember 800-1000 items">
Tiny</option>
<option value="2000" title="Remember 2,000-2,400 items">
Small</option>
<option value="4000" title="Remember 4,000-4,800 items">
Medium</option>
<option value="10000" title="Remember 10,000-12,000 items">
Large</option>
<option value="50000" title="Remember 50,000-60,000 items">
Ludicrous</option>
</select></div>
<div><select id="addsourcerescan" onchange=
"client.sourceview.selectAddFields();">
<option value="">Intervalo de la pre-exploración del valor por defecto</option>
<option value="never" title="Nunca pre-explore para los nuevos media">
Nunca</option>
<option value="once" title="Pre-exploración para los media apenas esto una vez">
Una vez que</option>
<option value="always" title="Pre-exploración siempre que se comience la fuente">
Siempre</option>
<option value="periodically" title=
"Pre-exploración para los media diarios">
Fuente del la del comience del SE del que del siempre de Pre-exploración</option>
</select></div>
<div><select id="addsourceaccess" onchange=
"client.sourceview.selectAddFields();">
<option value="">Distribución del valor por defecto</option>
<option value="disowned" title="no puede ser revisada por cualquier persona.">
sin dueño</option>
<option value="private" title="Otros usuarios no pueden endeudarse fuente">
privada</option>
<option value="shared" title="Otros pueden pedir prestado, pero no copiar o revisar fuente">
Compartido</option>
<option value="published" title="Otros pueden pedir prestado y copiar pero no revisar fuente">
Publicado</option>
<option value="public" title="otros pueden prestar, copiar y modificar la fuente">
Pública</option>
</select></div>
<div><select id="addsourcesongproxy" onchange=
"client.sourceview.selectAddFields();">
<option value="">Seleccione el modo de substitución de la canción…</option>
<option value="none" title="Esta fuente no está implicada en la substitución de los media.">
Ningunas substituciones</option>
<option value="donor" title="Las fuentes receptoras pueden substituir canciones que corresponden con de esta fuente.">
Proporciona a substituciones</option>
<option value="recipient" title=
"Esta fuente puede substituir media con los emparejamientos de las fuentes dispensadoras de aceite.">
Valida substituciones</option>
</select></div>
<div><select id="addsourcepersistence" onchange=
"client.sourceview.selectAddFields();">
<option value="">No recuerde la fuente</option>
<option value="remember" title=
"Recuerde y permita la reutilización manual.">
Ningunos cambios del ninguna del recuerde</option>
<option value="restore" title="Recuerde y restablezca automáticamente en lanzamiento.">
Restore</option>
</select></div>
<div><label for="addsourcename">Nombre de la fuente</label>
<input id="addsourcename" type="text" placeholder=
"Blanco por incumplimiento"></div>
<ul class="actions">
<li><a class="button" onclick=
"client.sourceview.addSource ();">Añadir fuente</a></li>
</ul>
</div>
<h3 id="borrowsource" class="privservice">Uso fuente existente</h3>
<div id="borrowsourceparameters"><select id="borrowlist" onchange=
"client.sourceview.selectBorrowFields();">
<option>Seleccione la fuente para permitir ...</option>
</select>
<div><select id="borrowsourcepandoraplus">
<option value="">Select account type...</option>
<option value="automatic">Automatic</option>
<option value="standard">Standard</option>
<option value="plus">Pandora Plus</option>
<option value="premium">Pandora Premium</option>
</select></div>
<div>
<div><input id="borrowsourcechannelize" type="checkbox">
<label for="borrowsourcechannelize">Include left &amp; right tones</label></div>
<input id="borrowsourceratingbias_enable" type="checkbox" onchange=
"client.sourceview.selectBorrowFields();"> <label for=
"borrowsourceratingbias_enable">Nivel diagonal del grado</label>
<input id="borrowsourceratingbias" type="range" min="1" max="100"
step="1" title="Controle cuánto efectúan los grados selecciones de la canción.  En la izquierda, ningunos; en la derecha, cada estrella predispone probabilidad de la selección por alrededor 3x (canciones sin calificar = 3 estrellas)."></div>
<div><input id="borrowsourcerecentbias_enable" type="checkbox"
onchange="client.sourceview.selectBorrowFields();"> <label for=
"borrowsourcerecentbias_enable">Nivel diagonal recientemente jugado</label>
<input id="borrowsourcerecentbias" type="range" min="1" max="100"
step="1" title="Controle cuánto juego reciente efectúa selecciones de la canción.  En la izquierda, ningunos; en la derecha, la probabilidad de la selección es áspero linear predispuesto con tiempo desde el juego pasado."></div>
<div><select id="borrowsourcecachesize">
<option value="">Retain current cache size</option>
<option value="800" title="Remember 800-1000 items">
Tiny</option>
<option value="2000" title="Remember 2,000-2,400 items">
Small</option>
<option value="4000" title="Remember 4,000-4,800 items">
Medium</option>
<option value="10000" title="Remember 10,000-12,000 items">
Large</option>
<option value="50000" title="Remember 50,000-60,000 items">
Ludicrous</option>
</select></div>
<div><select id="borrowsourcerescan" onchange=
"client.sourceview.selectBorrowFields();">
<option value="">Conserve el intervalo actual de la pre-exploración</option>
<option value="never" title="Nunca pre-explore para los nuevos media">
Nunca</option>
<option value="once" title="Pre-exploración para los media apenas esto una vez">
Una vez que</option>
<option value="always" title="Pre-exploración siempre que se comience la fuente">
Siempre</option>
<option value="periodically" title=
"Pre-exploración para los media diarios">
Fuente del la del comience del SE del que del siempre de Pre-exploración</option>
</select></div>
<div><select id="borrowsourceaccess" onchange=
"client.sourceview.selectBorrowFields();">
<option value="">Conserve el comportamiento de distribución actual</option>
<option value="disowned" title="no puede ser revisada por cualquier persona.">
sin dueño</option>
<option value="private" title="Otros usuarios no pueden endeudarse fuente">
privada</option>
<option value="shared" title="Otros pueden pedir prestado, pero no copiar o revisar fuente">
Compartido</option>
<option value="published" title="Otros pueden pedir prestado y copiar pero no revisar fuente">
Publicado</option>
<option value="public" title="otros pueden prestar, copiar y modificar la fuente">
Pública</option>
</select></div>
<div><select id="borrowsourcesongproxy" onchange=
"client.sourceview.selectBorrowFields();">
<option value="">Utilice el modo de substitución actual de la canción</option>
<option value="none" title="Esta fuente no está implicada en la substitución de los media.">
Ningunas substituciones</option>
<option value="donor" title="Las fuentes receptoras pueden substituir canciones que corresponden con de esta fuente.">
Proporciona a substituciones</option>
<option value="recipient" title=
"Esta fuente puede substituir media con los emparejamientos de las fuentes dispensadoras de aceite.">
Valida substituciones</option>
</select></div>
<div><select id="borrowsourcepersistence" onchange=
"client.sourceview.selectBorrowFields();">
<option value="">No recuerde ninguna cambios</option>
<option value="remember" title=
"Recuerde y permita la reutilización manual.">
Ningunos cambios del ninguna del recuerde</option>
<option value="restore" title="Recuerde y restablezca automáticamente en lanzamiento.">
Restore</option>
</select></div>
<ul class="actions">
<li><a class="button" onclick=
"client.sourceview.borrowSource();">Uso fuente</a></li>
</ul>
</div>
<h3 id="forgetsource" class="privlistener">
Forget source</h3>
<div><select id="sourceforgetlist">
<option>Seleccione la fuente para permitir ...</option>
</select>
<ul class="actions">
<li><a class="button" onclick=
"client.sourceview.forgetSource();">Forget source</a></li>
</ul>
</div>
<h3 class="privservice">Quitar fuente</h3>
<div>
<ul class="actions">
<li><a class="button" onclick=
"client.sourceview.removeSource();">Eliminar fuente de corriente</a></li>
</ul>
</div>
</div>
</div>
<div id="trackview" style="display:none;">
<div id="trackviewtitle"><span class="trackcontrols"><a class=
"playpause button privuser" onclick=
"client.execute ('PAUSE TOGGLE');" title=
"Pausar o reanudar la reproducción.">▶</a> <a class="button" onclick=
"client.trackview.showActionPopup();" title=
"Muestre más acciones…">⚙</a></span> <a id="nowplaying"
onclick="client.trackview.goToCurrentTrack();" class="button"
title="Mostrar canción que se está reproduciendo.">Now Playing</a> <select id=
"playlistname" onchange="client.trackview.selectPlaylist();">
<option>Seleccionar lista de reproducción ...</option>
<optgroup label="listas especiales">
<option value="stop">Detener</option>
<option value="request">Sólo solicitudes</option>
<option value="mix">Mix listas de reproducción seleccionado</option>
<option value="everything">Mezcla todo</option>
<option value="auto">Las listas de reproducción Autotune</option>
</optgroup>
<optgroup id="playlists" label="Las listas de reproducción individuales">
<option>&nbsp;</option>
</optgroup>
</select></div>
<div id="addplaylistfromtrack" class="popup">
<div><a class="close button" onclick=
"$('#addplaylistfromtrack').hide();">✖</a>
<p>Añadir semilla de la canción</p>
<label for="addfromtracktarget">Nombre lista de reproducción:</label>
<select id="addfromtracktarget" onchange=
"client.trackview.selectAddTarget();">
<option value="">Crear nueva lista de reproducción</option>
</select>
<div id="addfromtracknewplaylist"><label for=
"addfromtracksource">Crear lista de reproducción de la fuente:</label>
<select id="addfromtracksource">
<option>&nbsp;</option>
</select> <input id="addfromtrackplaylist" type="text" placeholder=
"Nuevo nombre de la lista"></div>
<ul class="actions">
<li><a id="doaddseedfromsong" class="button" onclick=
"client.trackview.addFromTrack('song');">-</a></li>
<li><a id="doaddseedfromalbum" class="button" onclick=
"client.trackview.addFromTrack('album');">-</a></li>
<li><a id="doaddseedfromartist" class="button" onclick=
"client.trackview.addFromTrack('artist');">-</a></li>
</ul>
</div>
</div>
<div id="trackactions" class="popup">
<div><a class="close button" onclick=
"$('#trackactions').hide();">✖</a>
<ul class="actions">
<li><a id="skiprecuetrack" title="Saltar la canción actual" class="button"
onclick="client.trackview.skipOrRecue();">Canción del salto</a></li>
<li><a class="button privdeejay" title="Cancelar todas las solicitudes pendientes."
onclick=
"client.trackview.clearQueue();">Peticiones claras</a></li>
</ul>
<label for=
"queuerandomize">Haga las selecciones al azar cerca</label>
<select id="queuerandomize" class="privuser" onchange=
"client.trackview.selectQueueRandomize();">
<option value="song" selected>Canciones individuales clasificadas</option>
<option value="album">Elegir un álbum entero</option>
<option value="artist">Algunas canciones de un solo artista</option>
<option value="playlist">Canciones clasificadas a partir de una lista de temas</option>
<option value="random">Un método aleatoriamente elegido</option>
</select>
<div id="roomselector"><label for=
"rooms">Elija el sitio:</label> <select id="rooms"
onchange="client.trackview.selectRoom();">
<option value="pianod" selected>Loading room list...</option>
</select></div>
<label for="crossfadeduration">Duración de Crossfade
<span><span id=
"crossfadeseconds">&nbsp;</span>&nbsp;segundos</span></label>
<div class="rangewrap privuser"><input id="crossfadeduration"
class="privuser" type="range" min="0" max="15" step="0.1" title=
"Crossfade duration" onchange=
"client.trackview.setCrossfadeDuration ();"></div>
<label for="crossfadelevel">Nivel de Crossfade <span><span id=
"crossfadedecibels">testvalue&nbsp;</span>dB</span></label>
<div class="rangewrap privuser"><input id="crossfadelevel" class=
"privuser" type="range" min="0" max="50" step="0.1" title=
"Crossfade level" onchange=
"client.trackview.setCrossfadeLevel ();"></div>
</div>
</div>
<div id="trackinfo">
<div id="albumcover"><img src="no-art.jpeg" id="albumart" alt=''
ondblclick="client.trackview.showArt ();"> <a id="why" class=
"button privowner" onclick="client.trackview.explainSong ();"
title="Explique por qué se jugó esta canción.">?</a>
<div class="pager"><a id="previoustrack" onclick=
"client.trackview.goToPreviousTrack();" class="button previous"
title="Mostrar pista antes">&nbsp;←&nbsp;</a> <a id=
"nexttrack" onclick="client.trackview.goToNextTrack();" class=
"button next" title=
"Mostrar siguiente pista">&nbsp;→&nbsp;</a></div>
<div id="networkstatus" class="statusoverlay">
<span>〈•••〉</span><br>
Reproducción estancó<br>
Compruebe la conexión de red </div>
<div id="pausestatus" class="statusoverlay" onclick=
"client.execute('RESUME');"><span>|| Pausa</span><br>
&nbsp;</div>
<div id="stoppedstatus" class="statusoverlay" onclick=
"client.execute('PLAY');"><span>█ Detenido</span><br>
&nbsp;</div>
<div id="norequests" class="statusoverlay">
<span>Ningunas peticiones</span><br>
Solicitar un poco de música.</div>
<div id="needplaylists" class="statusoverlay" onclick=
"viewmanager.show ('source');"><span>No hay listas de reproducción</span><br>
Favor agregar algunas fuentes.</div>
<div id="needmixselections" class="statusoverlay" onclick=
"viewmanager.show ('playlist');"><span>Mix está vacía</span><br>
Elija listas para mezclar.</div>
<div id="go" class="statusoverlay" onclick=
"client.execute ('PLAY');"><span>▶&nbsp;Juega</span><br>
&nbsp;</div>
<div id="controller">
<div><span id="timepoint" class="time">&nbsp;</span> <span id=
"ratings"><a id="overplayed" class="button privowner" onclick=
"client.trackview.rateOverplayed();" title=
"No juegues esta canción durante un mes.">♻</a> <span id=
"trackrating">&nbsp;</span></span> <span id="duration" class=
"time">&nbsp;</span></div>
<div id="statusbar">
<div id="progressbar" style="width:50%;"><span style=
"display:none;">&nbsp;</span></div>
</div>
</div>
</div>
<div id="trackdetails">
<div id="trackname"><a id="additionalinfo" target="_blank" href=
"http://deviousfish.com/pianod"><span class=
"value">&nbsp;</span></a> <a class="button seed privowner" onclick=
"client.trackview.toggleSeed ('song');" title=
"Toggle semilla canción.">種</a></div>
<div id="artistname"><a id="artistinfo" onclick=
"client.trackview.moreInfo('artist');"><span class=
"value">&nbsp;</span></a> <a class="button seed privowner" onclick=
"client.trackview.toggleSeed ('artist');" title=
"Toggle artista semilla.">種</a></div>
<div id="albumname"><a id="albuminfo" onclick=
"client.trackview.moreInfo('album');"><span class=
"value">&nbsp;</span></a> <a class="button seed privowner" onclick=
"client.trackview.toggleSeed ('album');" title=
"Toggle semilla álbum.">種</a></div>
<div id="songplaylist"><a class="privowner" onclick=
"client.trackview.showSeeds ();"><span class=
"value">&nbsp;</span></a> <a class="button seed privowner" onclick=
"$('#addplaylistfromtrack').show();" title=
"Añadir semilla o crear lista de reproducción de la canción, álbum o artista.">+</a></div>
</div>
</div>
<input id="volume" class="privuser" type="range" min="-40" max="0"
step="1" title="Volume" onchange=
"client.trackview.setVolume ();"></div>
<div id="queueview" style="display:none;">
<h2>Coleta</h2>
<div>
<table id="queue" class="scrolling">
<thead>
<tr>
<th style="display:none;">&nbsp;</th>
</tr>
</thead>
<tbody>
<tr>
<td>&nbsp;</td>
</tr>
</tbody>
</table>
</div>
</div>
<div id="searchview" style="display:none;">
<h2>Búsqueda de música</h2>
<label for="searchtype">Buscar</label> <select id=
"searchtype">
<option value="any">Cualquier</option>
<option value="song">Canción</option>
<option value="artist">Artista</option>
<option value="album">Álbum</option>
<option value="playlist">Género</option>
<option value="request">Sólo canciones solicitable</option>
</select> <label for="searchmethod">Método</label>
<select id="searchmethod" onchange=
"client.searchview.selectMethod();">
<option value="like">Palabras partido</option>
<option value="where">Frase Partido</option>
<option value="expr">Expresión </option>
</select> <label for="searchfor">Matching</label>
<input id="searchfor" type="text" placeholder=
"Texto de búsqueda"> <a class="button" id=
"searchbutton" onclick=
"client.searchview.search();">Buscar</a> <a class=
"button privdeejay" id="searchrequest" onclick=
"client.searchview.request();">Solicitud todo</a>
<a class="button privuser" id="searchplaytransient" onclick=
"client.searchview.playTransient();">Mix</a>
<a class="button" id="searchcreate" onclick=
"client.searchview.showAllResultsMenu();">Seed</a>
<div id="searchaction" class="popup">
<div><a class="close button" onclick=
"$('#searchaction').hide();">✖</a> <span id=
"addfromsearchname">&nbsp;</span> (<span id=
"addfromsearchtype">&nbsp;</span>)<br>
<span id="searchinfosource">&nbsp;</span> (<span id=
"searchinfoname">&nbsp;</span>)
<div class="actiongrouping">
<p>Add seed from search result</p>
<label for="addfromsearchtarget">Nombre lista de reproducción:</label>
<select id="addfromsearchtarget">
<option value="">Seleccionar lista de reproducción ...</option>
</select>
<ul class="actions">
<li><a class="button" onclick=
"client.searchview.addSeed ();">Añadir semilla</a></li>
</ul>
</div>
<div class="actiongrouping">
<p>Create playlist from search result</p>
<label for=
"createfromsearchsource">Crear lista de reproducción de la fuente:</label>
<select id="createfromsearchsource">
<option>&nbsp;</option>
</select> <input id="createfromsearchplaylist" type="text"
placeholder="Nuevo nombre de la lista">
<div id="createfromsearchtype"><input id=
"createfromsearchtypestandard" name="createfromsearchtype" type=
"radio" value="standard"> <label for=
"createfromsearchtypestandard">Standard playlist</label>
<input id="createfromsearchtypesmart" name="createfromsearchtype"
type="radio" value="smart"> <label for=
"createfromsearchtypesmart">Smart playlist</label></div>
<ul class="actions">
<li><a id="createplaylist" class="button" onclick=
"client.searchview.createPlaylist() ;">Crear nueva lista de reproducción</a></li>
</ul>
</div>
</div>
</div>
<table id="searchresults" class="scrolling">
<thead>
<tr>
<th style="display:none;">&nbsp;</th>
</tr>
</thead>
<tbody>
<tr>
<td>&nbsp;</td>
</tr>
</tbody>
</table>
</div>
<div id="playlistview" style="display:none;"><span class=
"pagenumber">página &nbsp; <span id="playlistpage">&nbsp;</span>
&nbsp; de &nbsp; <span id="playlistpages">&nbsp;</span>
</span>
<h2>Listas</h2>
<span class="pager"><a class="button" onclick=
"client.playlistview.previous()">↑</a> <a class=
"button" onclick="client.playlistview.next()">↓</a>
<select id="showmode" onchange=
"client.playlistview.changeShownMode()">
<option value="playlist">Seleccionar lista de reproducción ...</option>
<option value="mix">Mix listas de reproducción seleccionado</option>
<option value="everything">Mezcla todo</option>
<option value="auto">Las listas de reproducción Autotune</option>
</select> <a id="selectmode" class="button" onclick=
"client.playlistview.selectMode()" title=
"Seleccione el modo de reproducción y reanudar la reproducción.">▶</a></span>
<ul id="playlistlist" class="columnar privinfluence">
<li>No hay listas de reproducción</li>
</ul>
</div>
<div id="seedview" style="display:none;">
<h2>Las listas de reproducción revisar</h2>
<label for="reviseplaylist">Lista de reproducción</label>
<select id="reviseplaylist" onchange=
"client.seedview.selectPlaylist();">
<option value="">Seleccionar lista de reproducción ...</option>
</select>
<div id="renameplaylist" class="popup">
<div><a class="close button" onclick=
"$('#renameplaylist').hide();">✖</a>
<p>Rename playlist</p>
Source: <span id="renameplaylistsourcetype">&nbsp;</span> /
<span id="renameplaylistsourcename">&nbsp;</span>
<p><label for=
"renameplaylistoldname">Old playlist name:</label> <input id=
"renameplaylistoldname" type="text" disabled> <label for=
"renameplaylistnewname">New playlist name:</label> <input id=
"renameplaylistnewname" type="text"></p>
<ul class="actions">
<li><a class="button" onclick=
"client.seedview.renamePlaylist();">Rename playlist</a></li>
</ul>
</div>
</div>
<div>
<table id="seedsexisting" class="scrolling">
<thead>
<tr>
<th style="display:none;">&nbsp;</th>
</tr>
</thead>
<tbody>
<tr>
<td>&nbsp;</td>
</tr>
</tbody>
</table>
<a class="button" onclick=
"client.seedview.showRenamePlaylist();">Rename playlist</a>
<a class="button" onclick=
"client.seedview.removePlaylist();">Eliminar lista de reproducción en su totalidad</a></div>
</div>
<div id="activityview" style="display:none;">
<h2>Actividad reciente</h2>
<ul id="recentactivity" class="scrolling">
<li>&nbsp;</li>
</ul>
<textarea id="yellmessage" rows="4" placeholder=
"Escriba mensaje"></textarea>
<br>
<a id="yellbutton" class="button" title="Difusión de su mensaje a otros usuarios pianod conectados."
onclick="client.activityview.yell()">Yell se</a></div>
<div id="userview" style="display:none;">
<h2>Mantenimiento del usuario</h2>
<div class="accordion">
<h3>Cambie su contraseña</h3>
<div><label for="oldpassword">Antigua contraseña</label>
<input id="oldpassword" type="password"> <label for=
"newpassword">Nueva contraseña</label> <input id="newpassword"
type="password"> <label for=
"confirmpassword">Confirme la contraseña</label> <input id=
"confirmpassword" type="password">
<div><input id="shadowpassword" type="checkbox" onchange=
"client.userview.changeShadow();"> <label for="shadowpassword"
title=
"En vez de cambiar una palabra de paso, cambie a su palabra de paso del sistema para la autentificación del pianod.">Adopte la palabra de paso del sistema</label></div>
<a class="button" onclick=
"client.userview.changePassword();">Cambiarlo</a><br>

Contraseñas pueden ser enviados al servidor en forma no cifrada. No utilice una contraseña importante, al igual que el de su cuenta bancaria.</div>
<h3 class="privadmin">Crear un usuario</h3>
<div><label for="createusername">Nombre de usuario</label>
<input id="createusername" type="text"> <label for=
"createusertype">Nivel Privilege</label> <select id=
"createusertype">
<option value="listener">Listener</option>
<option value="user">Estándar</option>
<option value="admin">Administrador</option>
</select> <label for=
"createpassword">contraseña</label> <input id=
"createpassword" type="password"> <label for=
"createconfirmpassword">Confirme la contraseña</label> <input id=
"createconfirmpassword" type="password"> <a class="button" onclick=
"client.userview.createUser()">Crear usuario</a><br>
Contraseñas pueden ser enviados al servidor en forma no cifrada. No utilice una contraseña importante, al igual que el de su cuenta bancaria.</div>
<h3 id="alter_user" class="privadmin">Alter un usuario</h3>
<div><label for="alterusername">Nombre de usuario</label>
<select id="alterusername" onchange=
"client.userview.selectAlterUser();">
<option value=''>Seleccionar usuario ...</option>
</select> <label for="alterusertype">Rango & amp; Privilegios</label>
<select id="alterusertype" onchange=
"client.userview.changeType();">
<option value="disabled">Discapacitados</option>
<option value="listener">Listener</option>
<option value="user">Estándar</option>
<option value="admin">Administrador</option>
</select>
<ul class="privilegelist">
<li><input id="privilegedeejay" type="checkbox" onchange=
"client.userview.changePrivilege ('deejay')"> <label for=
"privilegedeejay">Request music and manage queue</label></li>
<li><input id="privilegeservice" type="checkbox" onchange=
"client.userview.changePrivilege ('service')"> <label for=
"privilegeservice">Agregar, quitar o pedir prestado fuentes</label></li>
<li><input id="privilegeinfluence" type="checkbox" onchange=
"client.userview.changePrivilege ('influence')"> <label for=
"privilegeinfluence">Las listas de reproducción autotuned Influencia</label></li>
</ul>
<a class="button" onclick=
"client.userview.resetPassword()">Restablecer contraseña</a>
<a class="button" onclick=
"client.userview.deleteUser()">Borrar usuario</a>
<p class="note">Tenga cuidado de no eliminar el rango de administrador de todos los usuarios, lo que le dejará sin un administrador.</p>
</div>
<h3 id="kick_user" class="privadmin">Usuarios desahucie</h3>
<div>
<ul id="onlineusers" class="actions">
<li>user list goes here</li>
</ul>
</div>
</div>
</div>
</div>
<div id="status"><a id="showerrors" onclick=
"$('#errors').show();">ℹ</a> <span class=
"value">&nbsp;</span></div>
<script type="module">
      window.assert = function() {};
      
        import { Client } from './client.lib.jsm';
      

      /* Set up global variables */
      var translator = new Translate();
      window.translate = translator.translate;

      /* Set global execute function */
      window.client = new Client ();
</script>
</body>
</html>
