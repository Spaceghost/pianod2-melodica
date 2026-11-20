/* pianod web client - internationalization/translation support
   Copr. 2015-2017 Perette Barella/Devious Fish
   All rights reserved.
*/




var Translate = function () {
	function Translate () {
		/* Nothing */
	};

	/* Constants */
	var ERRORS = {


		200: "Éxito",
		203: "Inicio de los datos",
		204: "No hay datos o final de los datos",
		206: "Partido encontró",
		207: "Valor se ha aproximado",
		210: "Comando reconoció pero la acción está pendiente",
		300: "Detalle",
		301: "Proxy Configuración inválida",
		303: "Válida lista de reproducción",
		304: "Elemento no encontrado",
		400: "Comando Mala",
		401: "No autorizado para la acción solicitada",
		402: "Error en la acción",
		403: "Ya existe",
		404: "Artículo solicitado no encontrado",
		405: "Acción no es aplicable al estado actual",
		406: "Inicio de sesión o contraseña no válidos",
		407: "Parámetro válida",
		408: "Playlist personalización fracasó",
		409: "Conflicto encontró",
		410: "Fallo temporal, futuro finalización desconocido",
		411: "Superó Cuota",
		412: "Que estar registrado",
		413: "No compatible",
		414: "Recursos insuficientes",
		415: "Límite ha sido alcanzado",
		416: "Operación sólo es válida en las listas de reproducción primarias",
		417: "Tipo incorrecto para la acción",
		418: "Expresión persistente o el valor necesarios",
		460: "Acción no soportado por fuente",
		461: "El valor no está soportado por fuente",
		462: "Acción no es posible en media manager",
		498: "Hay un error en pianod",
		499: "No implementado",
		500: "Error del servidor interna",
		501: "Nada de jugar",
		502: "Fracaso red",
		503: "Servicio cerrar",
		504: "Fracaso autenticación",
		505: "Recursos insufficent",
		507: "Error comunicarse con Pandora",
		508: "Ejecución de comandos incompleta",
		509: "Permiso denegado",
		510: "Excepción",
		511: "Tiempo de espera Red",
		512: "No se puede abrir la salida de audio",
		'Bogus': 'Bogosity'
	};



	var MESSAGES = {
			"ERROR_LOGIN_FAILED": "Conexión fallada",
	"ERROR_CONNECTION_FAILED": "Conexión fallada.",
	"ERROR_LOGIN_BAD_CREDENTIALS": "Malas credenciales del utilizador.",
	"CHECK_PORT_NUMBER": "Compruebe el número de puerto.",
	"LOGIN_SESSION_STARTING": "Conexión de la apertura a %1$s.",
	"LOGIN_SESSION_ENDED": "Sesión terminada.",
	"STATUS_AUTHENTICATION_OK": "Authentication successful.",
	

		"HEADING_ALBUM": "Álbum",
	
		"HEADING_ARTIST": "Artista",
	
		"HEADING_SONG": "Title",
	
		"HEADING_GENRE": "Género",
	
		"HEADING_TYPE": "Tipo",
	
		"HEADING_SEEDTYPE": "Tipo del germen",
	
		"HEADING_PLAYLIST": "Lista de temas",
	
		"HEADING_YEAR": "Año",
	
		"HEADING_DURATION": "Duración",
	
		"HEADING_ACTION": "Acciones",
	

				"Cap_song": "Canción",
	"Cap_artist": "Artista",
	"Cap_album": "Álbum",
	"Cap_playlist": "Lista de temas",
	

		"lower_song": "canción",
	"lower_artist": "artista",
	"lower_album": "album",
	"lower_playlist": "lista de temas",
	



				"PROMPT_CONFIRM_DELETE": "¿Está usted seguro usted quiere suprimir %1$s?",
	"CONFIRM_DELETE": "%1$s suprimido.",
	"CONFIRM_ADD": "%1$s added.",
	"CONFIRM_ADDED_TO": "%1$s agregado a %2$s.",
	"CONFIRM_CREATE": "%1$s creado.",
	"CONFIRM_RENAME": "%1$s renamed to %2$s.",
	


				"ACTION_TABLE_QUE": "+Coleta",
	"ACTION_TABLE_DEQUE": "-Decoleta",
	"ACTION_TABLE_SKIP": "Salto",
	"ACTION_TABLE_SEED": "+Germen",
	"ACTION_TABLE_UNSEED": "✖Quite el germen",
	



				"CONFIRM_CLEAR_REQUESTS": "Coleta de petición borrada.",
	"SELECT_PLAYLISTS_NONE": "Ningunas listas de temas",
	"ERROR_NO_USABLE_SOURCES": "Ningunas fuentes usables.",
	"CONFIRM_RATE_OVERPLAYED": "Grado de la canción retrocedido temporalmente.",
	"CONFIRM_ADD_SEED_TYPE_TO": "Germen agregado de %1$s a %2$s.",
	"CONFIRM_DEQUEUED": "Requested music cancelled.",
	"INDICATOR_AUTOTUNING": "∫",
	"STATUS_NOTHING_PLAYING": "There is nothing playing right now.",
	"ERROR_QUEUEHISTORY_RANGE": "No further items.",
	



				"INFO_SEARCH_RESULTS_OMITTED": "Algunos resultados fueron omitidos: ",
	"ERROR_CHOOSE_PLAYLIST": "Elija por favor una lista de temas de la blanco.",
	"ERROR_NEED_PLAYLIST_NAME": "Proporcione por favor a un nombre de la lista de temas.",
	"CONFIRM_QUEUED": "Peticiones hechas cola.",
	"ERROR_NEED_SEARCH_QUERY": "Incorpore por favor una interrogación de la búsqueda.",
	"MESSAGE_CREATE_WITH_ALL_RESULTS": "todos buscan resultados",
	"MESSAGE_CREATE_APPLIES_TARGET": "se aplica a la fuente de la blanco",
	



				"ERROR_NO_SOURCES": "Proporcione por favor a algunas fuentes.",
	"ERROR_NEED_SOURCE_SELECTION": "Please choose a source.",
	"ERROR_NEED_SOURCE_TYPE": "Please choose a source type.",
	"CONFIRM_SOURCE_ADDED": "La fuente agregada, puede tardar un momento para estar lista.",
	"CONFIRM_SOURCE_REMOVED": "Fuente quitada.",
	"CONFIRM_SOURCE_SELECTED": "Quitada de Fuente.",
	

		"ADDSOURCE_NULL_PATH": "Especifique por favor el camino/la carpeta para la música.",
	



				"ACTION_SKIP": "Canción del salto",
	
		"TOOLTIP_SKIP": "Saltar la canción actual",
	
		"ACTION_DEQUEUE": "Dequeue",
	
		"TOOLTIP_DEQUEUE": "Cancele una canción próxima.",
	
		"ACTION_RECUE": "Canción de la petición",
	
		"TOOLTIP_RECUE": "Pida esta canción otra vez.",
	

		"PROMPT_CREATE_PLAYLIST_FROM": "Cree la lista de temas de %1$s",
	"PROMPT_ADD_SEED_FROM": "Agregue el germen de %1$s",
	



				"PROMPT_CONFIRM_DELETE_SEED": "¿Está usted seguro usted quiere quitar el germen para %1$s?",
	"ERROR_SELECT_PLAYLIST": "Seleccione por favor una lista de temas.",
	



				"HEADING_LISTENERS_ONLINE": "Módulos de escucha en línea:",
	"ERROR_ACTIVITY_DISABLED": "Los mensajes de la actividad son lisiados.",
	"ERROR_YELL_NO_MESSAGE": "NinguÌn mensaje a gritar.",
	



				"PROMPT_OLD_PASSWORD": "Antigua contraseña",
	"PROMPT_NEW_PASSWORD": "Nueva contraseña",
	"PROMPT_PIANOD_PASSWORD": "palabra de paso del pianod",
	"PROMPT_SYSTEM_PASSWORD": "Palabra de paso del sistema",
	

		"PROMPT_CONFIRM_NO_PASSWORD": "Esto borrará palabra de paso de %1$s así que pueden abrirse una sesión sin una palabra de paso. Esto es inseguro.  ¿Está usted seguro?",
	"CONFIRM_USER_UPDATED": "Usuario %1$s actualizados.",
	"CONFIRM_PASSWORD_CHANGE": "Se ha puesto al día la palabra de paso.",
	"ERROR_CANNOT_GET_USER_LIST": "No puede extraer la lista de utilizador.",
	"ERROR_PASSWORDS_UNMATCHED": "Las nuevas palabras de paso no corresponden con.",
	"ERROR_SELECT_USER": "Seleccione por favor a un utilizador.",
	"LABEL_KICK_VISITORS": "Todos los visitantes",
	"CONFIRM_KICK": "Utilizador %1$s golpeado con el pie.",
	

		'Bogus': 'Bogosity'
	};
	
	/* Private variables */

	/* Private functions */
	if (typeof (assert) == 'undefined') {
		var assert = function () { };
	}

	/* Return a function object, which will see all of
	   the above via the closure. */
	Translate.prototype.translate = function (value) {
		if (typeof (value) == 'string') {
			// Translate a string
			var message = value in MESSAGES ? MESSAGES [value] : value;
			// Perform printf-style positional string substitutions.
			for (var i = 1; i < arguments.length; i++) {
				message = message.replace (RegExp ("%" + i + "\\$s", "g"), arguments [i]);
			}
			return message;
		}
		// Translate a Football comm response.
		assert (arguments.length == 1);
		assert (typeof (value) == 'object');
		assert ('code' in value);
		if (typeof (value) != 'object' || !('code' in value)) {
			return 'Bad translation request';
		}
		if (!(value.code in ERRORS)) {
			return value.text;
		}
		// Replace value up to first colon.
		var pieces = value.text.split (':');
		pieces [0] = ERRORS [value.code];
		return pieces.join (':')
	};
	return Translate;
}();

