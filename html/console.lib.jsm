export var Pianod;
(function (Pianod) {
let MSG;
(function (MSG) {
MSG[MSG["PLAYING"] = 1] = "PLAYING";
MSG[MSG["PAUSED"] = 2] = "PAUSED";
MSG[MSG["STALLED"] = 3] = "STALLED";
MSG[MSG["TRACKCOMPLETE"] = 4] = "TRACKCOMPLETE";
MSG[MSG["INTERTRACK"] = 5] = "INTERTRACK";
MSG[MSG["IDLE"] = 6] = "IDLE";
MSG[MSG["STOPPED"] = 7] = "STOPPED";
MSG[MSG["REQUESTS"] = 8] = "REQUESTS";
MSG[MSG["RANDOMPLAY"] = 9] = "RANDOMPLAY";
MSG[MSG["SELECTEDSOURCE"] = 11] = "SELECTEDSOURCE";
MSG[MSG["SELECTEDPLAYLIST"] = 12] = "SELECTEDPLAYLIST";
MSG[MSG["MIX_CHANGED"] = 21] = "MIX_CHANGED";
MSG[MSG["PLAYLISTS_CHANGED"] = 22] = "PLAYLISTS_CHANGED";
MSG[MSG["USERRATINGS_CHANGED"] = 23] = "USERRATINGS_CHANGED";
MSG[MSG["SOURCES_CHANGED"] = 24] = "SOURCES_CHANGED";
MSG[MSG["SONGRATING_CHANGED"] = 25] = "SONGRATING_CHANGED";
MSG[MSG["QUEUE_CHANGED"] = 26] = "QUEUE_CHANGED";
MSG[MSG["YELL"] = 31] = "YELL";
MSG[MSG["ACTIVITY"] = 32] = "ACTIVITY";
MSG[MSG["STATUS"] = 33] = "STATUS";
MSG[MSG["SOURCESTATUS"] = 34] = "SOURCESTATUS";
MSG[MSG["QUEUERANDOMIZE"] = 41] = "QUEUERANDOMIZE";
MSG[MSG["WELCOME"] = 100] = "WELCOME";
MSG[MSG["ID"] = 111] = "ID";
MSG[MSG["ALBUM"] = 112] = "ALBUM";
MSG[MSG["ARTIST"] = 113] = "ARTIST";
MSG[MSG["SONG"] = 114] = "SONG";
MSG[MSG["PLAYLIST"] = 115] = "PLAYLIST";
MSG[MSG["RATING"] = 116] = "RATING";
MSG[MSG["URL"] = 117] = "URL";
MSG[MSG["ART"] = 118] = "ART";
MSG[MSG["GENRE"] = 119] = "GENRE";
MSG[MSG["PLAYLISTRATING"] = 120] = "PLAYLISTRATING";
MSG[MSG["EXPLANATION"] = 121] = "EXPLANATION";
MSG[MSG["OWNER"] = 122] = "OWNER";
MSG[MSG["SOURCE"] = 123] = "SOURCE";
MSG[MSG["NAME"] = 124] = "NAME";
MSG[MSG["YEAR"] = 125] = "YEAR";
MSG[MSG["DURATION"] = 126] = "DURATION";
MSG[MSG["ACTIONS"] = 127] = "ACTIONS";
MSG[MSG["INFO"] = 132] = "INFO";
MSG[MSG["PRIVILEGES"] = 136] = "PRIVILEGES";
MSG[MSG["VOLUME"] = 141] = "VOLUME";
MSG[MSG["HISTORYSIZE"] = 142] = "HISTORYSIZE";
MSG[MSG["AUDIOQUALITY"] = 143] = "AUDIOQUALITY";
MSG[MSG["AUTOTUNE_MODE"] = 144] = "AUTOTUNE_MODE";
MSG[MSG["PAUSE_TIMEOUT"] = 146] = "PAUSE_TIMEOUT";
MSG[MSG["PLAYLIST_TIMEOUT"] = 147] = "PLAYLIST_TIMEOUT";
MSG[MSG["PROXY"] = 161] = "PROXY";
MSG[MSG["CONTROLPROXY"] = 162] = "CONTROLPROXY";
MSG[MSG["RPCHOST"] = 163] = "RPCHOST";
MSG[MSG["RPCTLSPORT"] = 164] = "RPCTLSPORT";
MSG[MSG["PARTNERUSER"] = 165] = "PARTNERUSER";
MSG[MSG["PARTNERPASSWORD"] = 166] = "PARTNERPASSWORD";
MSG[MSG["DEVICE"] = 167] = "DEVICE";
MSG[MSG["ENCRYPTION_PASSWORD"] = 168] = "ENCRYPTION_PASSWORD";
MSG[MSG["DECRYPTION_PASSWORD"] = 169] = "DECRYPTION_PASSWORD";
MSG[MSG["SOURCE_USER"] = 170] = "SOURCE_USER";
MSG[MSG["SOURCE_PASSWORD"] = 171] = "SOURCE_PASSWORD";
MSG[MSG["TLSFINGERPRINT"] = 172] = "TLSFINGERPRINT";
MSG[MSG["OUTPUT_DRIVER"] = 181] = "OUTPUT_DRIVER";
MSG[MSG["OUTPUT_DEVICE"] = 182] = "OUTPUT_DEVICE";
MSG[MSG["OUTPUT_ID"] = 183] = "OUTPUT_ID";
MSG[MSG["OUTPUT_SERVER"] = 184] = "OUTPUT_SERVER";
MSG[MSG["S_OK"] = 200] = "S_OK";
MSG[MSG["DATA_START"] = 203] = "DATA_START";
MSG[MSG["DATA_END"] = 204] = "DATA_END";
MSG[MSG["S_MATCH"] = 206] = "S_MATCH";
MSG[MSG["S_ROUNDING"] = 207] = "S_ROUNDING";
MSG[MSG["S_PENDING"] = 210] = "S_PENDING";
MSG[MSG["EVENT_START"] = 0] = "EVENT_START";
MSG[MSG["EVENT_END"] = 99] = "EVENT_END";
MSG[MSG["INFO_START"] = 100] = "INFO_START";
MSG[MSG["INFO_END"] = 199] = "INFO_END";
MSG[MSG["SUCCESS_START"] = 200] = "SUCCESS_START";
MSG[MSG["SUCCESS_END"] = 299] = "SUCCESS_END";
MSG[MSG["DIAG_START"] = 300] = "DIAG_START";
MSG[MSG["DIAG_END"] = 399] = "DIAG_END";
MSG[MSG["ERROR_START"] = 400] = "ERROR_START";
MSG[MSG["WRONG_STATE"] = 405] = "WRONG_STATE";
MSG[MSG["MEDIA_ACTION_UNSUPPORTED"] = 460] = "MEDIA_ACTION_UNSUPPORTED";
MSG[MSG["ERROR_END"] = 499] = "ERROR_END";
MSG[MSG["FAILURE_START"] = 500] = "FAILURE_START";
MSG[MSG["FAILURE_PLAYER_EMPTY"] = 501] = "FAILURE_PLAYER_EMPTY";
MSG[MSG["FAILURE_END"] = 599] = "FAILURE_END";
})(MSG = Pianod.MSG || (Pianod.MSG = {}));
;
Pianod.SongPlayingMessages = [Pianod.MSG.PLAYING, Pianod.MSG.PAUSED, Pianod.MSG.STALLED];
Pianod.SongDataFields = [Pianod.MSG.ID, Pianod.MSG.ALBUM, Pianod.MSG.ARTIST, Pianod.MSG.SONG, Pianod.MSG.ART,
Pianod.MSG.RATING, Pianod.MSG.PLAYLIST, Pianod.MSG.URL, Pianod.MSG.YEAR, Pianod.MSG.ACTIONS,
Pianod.MSG.PLAYLISTRATING];
function sortOnFields(data, fields) {
if (typeof (fields) == "string") {
fields = [fields];
}
return data.sort(function (a, b) {
for (let f of fields) {
let alc = '', blc = '';
if (f in a)
alc = a[f].toLowerCase();
if (f in b)
blc = b[f].toLowerCase();
if (alc < blc)
return -1;
if (alc > blc)
return 1;
}
return 0;
});
}
Pianod.sortOnFields = sortOnFields;
let FieldType;
(function (FieldType) {
FieldType[FieldType["String"] = 0] = "String";
FieldType[FieldType["Rating"] = 1] = "Rating";
FieldType[FieldType["Flags"] = 2] = "Flags";
})(FieldType || (FieldType = {}));
;
const ParseRules = {
111: { name: 'id' },
112: { name: 'album' },
113: { name: 'artist' },
114: { name: 'song' },
115: { name: 'playlist' },
116: {
name: 'rating',
type: FieldType.Rating,
words: ['seed', 'albumseed', 'artistseed']
},
117: { name: 'url' },
118: { name: 'art' },
119: { name: 'genre' },
120: {
name: 'playlistrating',
type: FieldType.Rating,
words: []
},
121: { name: 'explanation' },
122: { name: 'owner' },
123: { name: 'source' },
124: { name: 'sourcename' },
125: { name: 'year' },
126: { name: 'duration' },
127: {
name: 'action',
type: FieldType.Flags,
words: ['rate', 'request', 'seed', 'albumseed', 'artistseed']
},
132: { name: 'info' },
136: {
name: 'privilege',
type: FieldType.Flags,
words: ['disabled', 'guest', 'listener', 'admin', 'user',
'service', 'influence', 'tuner', 'deejay']
},
148: { name: 'room' },
174: { name: 'pathname' },
175: { name: 'selectionAlgorithm' }
};
;
class Message {
constructor(message) {
if (!message)
return;
this.code = parseInt(message.substring(0, 3), 10);
this.text = message.substring(4);
let pieces = this.text.split(': ');
this.tag = pieces[0];
this.value = pieces.splice(1).join(': ');
this.words = this.value.split(/\s+/);
if (this.code in ParseRules &&
'words' in ParseRules[this.code]) {
let wds = ParseRules[this.code].words;
this.flags = {};
for (let w of wds) {
this.flags[w] = (this.words.indexOf(w) >= 0);
}
if (ParseRules[this.code].type == FieldType.Rating &&
this.words.length >= 2 && this.isNumeric(this.words[1])) {
this.rated_word = this.words[0];
this.rated = parseFloat(this.words[1]);
}
}
}
get ok() {
return (this.code >= 200 && this.code <= 299);
}
isNumeric(str) {
let v = parseFloat(str);
return !isNaN(v) && isFinite(v);
}
}
Pianod.Message = Message;
class MusicData {
populate(message) {
assert(message.code in ParseRules);
let rules = ParseRules[message.code];
let type = rules.type || FieldType.String;
if (type == FieldType.String) {
this[rules.name] = message.value;
}
else if (type == FieldType.Flags || type == FieldType.Rating) {
this[rules.name] = message.flags;
if (type == FieldType.Rating) {
this[rules.name]['rated'] = message.rated;
this[rules.name]['rated_word'] = message.rated_word;
}
}
else {
assert(!"Unknown field type");
}
}
get type() {
if ('song' in this)
return "song";
if ('album' in this)
return "album";
if ('artist' in this)
return "artist";
return "playlist";
}
get name() {
return this.song || this.album || this.artist || this.playlist || this.genre || '';
}
}
Pianod.MusicData = MusicData;
class Response extends Message {
constructor() {
super(...arguments);
this.diagnostics = [];
this.data = [];
this.record = [];
}
getSortedColumn(column) {
let data = [];
for (let row of this.record) {
data.push(row[column]);
}
data.sort(function (a, b) {
let alc = a.toLowerCase();
let blc = b.toLowerCase();
return (alc < blc ? -1 : alc > blc ? 1 : 0);
});
return data;
}
sortOnFields(fields) {
return sortOnFields(this.record, fields);
}
}
Pianod.Response = Response;
;
function assembleCommand(command, expression) {
let cmd;
for (let i = 0; i < command.length; i++) {
command[i] = command[i].toString().replace(/"/g, '""');
}
cmd = '"' + command.join('" "') + '"';
if (typeof (expression) != 'undefined') {
cmd += ' ' + expression;
}
return cmd;
}
Pianod.assembleCommand = assembleCommand;
class Connection {
constructor(piano_url, successcb, disconnectcb) {
this.commands = { 0: {
'id': 0,
'command': 'connect',
'handler': () => void {}
} };
this.commandid = 1;
this.responseid = 0;
this.response = new Response();
this.record = {};
this.subscriptions = {};
this.backissues = {};
this.close_functions = [];
this.state = Connection.STATE.DISCRETE;
if (typeof (disconnectcb) == 'function') {
this.close_functions.push(disconnectcb);
}
if (typeof (successcb) == 'function') {
this.commands[0].handler = successcb;
}
this.socket = new WebSocket(piano_url);
this.socket.onmessage = (event) => {
this.receiveMessage(event);
};
this.socket.onopen = (event) => {
};
this.socket.onclose = (event) => {
this.closed(event);
};
this.socket.onerror = (event) => {
this.closed(event);
};
}
merge(from, into) {
for (let key in from) {
if (from.hasOwnProperty(key)) {
assert(!(key in into));
into[key] = from[key];
}
}
}
command_complete(msg) {
let cmd = this.commands[this.responseid];
if (typeof (cmd) == 'undefined') {
alert('Excess 200/400 Success/Failure responses received!');
console.log('Excess 200/400 Success/Failure responses received!');
}
else {
delete this.commands[this.responseid++];
if (Connection.debug) {
console.log("Received response: ", msg);
}
assert(typeof (cmd.handler) == 'function');
cmd.handler(msg);
}
}
pushRecord() {
let parsed = new MusicData();
for (let field in this.record) {
parsed.populate(this.record[field]);
}
this.response.data.push(this.record);
this.response.record.push(parsed);
this.record = {};
}
receiveMessage(event) {
if (Connection.debug) {
console.log(event.data);
}
let msg = new Message(event.data);
if ((msg.code == MSG.DATA_START || msg.code == MSG.DATA_END) &&
Object.keys(this.record).length > 0) {
this.pushRecord();
}
if (msg.code == MSG.DATA_START) {
this.state = Connection.STATE.COMMAND;
}
else if ((msg.code >= MSG.SUCCESS_START && msg.code <= MSG.SUCCESS_END) ||
(msg.code >= MSG.ERROR_START && msg.code <= MSG.ERROR_END)) {
assert(Object.keys(this.record).length === 0);
assert(this.response.data.length === 0 || msg.code == MSG.DATA_END);
if (this.state == Connection.STATE.COMMAND && this.response.diagnostics.length == 0 &&
msg.code != MSG.DATA_END) {
alert('Received ' + msg.code + ' instead of ' + MSG.DATA_END);
console.log('Received ', msg.code, ' instead of ', MSG.DATA_END);
console.log('Command: ', this.commands[this.responseid].command);
}
this.response.command = this.commands[this.responseid].command;
this.merge(msg, this.response);
if (this.response.record.length == 1) {
this.merge(this.response.record[0], this.response);
}
let resp = this.response;
this.response = new Response();
this.command_complete(resp);
this.state = Connection.STATE.DISCRETE;
}
else if (msg.code >= MSG.DIAG_START && msg.code <= MSG.DIAG_END) {
this.state = Connection.STATE.COMMAND;
this.response.diagnostics.push(msg);
}
else if (this.state == Connection.STATE.COMMAND &&
msg.code >= MSG.INFO_START && msg.code <= MSG.INFO_END) {
if (msg.code in this.record) {
assert(Object.keys(this.record).length == 1);
this.pushRecord();
}
this.record[msg.code] = msg;
}
else {
for (let script of (this.subscriptions[msg.code] || [])) {
script(msg);
}
this.backissues[msg.code] = msg;
}
}
closed(event) {
for (let i = this.close_functions.length - 1; i >= 0; i--) {
this.close_functions[i](event);
}
}
requestTimeStatus() {
this.socket.send(' ');
}
sendCommand(command, func) {
if (command instanceof Array) {
command = assembleCommand(command);
}
let cmd = {
'id': this.commandid,
'command': command,
'handler': func || function () { }
};
this.commands[this.commandid++] = cmd;
this.socket.send(command);
}
;
on_close(func) {
this.close_functions.push(func);
}
subscribe(codes, func, ketchup = true) {
if (typeof (codes) == 'number') {
codes = [codes];
}
for (let code of codes) {
if (!(code in this.subscriptions)) {
this.subscriptions[code] = [];
}
this.subscriptions[code].push(func);
if (Connection.debug) {
console.log('subscribed ', func, ' to ', code);
}
if (ketchup && code in this.backissues) {
func(this.backissues[code]);
}
}
}
}
Connection.STATE = {
COMMAND: 1,
DISCRETE: 2
};
Connection.debug = false;
Pianod.Connection = Connection;
class UrlInfo {
}
Pianod.UrlInfo = UrlInfo;
class Finder {
static parseQueryStrings() {
let query_strings = {};
let url = window.location.toString().split('?');
if (url.length > 1) {
let pairs = url[1].split('&');
for (let i = 0; i < pairs.length; i++) {
let param = pairs[i].split('=');
query_strings[param[0]] = param[1];
}
}
return query_strings;
}
static getURLInfo() {
let url = window.location.toString().split('?');
if (url.length < 1)
return undefined;
let result = new UrlInfo();
result.url = url[0];
let domains = url[0].split('/');
if (domains.length <= 3)
return undefined;
result.service = domains[3] || '';
result.path = domains.splice(3).join('/');
let scheme = domains[0];
if (scheme != 'http:' && scheme != 'https:')
return undefined;
result.scheme = scheme;
result.secure = (scheme == 'https:');
let domain = domains[2].toLowerCase();
result.domain = domain;
result.fromweb = domain.indexOf('deviousfish.com') >= 0;
return result;
}
static getPianodURL() {
let url = Finder.getURLInfo();
if (url === undefined)
return undefined;
if (url.fromweb)
return undefined;
return (url.secure ? 'wss://' : 'ws://') + url.domain + '/' + url.service;
}
static getBestURL() {
let query_params = Finder.parseQueryStrings();
if ('server' in query_params) {
let server = query_params['server'] || '';
let room = query_params['room'] || 'pianod';
if (server !== undefined && server !== '') {
return 'ws://' + server + '/' + room;
}
}
let url = Finder.getPianodURL();
if (url !== undefined)
return url;
return 'ws://house.perette.barella.org:4446/pianod';
}
}
Pianod.Finder = Finder;
;
})(Pianod || (Pianod = {}));
export class PianodConsole {
constructor(url) {
this.start_time = 0;
this.duration = 0;
this.command_results = [];
this.toggles = {};
this.postLog = (msg) => {
let s = $($('#statuspane')[0]);
let code = msg.code.toString();
while (code.length < 3) {
code = '0' + code;
}
s.append(code + ' ' + msg.text + '</br>');
s.scrollTop(s.prop('scrollHeight'));
};
this.updateTrackTime = () => {
let now = new Date();
let point = Math.round((now.getTime() - this.start_time) / 1000);
let min = Math.floor(point / 60);
let sec = point % 60;
$('#timepoint').text(min + ':' + (sec <= 9 ? '0' : '') + sec);
if (this.duration > 0) {
let percent = Math.round(point * 10000 / this.duration) / 100;
if (percent > 100)
percent = 100;
$('#progressbar').css('width', percent + '%');
}
};
this.parseTrackTime = (msg) => {
$('.offline').hide();
$('.idle').hide();
$('.playing').show();
$('#paused').toggle(msg.code == Pianod.MSG.PAUSED);
$('#stalled').toggle(msg.code == Pianod.MSG.STALLED);
if (msg.code == Pianod.MSG.PLAYING) {
if (typeof (this.interval) == 'undefined') {
this.interval = setInterval(this.updateTrackTime, 1000);
}
}
else {
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
delete this.interval;
}
}
let now = new Date();
let timevalues = msg.words[0].split('/');
$('#duration').text(timevalues[1]);
let points = timevalues[0].split(':');
let durations = timevalues[1].split(':');
let point = parseInt(points[0], 10) * 60 + parseInt(points[1], 10);
this.duration = parseInt(durations[0], 10) * 60 + parseInt(durations[1], 10);
this.start_time = now.getTime() - point * 1000;
this.updateTrackTime();
};
this.privilegeChange = (msg) => {
let privs = msg.flags || [];
$('.privadmin').toggleClass('disabled', !privs['admin']);
$('.privuser').toggleClass('disabled', !privs['admin'] && !privs['user']);
$('.privlistener').toggleClass('disabled', !privs['admin'] && !privs['user'] && !privs['listener']);
};
$('.playbackstatus').hide();
$('.idle').hide();
$('.playing').hide();
$('.offline').show();
this.url = url;
this.privilegeChange({ flags: { 'admin': false, 'user': false, 'listener': false,
'service': false, 'influence': false } });
this.restoreToggles();
this.openPiano();
}
setField(code, value) {
if (code == Pianod.MSG.ART) {
$('#albumart').attr('src', value === '' ? 'no-art.jpeg' : value);
}
else {
let id = PianodConsole.code_to_location[code];
$('#' + id).toggle(value !== '');
$('#' + id + ' .value').text(value);
}
}
toggleField(field) {
let name = 'show' + field;
let enabled = $('#' + name).is(':checked');
this.toggles[field] = enabled;
if (!enabled) {
localStorage[name] = "false";
}
else {
delete localStorage[name];
}
this.renderResults();
}
restoreToggles() {
for (let i = 100; i < 200; i++) {
let name = 'show' + i;
this.toggles[i] = !(name in localStorage);
}
}
static escapeHtml(str) {
return str.replace(/[&<>"'\/]/g, (s) => {
return PianodConsole.entityMap[s];
});
}
renderResults() {
$('#page').text(this.page);
let id = this.page - 1;
let cmd = this.command_results[id].command;
$('#commandinput .value').text(cmd);
$('#commandinput .result').text(this.command_results[id].code);
let results = this.command_results[id].data;
$('#columntoggles').toggle(results.length > 1);
if (results.length === 0) {
$('#commandoutput').text(this.command_results[id].text);
return;
}
let html = '<table>', keydata = {};
for (let result of results) {
for (let key of Object.keys(result)) {
keydata[key] = result[key].tag;
}
}
let keys = Object.keys(keydata);
if (results.length == 1) {
html += '<tbody>';
for (let col of keys) {
html += '<tr><th>' + col + '/' + keydata[col] + '</th><td>' +
PianodConsole.escapeHtml(results[0][col].value) + '</td></tr>';
}
html += '</tbody>';
}
else {
let toggles = $('<ul/>');
let numerics = "", titles = "", body = "";
for (let col of keys) {
toggles.append($('<li/>').html('<input id="show' + col + '" type="checkbox" ' +
(this.toggles[col] ? 'checked ' : '') + ' onclick="console.toggleField (' + col +
');"><label for="show' + col + '" title="' + keydata[col].replace(/"/g, '') + '">' + col + '</label>'));
if (!this.toggles[col])
continue;
numerics += '<th>' + col + '</th>';
titles += '<th>' + keydata[col] + '</th>';
}
$('#columntoggles').html(toggles.html());
if (numerics == '') {
$('#commandoutput').text('Please enable some columns.');
return;
}
for (let row of results) {
html += '<tr>';
for (let col of keys) {
if (!this.toggles[col])
continue;
if (col in row) {
html += '<td>' + PianodConsole.escapeHtml(row[col].value) + '</td>';
}
else {
html += '<td class="missing">&nbsp;</td>';
}
}
html += '</tr>';
}
html += '<thead><tr>' + numerics + '</tr><tr>' + titles +
'</tr></thead><tbody><tr>' + body + '</tr></tbody>';
}
html += '</table>';
$('#commandoutput').html(html);
}
postCommand(command) {
this.piano.sendCommand(command, (response) => {
this.command_results.push(response);
this.page = this.command_results.length;
$('#pagecount').text(this.page);
this.renderResults();
});
}
openPiano() {
this.piano = new Pianod.Connection(this.url, () => {
this.piano.subscribe(Pianod.MSG.PRIVILEGES, this.privilegeChange);
this.piano.subscribe([Pianod.MSG.PLAYING, Pianod.MSG.PAUSED, Pianod.MSG.STALLED], this.parseTrackTime, false);
this.piano.subscribe(Pianod.MSG.TRACKCOMPLETE, () => {
$('.offline').hide();
$('.playing').hide();
$('.idle').show();
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
delete this.interval;
}
for (let field of PianodConsole.song_data_fields) {
this.setField(field, '');
}
});
this.piano.subscribe([Pianod.MSG.IDLE, Pianod.MSG.INTERTRACK], () => {
$('.offline').hide();
$('.playing').hide();
$('.idle').show();
}, false);
this.piano.subscribe(Pianod.MSG.SELECTEDPLAYLIST, (msg) => {
$('#playlistname .value').text(msg.value);
}, false);
this.piano.subscribe([Pianod.MSG.ALBUM, Pianod.MSG.ARTIST, Pianod.MSG.SONG, Pianod.MSG.ART,
Pianod.MSG.PLAYLIST], (msg) => {
this.setField(msg.code, msg.value);
});
for (let i = Pianod.MSG.EVENT_START; i <= Pianod.MSG.INFO_END; i++) {
this.piano.subscribe(i, this.postLog);
}
for (let i = Pianod.MSG.FAILURE_START; i <= Pianod.MSG.FAILURE_END; i++) {
this.piano.subscribe(i, this.postLog);
}
$('#columntoggles').hide();
}, () => {
$('.idle').hide();
$('.playing').hide();
$('.offline').show();
delete this.piano;
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
delete this.interval;
}
});
$('.command').each(function () {
let id = $(this).prop('id');
if (id) {
$(this).val(localStorage[id]);
}
});
}
destroy() {
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
}
}
execute(command) {
this.postCommand(command);
}
previousPage() {
if (this.page > 1) {
--this.page;
this.renderResults();
}
}
nextPage() {
if (this.page < this.command_results.length) {
++this.page;
this.renderResults();
}
}
refresh() {
this.renderResults();
}
}
PianodConsole.code_to_location = {
112: 'albumname',
113: 'artistname',
114: 'trackname',
115: 'songplaylist'
};
PianodConsole.song_data_fields = [Pianod.MSG.ALBUM, Pianod.MSG.ARTIST, Pianod.MSG.SONG, Pianod.MSG.ART, Pianod.MSG.RATING,
Pianod.MSG.PLAYLIST, Pianod.MSG.ID, Pianod.MSG.URL, Pianod.MSG.YEAR, Pianod.MSG.ACTIONS,
Pianod.MSG.PLAYLISTRATING];
PianodConsole.Finder = Pianod.Finder;
PianodConsole.entityMap = {
"&": "&amp;",
"<": "&lt;",
">": "&gt;",
'"': '&quot;',
"'": '&#39;',
"/": '&#x2F;'
};
$(function () {
$('.command').keydown(function (event) {
let keypressed = event.keyCode || event.which;
if (keypressed == 13) {
let id = $(this).prop('id');
let command = $(this).val().trim();
if (id) {
localStorage[id] = command;
}
if (command !== '') {
pdconsole.execute(command);
}
}
});
$('.switch').change(function () {
let id = $(this).prop('id');
if (id) {
localStorage[id] = $(this).is(':checked') ? 'true' : 'false';
}
pdconsole.refresh();
});
});
