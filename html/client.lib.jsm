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
export class View {
constructor(view_name, singleton) {
this.shown = true;
this.single = false;
this.accordionClickHandler = (target) => {
let fold = $(target);
let closing = fold.hasClass('selected');
fold.parent().find('h3').removeClass('selected');
if (!closing) {
let foldname = fold.attr('id');
if (typeof (foldname) != 'undefined') {
eval('this.open_' + foldname + '();');
}
fold.addClass('selected');
}
};
if (!View.initialized) {
View.initialized = true;
window.onresize = () => {
View.updateView();
};
}
assert(!(view_name in View.views));
View.VIEWS.push(view_name);
View.views[view_name] = this;
View.view_panes[view_name] = 0;
this.name = view_name;
View.reset($('#' + this.name + 'view'));
this.setupAccordions($('#' + view_name + 'view'));
this.single = singleton;
}
destroy() {
assert(this.name in View.views);
delete View.view_panes[this.name];
delete View.views[this.name];
View.VIEWS.splice(View.VIEWS.indexOf(this.name), 1);
View.view_order.splice(View.view_order.indexOf(this.name), 1);
View.reset($('#' + this.name + 'view'));
}
;
static reset(v) {
v.find('*').off();
v.find('.accordion').parent().find('h3').removeClass('selected');
v.find('input[type=text]').val('');
v.find('input[type=url]').val('');
v.find('input[type=password]').val('');
v.find('textarea').val('');
v.find('select').prop('selectedIndex', 0);
v.find('.ratingwidget').html('');
}
static updateView() {
let width = $(window).innerWidth();
let height = $(window).innerHeight();
let font_size = Math.round(parseInt($('#status').css('font-size').replace('.px', '')));
let line_height = font_size * 1.3;
let height_in_lines = Math.floor(height / line_height);
let total_panes = Math.floor(width / View.MIN_PANE_WIDTH);
let pane_extra = Math.floor((width - (total_panes * View.MIN_PANE_WIDTH)) / total_panes);
let panes_left = total_panes;
height_in_lines -= (panes_left <= 1 ? 5 : 4);
if (height_in_lines < 0) {
height_in_lines = 1;
}
$('#views > div').hide();
$('#views').css('min-height', Math.floor(height_in_lines * line_height) + 'px');
let i;
for (i = 0; i < View.view_order.length && panes_left > 0; i++) {
let this_view = View.view_order[i];
let pane_width = this.views[this_view].getWidth(total_panes, panes_left, height_in_lines);
View.view_panes[this_view] = pane_width;
if (pane_width === 0) {
continue;
}
let flex = pane_width + ' ' + pane_width + ' ' + (240 * pane_width) + 'px';
let column_width = (220 + pane_extra) + 'px auto';
$('#' + this.view_order[i] + 'view').show().
attr('class', 'columns-' + pane_width).
css('-webkit-flex', flex).
css('flex', flex).
find('.columnar').
css('columns', column_width).
css('-webkit-columns', column_width);
panes_left -= pane_width;
}
$('#views').toggleClass('single', i == 1);
$('#switcher').toggleClass('collapse', total_panes <= 2);
$('#switcher .handle').toggleClass('hidden', total_panes > 2);
$('#switcher .menu').toggle(total_panes > 2);
for ( ; i < View.view_order.length; i++) {
View.view_panes[View.view_order[i]] = 0;
}
}
static viewIsShown(view_name) {
return (View.view_panes[view_name] > 0);
}
static showView(view_names) {
if (typeof (view_names) == 'string') {
view_names = [view_names];
}
$('#switcher.collapse .menu').hide();
let primary_view = View.views[view_names[0]];
assert(primary_view.shown);
let new_order = [view_names[0]];
if (!primary_view.single) {
if (view_names[0] != 'track' && 'track' in View.views) {
new_order.push('track');
}
for (let view of View.view_order.concat(view_names, Object.keys(View.views))) {
assert(view in View.views);
if (new_order.indexOf(view) < 0 && View.views[view].shown) {
new_order.push(view);
}
}
}
this.view_order = new_order;
this.updateView();
}
static getView(view_name) {
assert(view_name in View.views);
return (View.views[view_name]);
}
static toggleMenu() {
$('#switcher.collapse .menu').toggle();
}
static renderOptions(target, items, value_field, text_field, retain = 1) {
let list = $(target);
let selected = View.getSelectedValue(list);
list.find("option").slice(retain).remove();
for (let obj of items) {
let option = $('<option/>').text(obj[text_field]);
if (value_field !== null) {
option.attr('value', obj[value_field]);
}
list.append(option);
}
list.prop('selectedIndex', 0);
if (selected !== '' && value_field !== null) {
let moved_to = list.find('option[value=' + selected + ']');
if (moved_to.length < 0) {
return false;
}
moved_to.prop('selected', 'selected');
}
return true;
}
getWidth(total_panes, panes_left, height_in_lines) {
return 1;
}
show() {
this.shown = true;
View.showView(this.name);
}
isShown() {
return View.viewIsShown(this.name);
}
hide() {
this.shown = false;
}
reportStatus(success, response, status, args, show_popup) {
$('#status').toggleClass('error', !success);
let message = translate(typeof (status) == 'string' ? status : response);
for (let phrase of args) {
message += translate(phrase);
}
let diag = $('<ul/>');
let have_diags = false;
if (response !== null) {
for (let d of response.diagnostics) {
have_diags = true;
diag.append($('<li/>').text(translate(d)));
}
}
if (have_diags && typeof (status) != 'undefined') {
message += (message.substr(-1) == '.' ? '  ' : ': ') + translate(response);
}
$('#status .value').text(message);
$('#diagmain').text(message);
if (show_popup) {
$('#diagdetails').html(diag.html());
$('#diagnostics').attr('class', 'popup ' + (have_diags && success ? 'warn' : success ? 'success' : 'error'));
$('#showerrors').toggle(have_diags);
let e = $('#diagnostics').show();
if (!have_diags) {
e.stop();
e.fadeOut(2750);
}
}
}
status(status, ...args) {
this.reportStatus(true, null, status, args, false);
}
success(response, status, ...args) {
this.reportStatus(true, response, status, args, true);
}
error(status, ...args) {
this.reportStatus(false, null, status, args, true);
}
commError(response, status, ...args) {
this.reportStatus(false, response, status, args, true);
}
clear() {
$('#status').removeClass('error');
$('#status .value').html('&nbsp;');
$('#diagnostics').hide();
}
closeAccordion(view = this.name) {
let accordions = $('#' + view + 'view .accordion');
accordions.each((unused, accordion) => {
$(accordion).find('h3').removeClass('selected');
});
}
setupAccordions(v) {
v.find('.accordion h3').click((trigger) => { this.accordionClickHandler(trigger.target); });
}
static getInputText(input_id) {
let input = $(input_id);
assert(input.is('input[type=text]') || input.is('input[type=password]') || input.is('input[type=email]') || input.is('input[type=url]') || input.is('textarea'));
return input.val().trim();
}
static getInputNumber(input_id) {
let input = $(input_id);
assert(input.is('input[type=number]') || input.is('input[type=range]'));
return input.val();
}
static getSelectedValue(input_id) {
let input = $(input_id);
assert(input.is('select'));
return input.val();
}
static isInputChecked(input_id) {
let input = $(input_id);
assert(input.is('input[type=checkbox]') || input.is('type=input[radio]'));
return input.is(':checked');
}
}
View.VIEWS = [];
View.MIN_PANE_WIDTH = 250;
View.view_order = [];
View.views = {};
View.view_panes = {};
View.initialized = false;
export class CommView extends View {
constructor(view_name, connection) {
super(view_name, false);
this.protocol_version = 0;
this.privilegeChange = (msg) => {
assert('flags' in msg);
let privs = msg.flags || [];
for (let p of ['admin', 'user', 'service', 'influence', 'deejay']) {
let disabled = !privs[p];
if (p == 'user') {
disabled = !privs['admin'] && !privs['user'];
}
$('.priv' + p).toggleClass('disabled', disabled);
$('input.priv' + p).prop('disabled', disabled);
$('select.priv' + p).prop('disabled', disabled);
}
};
this.recordProtocolVersion = (msg) => {
let pieces = msg.text.split(".");
this.protocol_version = 0;
if (pieces.length >= 2) {
let words = pieces[0].split(/\s+/);
if (words.length >= 2 && words[0] == 'pianod2') {
this.protocol_version = parseInt(words[1], 10);
for (let pv = 1; pv <= 500; pv++) {
$('.protoend' + pv).toggle(this.protocol_version < pv);
$('.proto' + pv).toggle(this.protocol_version >= pv);
}
}
else {
alert('Service identifies itself as ' +
(words.length >= 2 ? words[0] : "(unidentified)") +
', not pianod2.');
}
}
};
this.piano = connection;
this.privilegeChange({ flags: { 'admin': false, 'user': false, 'service': false, 'influence': false } });
this.piano.subscribe(Pianod.MSG.WELCOME, this.recordProtocolVersion);
this.piano.subscribe(Pianod.MSG.PRIVILEGES, this.privilegeChange);
}
destroy() {
super.destroy();
}
;
logout() {
this.piano.sendCommand('QUIT');
}
execute(command, success, failure, suppress_clear) {
this.piano.sendCommand(command, (response) => {
if (response.ok) {
if (typeof (success) == 'string') {
this.success(response, success);
}
else {
if (typeof (suppress_clear) != 'undefined') {
}
else if (typeof (failure) != 'undefined' && failure === null) {
}
else {
this.clear();
}
if (typeof (success) != 'undefined') {
success(response);
}
}
}
else if (typeof (failure) == 'undefined' || failure === null) {
this.commError(response);
}
else {
this.commError(response, failure, failure.substr(-1) == '.' ? '  ' : ': ', translate(response));
}
});
}
executeControlled(command, callback) {
this.piano.sendCommand(command, callback);
}
}
export class TableView extends CommView {
constructor(name, piano, config) {
super(name, piano);
this.button_actions = {
'SKIP': (item) => { this.execute('SKIP'); },
'QUE': (item) => { this.execute(['REQUEST', 'ID', item.id], 'Added ' + item.name + ' to the queue.'); },
'DEQUE': (item) => { this.execute(['REQUEST', 'CANCEL', 'ID', item.id], 'Dequeued ' + item.name + '.'); },
'SEED': (item) => { View.getView('search').showSeedMenu(item); }
};
this.table_data = [];
this.setSortOrder = (field) => {
assert(typeof (field) != 'undefined');
let index = this.sort_order.indexOf(field);
assert(index != -1);
this.sort_order.splice(index, 1);
this.sort_order.unshift(field);
this.render();
};
this.min_panes = config.min_panes || 1;
this.max_panes = config.max_panes || 3;
this.reserved_lines = config.reserved_lines || 4;
this.min_height = config.min_height || 5;
this.anchor = config.anchor;
this.sort_order = config.fields || ['artist', 'album', 'song', 'genre', 'year', 'playlist', 'duration'];
this.extra_fields = config.extra_fields || [];
this.fields = this.sort_order.concat(this.extra_fields);
this.renderer = {
'song': this.renderSong,
'album': this.renderAlbum,
'artist': this.renderArtist,
'action': this.renderAction
};
if (typeof (config.sort) != 'undefined' && !config.sort) {
this.sort_order = [];
}
}
static getResponseFields(response, fields, always_include = []) {
let found = [];
for (let field of fields) {
if (always_include.indexOf(field) >= 0) {
found.push(field);
}
else {
for (let record of response) {
if (field in record) {
found.push(field);
break;
}
}
}
}
return found;
}
createButton(item, text, func) {
let button = $('<a/>').text(translate('ACTION_TABLE_' + text)).addClass('button');
if (typeof (func) == 'undefined') {
assert(text in this.button_actions);
func = this.button_actions[text];
}
return button.click(() => { func.call(this, item); });
}
renderSong(item) {
return $('<a />').text(item.song).
click(() => { View.getView('search').research('song', item.song); });
}
renderAlbum(item) {
return $('<a />').text(item.album).
click(() => { View.getView('search').research('album', item.album); });
}
renderArtist(item) {
return $('<a />').text(item.artist).
click(() => { View.getView('search').research('artist', item.artist); });
}
renderAction(item) {
let actions = $('<span/>');
if ('action' in item && item.action.request) {
actions.append(this.createButton(item, 'QUE'));
}
actions.append(this.createButton(item, 'SEED'));
return actions;
}
startRow(record) {
return $('<tr/>');
}
renderTable() {
let head = this.anchor.find('thead');
let headers = $('<tr />');
for (let col of this.render_fields) {
let sort_rank = this.sort_order.indexOf(col);
let ind = (sort_rank >= 0 ? '⬍' : '');
if (sort_rank >= 0 && sort_rank < TableView.SortIndicators.length) {
ind = TableView.SortIndicators[sort_rank];
}
let th = $('<th />').text(translate('HEADING_' + col.toUpperCase()) + ind + ' ');
if (sort_rank >= 0) {
th.click(() => { this.setSortOrder(col); });
}
headers.append(th);
}
head.html('');
head.append(headers);
let body = this.anchor.find('tbody');
body.text('');
for (let record of this.table_data) {
let row = this.startRow(record);
for (let col of this.render_fields) {
let value = $('');
if (col in this.renderer) {
value = this.renderer[col].call(this, record);
}
else if (col in record) {
value = $('<span />').text(record[col]);
}
row.append($('<td />').append(value));
}
body.append(row);
}
}
render(data) {
if (typeof (data) != 'undefined') {
this.table_data = data;
this.render_fields = TableView.getResponseFields(this.table_data, this.fields, this.extra_fields);
}
if (this.table_data.length == 0) {
this.anchor.find('thead').html('<tr><td>&nbsp;</td></tr>');
this.anchor.find('tbody').html('<tr><td>No data.</td></tr>');
}
else {
if (this.sort_order.length) {
Pianod.sortOnFields(this.table_data, this.sort_order);
}
this.renderTable();
}
}
setRenderer(render) {
for (let rend in render) {
this.renderer[rend] = render[rend];
}
}
getWidth(total, available, height) {
height -= this.reserved_lines;
if (height < this.min_height)
height = this.min_height;
this.anchor.css('height', height * 2.5 + 'ex');
if (available < this.min_panes) {
return (available == total ? available : 0);
}
let reserve = 0;
if (total == available && available > this.min_panes) {
reserve = 1;
}
return available > this.max_panes ? this.max_panes : available - reserve;
}
getById(id) {
for (let i of this.table_data) {
if (i['id'] == id) {
return i;
}
}
assert(!"Not found");
throw new Error(id + ": ID not found.");
}
}
TableView.SortIndicators = ['▼', '▾', '▿'];
;
export class Client {
constructor() {
this.launchClient = (pianod) => {
this.loginview.hide();
this.activityview = new ActivityView('activity', pianod);
this.queueview = new QueueView('queue', pianod);
this.userview = new UserView('user', pianod);
this.sourceview = new SourceView('source', pianod);
this.playlistview = new PlaylistView('playlist', pianod);
this.searchview = new SearchView('search', pianod);
this.seedview = new SeedView('seed', pianod);
this.trackview = new TrackView('track', pianod);
View.showView(['track', 'playlist', 'search', 'activity']);
$('#switcher').show();
};
let query_string = Pianod.Finder.parseQueryStrings();
this.loginview = new LoginView('login', query_string, this.launchClient, this.terminateSession);
this.loginview.show();
}
terminateSession() {
$('#switcher').hide();
for (let view of Client.VIEWS) {
View.getView(view).destroy();
eval("delete this." + view + "view;");
}
}
show(view) {
View.showView(view);
}
toggleMenu() {
View.toggleMenu();
}
execute(command, success_message) {
this.trackview.execute(command, success_message);
}
}
Client.VIEWS = ['track', 'seed', 'search', 'playlist',
'source', 'user', 'queue', 'activity'];
export class ActivityView extends CommView {
constructor(name, piano) {
super(name, piano);
this.history = [];
piano.subscribe([Pianod.MSG.WELCOME, Pianod.MSG.YELL, Pianod.MSG.ACTIVITY, Pianod.MSG.STATUS, Pianod.MSG.SOURCESTATUS], (message) => {
this.appendHistory(message);
});
this.executeControlled(['USERS', 'ONLINE'], (response) => {
if (response.ok) {
let users = response.getSortedColumn('id');
this.history = users.reverse().concat([translate('HEADING_LISTENERS_ONLINE')], this.history);
}
else {
this.history.push(translate('ERROR_ACTIVITY_DISABLED'));
}
this.refresh();
});
$('#yellmessage').entergize(() => {
$('#yellbutton').click();
});
}
refresh() {
if (this.history.length > 0) {
let log = $('#recentactivity');
log.text('');
for (let line of this.history) {
log.prepend($('<li/>').text(line));
}
}
}
appendHistory(msg) {
this.history.push(msg.text);
if (this.history.length > ActivityView.history_limit) {
this.history = this.history.slice(25);
this.refresh();
}
else {
$('#recentactivity').append($('<li/>').text(msg.text));
}
$('#recentactivity').stop().animate({
scrollTop: $('#recentactivity')[0].scrollHeight
}, 1000);
}
getWidth(total, available, height) {
height -= ActivityView.reserved_lines;
$('#recentactivity').css('height', height * 2.5 + 'ex');
if (total == 2 && available == 2)
return 1;
return available > 2 ? 2 : available;
}
yell() {
let message = View.getInputText('#yellmessage').replace(/\n/g, "  ").replace('"', "'");
if (message.match(/^\s*$/)) {
this.error('ERROR_YELL_NO_MESSAGE');
}
else {
$('#yellmessage').val('');
this.execute(['YELL', message]);
}
}
}
ActivityView.history_limit = 200;
ActivityView.reserved_lines = 6;
export class QueueView extends TableView {
constructor(name, piano) {
super(name, piano, {
max_panes: 4,
min_height: 7,
reserved_lines: 1,
anchor: $('#queue'),
extra_fields: ['action'],
sort: false
});
this.herstory = [];
this.queue = [];
this.current_track = null;
this.subscriptions = [];
this.receiveHistory = (response) => {
this.herstory = response.record.reverse();
for (let item of this.herstory) {
item['chrono'] = 'past';
}
};
this.receiveQueue = (response) => {
this.queue = response.record;
for (let item of this.queue) {
item['chrono'] = 'future';
}
this.render();
};
piano.subscribe(Pianod.SongDataFields, (msg) => {
if (msg.code == Pianod.MSG.ID) {
if (this.queue.length && this.queue[0].id == msg.value) {
this.queue.shift();
}
this.loading_track = new Pianod.MusicData();
this.loading_track['chrono'] = 'present';
}
this.loading_track.populate(msg);
if (this.current_track !== null) {
this.announceSong();
}
});
piano.subscribe(Pianod.SongPlayingMessages, (msg) => {
if (this.current_track === null) {
this.current_track = this.loading_track;
this.announceSong();
}
});
piano.subscribe(Pianod.MSG.TRACKCOMPLETE, (msg) => {
this.loading_track['chrono'] = 'past';
this.herstory.push(this.loading_track);
this.current_track = null;
this.render();
}, false);
piano.subscribe(Pianod.MSG.QUEUE_CHANGED, (msg) => {
this.execute(['QUEUE', 'LIST'], this.receiveQueue, null);
}, false);
this.execute(['HISTORY', 'LIST'], this.receiveHistory, null);
this.execute(['QUEUE', 'LIST'], this.receiveQueue, null);
}
startRow(item) {
return $('<tr/>').addClass(item['chrono']);
}
render() {
let data = this.herstory.concat(this.current_track || [], this.queue);
super.render(data);
this.anchor.stop().animate({
scrollTop: $('#queue')[0].scrollHeight
}, 2000);
}
renderAction(item) {
let actions = $('<span/>');
let chrono = item['chrono'];
let button_type = '';
switch (chrono) {
case 'past':
if ('action' in item && item.action.request) {
button_type = 'QUE';
}
break;
case 'present':
button_type = 'SKIP';
break;
case 'future':
button_type = 'DEQUE';
break;
}
if (button_type != '') {
actions.append(this.createButton(item, button_type));
}
return actions;
}
subscribeToCurrentSong(sub) {
this.subscriptions.push(sub);
if (this.current_track !== null) {
sub(this.current_track);
}
}
announceSong() {
this.render();
for (let sub of this.subscriptions) {
sub(this.current_track);
}
}
getSong(index, update) {
let result = null;
if (index == 0) {
result = this.current_track;
}
else if (index > 0 && index <= this.queue.length) {
result = this.queue[index - 1];
}
else if (index < 0 && -index <= this.herstory.length) {
result = this.herstory[this.herstory.length + index];
}
if (result !== null && typeof (update) != 'undefined') {
this.executeControlled(['FIND', 'ALL', 'ID', result.id], (reply) => {
if (reply.ok) {
assert(reply.data.length == 1);
result = reply.record[0];
if (index < 0) {
result['chrono'] = 'past';
this.herstory[this.herstory.length + index] = result;
}
else if (index == 0 && this.loading_track.id == result.id) {
result['chrono'] = 'present';
this.loading_track = result;
if (this.current_track != null) {
this.current_track = result;
}
}
else if (index > 0 && index <= this.queue.length && this.queue[index - 1].id == result.id) {
result['chrono'] = 'future';
this.queue[index - 1] = result;
}
}
update(result);
});
}
return result;
}
}
var AuthorizationState;
(function (AuthorizationState) {
AuthorizationState[AuthorizationState["CONNECTED"] = 0] = "CONNECTED";
AuthorizationState[AuthorizationState["AUTHORIZING"] = 1] = "AUTHORIZING";
AuthorizationState[AuthorizationState["DISCONNECTED"] = 2] = "DISCONNECTED";
})(AuthorizationState || (AuthorizationState = {}));
export class LoginView extends View {
constructor(name, params, start_callback, end_callback) {
super(name, true);
this.user = localStorage['username'] || '';
this.password = localStorage['password'] || '';
this.server = localStorage['server'] || 'house.perette.barella.org:4446';
this.secure = ((localStorage['secure'] || '') == 'Secure');
this.use_encryption = false;
this.state = AuthorizationState.DISCONNECTED;
this.autologin = false;
this.save_credentials = true;
this.use_server_input = true;
this.authResponse = (response) => {
if (response.ok) {
this.success(response, "STATUS_AUTHENTICATION_OK");
if (this.save_credentials) {
localStorage['username'] = this.user;
localStorage['server'] = this.server;
localStorage['secure'] = (this.secure ? 'Secure' : '');
if ($('#storepassword').is(':checked')) {
localStorage['password'] = this.password;
}
else {
localStorage.removeItem('password');
}
}
this.state = AuthorizationState.CONNECTED;
this.auth_callback(this.pianod);
}
else {
this.error('ERROR_LOGIN_FAILED', ': ', translate(response));
this.pianod.sendCommand('QUIT', () => {
delete this.pianod;
});
}
};
this.authenticate = () => {
this.state = AuthorizationState.AUTHORIZING;
if (this.user === '') {
this.authResponse({ ok: true, diagnostics: [] });
}
else {
this.pianod.sendCommand(['USER', this.user, this.password], this.authResponse);
}
};
this.disconnect = () => {
if (this.state == AuthorizationState.CONNECTED) {
this.loss_callback();
this.show();
}
if (this.state == AuthorizationState.DISCONNECTED && this.server.indexOf(":") < 0) {
this.error('ERROR_CONNECTION_FAILED', '  ', 'CHECK_PORT_NUMBER');
}
else {
this.error(this.state == AuthorizationState.DISCONNECTED ? "ERROR_CONNECTION_FAILED" :
this.state == AuthorizationState.AUTHORIZING ? "ERROR_LOGIN_BAD_CREDENTIALS" :
"LOGIN_SESSION_ENDED");
}
this.state = AuthorizationState.DISCONNECTED;
};
this.auth_callback = start_callback;
this.loss_callback = end_callback;
let url = Pianod.Finder.getURLInfo();
if (typeof (url) != 'undefined') {
this.use_encryption = url.secure;
this.use_server_input = url.fromweb;
this.client_domain = url.domain;
}
if ('showserver' in params) {
this.use_server_input = true;
}
if (!this.use_server_input) {
$('#serverinput').hide();
}
if ('server' in params) {
this.server = params['server'] || '';
this.use_server_input = true;
}
if ('login' in params) {
let q = params['login'];
if (q == 'visitor') {
this.autologin = true;
this.save_credentials = false;
}
else if (q == 'auto') {
this.autologin = true;
}
else if (q == 'login') {
this.autologin = (this.user !== '');
}
else {
this.autologin = (q == this.user && this.user !== '');
if (this.user === '') {
this.user = q;
}
}
}
$('#server').entergize(function () {
$('#username').focus();
});
$('#username').entergize(function () {
$('#password').focus();
});
$('#password').entergize(function () {
$('#loginbutton').click();
});
$('#username').val(this.user);
$('#password').val(this.password);
$('#storepassword').prop('checked', this.password !== '');
$('#server').val(this.server);
$('#secureserver').prop('checked', this.secure);
if (this.autologin) {
this.connect(!this.save_credentials);
this.autologin = false;
}
}
connect(visitor) {
this.server = View.getInputText('#server');
this.secure = $('#secureserver').is(':checked');
let encrypt = this.use_encryption || this.secure;
let wsurl = (encrypt ? 'wss://' : 'ws://') +
(this.use_server_input ? this.server : this.client_domain) + '/pianod';
this.user = visitor ? '' : View.getInputText('#username');
this.password = visitor ? '' : View.getInputText('#password');
this.save_credentials = !visitor;
this.status(translate('LOGIN_SESSION_STARTING', wsurl));
this.pianod = new Pianod.Connection(wsurl, this.authenticate, this.disconnect);
if (typeof (this.pianod) == "undefined") {
this.error(this.server + ': ', 'ERROR_CONNECTION_FAILED');
}
}
static toggleSecurity() {
let secure = View.isInputChecked('#secureserver');
let server = View.getInputText('#server');
let split = server.split(':');
if (split.length == 2 && secure && split[1] == '4446') {
$('#server').val(split[0] + ':4447');
}
else if (split.length == 2 && !secure && split[1] == '4447') {
$('#server').val(split[0] + ':4446');
}
}
}
;
export class PlaylistView extends CommView {
constructor(name, piano) {
super(name, piano);
this.line_height = 1;
this.items_per_column = 15;
this.column_count = 1;
this.playlists = [];
this.subscriptions = [];
this.page = 0;
this.have_mix_selections = false;
this.current_mode = '';
this.shown_mode = 'playlist';
this.queue_mode = Pianod.MSG.STOPPED;
this.updatePlaylists = (response) => {
this.playlists = [];
let list = response.record;
for (let item of list) {
let rated = ('playlistrating' in item ?
item.playlistrating.rated : 0.0);
this.playlists.push({
name: item.playlist,
unique_name: item.playlist,
source: item.source,
sourcename: item.sourcename,
id: item['id'],
mix: false,
rating: rated
});
}
Pianod.sortOnFields(this.playlists, 'name');
let last_name = "";
for (let i = 0; i < this.playlists.length; i++) {
let this_name = this.playlists[i].name.toLowerCase();
let next_name = (i < this.playlists.length - 1 ? this.playlists[i + 1].name : "").toLowerCase();
if (this_name == last_name || this_name == next_name) {
this.playlists[i].unique_name += " (" + this.playlists[i].sourcename + ")";
}
last_name = this_name;
}
this.refresh();
for (let subscription of this.subscriptions) {
subscription(this.playlists);
}
};
this.updateMix = (response) => {
let list = response.record;
this.have_mix_selections = (list.length > 0);
for (let i = 0; i < this.playlists.length; i++) {
this.playlists[i].mix = false;
}
for (let i = 0; i < list.length; i++) {
for (let j = 0; j < this.playlists.length; j++) {
if (this.playlists[j].id == list[i]['id']) {
this.playlists[j].mix = true;
}
}
}
this.refresh();
};
piano.subscribe([Pianod.MSG.PLAYLISTS_CHANGED, Pianod.MSG.MIX_CHANGED, Pianod.MSG.SOURCES_CHANGED,
Pianod.MSG.USERRATINGS_CHANGED, Pianod.MSG.SELECTEDSOURCE], (msg) => { this.changeHandler(msg.code); });
piano.subscribe(Pianod.MSG.SELECTEDPLAYLIST, (msg) => {
let mode = msg.words[0];
if (this.shown_mode == this.current_mode || this.current_mode === '') {
this.shown_mode = mode;
$('#showmode').val(mode);
}
this.current_mode = mode;
this.refreshMix();
}, false);
piano.subscribe([Pianod.MSG.STOPPED, Pianod.MSG.REQUESTS, Pianod.MSG.RANDOMPLAY], (msg) => {
this.queue_mode = msg.code;
this.refreshMix();
}, false);
piano.subscribe(Pianod.MSG.FAILURE_PLAYER_EMPTY, () => {
this.error('ERROR_SELECT_PLAYLIST');
this.show();
}, false);
this.changeHandler(Pianod.MSG.PLAYLISTS_CHANGED);
this.execute('STATUS', (msg) => {
}, null);
}
createRater(playlist) {
let that = this;
return $('<span/>').rating(playlist.rating || 0.0).click(function () {
that.ratePlaylist(playlist.id, $(this).rating());
});
}
refresh() {
this.items_per_column = typeof (this.pane_height) == 'undefined' ?
this.playlists.length :
Math.floor(this.pane_height / this.line_height);
let items_per_page = this.items_per_column * this.column_count;
let pagecount = Math.ceil(this.playlists.length / items_per_page);
if (this.page >= pagecount) {
this.page = pagecount - 1;
}
if (this.page < 0) {
this.page = 0;
}
$('#playlistpages').text(pagecount);
$('#playlistpage').text(this.page + 1);
let start = this.page * items_per_page;
let dialog = $('#playlistlist');
dialog.text('');
for (let i = 0; i < items_per_page; i++) {
let newitem = $('<li/>');
dialog.append(newitem);
if (start < this.playlists.length) {
let playlist = this.playlists[start];
let rater = this.createRater(playlist);
let checkbox = $('<input/>');
checkbox.prop('checked', playlist.mix).
attr('type', 'checkbox').
attr('onclick', 'client.playlistview.togglePlaylist ("' + playlist.id + '")').
attr('id', 'playlistmix' + playlist.id);
let label = $('<label/>');
label.text(playlist.name).
attr('onclick', 'client.playlistview.selectPlaylist ("' + playlist.id + '")').
attr('for', 'playlistmix' + playlist.id);
newitem.append(rater).append(checkbox).append(label);
start++;
}
else {
newitem.html('&nbsp;');
}
}
}
changeHandler(reason) {
if (reason != Pianod.MSG.MIX_CHANGED) {
this.execute(['PLAYLIST', 'LIST'], this.updatePlaylists, null);
}
this.execute(['MIX', 'LIST', 'INCLUDED'], this.updateMix, null);
}
refreshMix() {
$('#selectmode').toggle(this.shown_mode != 'playlist' &&
(this.shown_mode != this.current_mode || this.queue_mode != Pianod.MSG.RANDOMPLAY));
$('#playlistlist').toggleClass('showmix', this.shown_mode == 'mix');
$('#playlistlist').toggleClass('showratings', this.shown_mode == 'auto');
$('#showmode').toggleClass('selected', this.current_mode == this.shown_mode);
$('#showmode').toggleClass('pending', this.current_mode != this.shown_mode);
let lh = (this.shown_mode == 'auto' ? 1.5 : 1.0);
if (lh != this.line_height) {
this.line_height = lh;
this.refresh();
}
}
getWidth(total, available, height) {
if (total == 1) {
this.pane_height = undefined;
this.refresh();
return 1;
}
height -= PlaylistView.reserved_lines;
this.pane_height = height > 0 ? height : 1;
this.items_per_column = Math.floor(this.pane_height / this.line_height);
this.column_count = Math.ceil(this.playlists.length / this.items_per_column);
if (this.column_count === 0) {
this.column_count = 1;
}
if (this.column_count > available) {
if (available == total && available > 1) {
this.column_count = available - 1;
}
else {
this.column_count = available;
}
}
this.refresh();
return this.column_count;
}
previous() {
this.page--;
this.refresh();
}
next() {
this.page++;
this.refresh();
}
selectPlaylist(id) {
if (this.shown_mode == 'playlist') {
this.execute(['PLAY', 'PLAYLIST', 'ID', id]);
View.showView('track');
}
}
togglePlaylist(id) {
if (this.shown_mode == 'mix') {
this.execute(['MIX', 'TOGGLE', 'ID', id]);
}
}
ratePlaylist(id, rating) {
if (typeof (rating) == "number") {
assert(rating > 0 && rating <= 5.0);
rating = rating.toString();
}
this.execute(['RATE', 'PLAYLIST', rating, 'ID', id]);
}
changeShownMode() {
this.shown_mode = View.getSelectedValue('#showmode');
this.refreshMix();
}
selectMode() {
this.execute(['PLAY', this.shown_mode]);
View.showView('track');
}
subscribeToPlaylists(callback) {
this.subscriptions.push(callback);
callback(this.playlists);
}
mixSelected() {
return this.have_mix_selections;
}
changeRooms() {
this.changeHandler(Pianod.MSG.PLAYLISTS_CHANGED);
this.execute('STATUS');
}
}
PlaylistView.reserved_lines = 3;
;
export class SearchView extends TableView {
constructor(name, piano) {
super(name, piano, {
min_panes: 2,
max_panes: 4,
min_height: 7,
reserved_lines: 5,
anchor: $('#searchresults'),
extra_fields: ['action']
});
View.getView('playlist').subscribeToPlaylists(this.updatePlaylists);
$('#searchfor').enterclicks('#searchbutton');
}
getPredicate() {
let method = View.getSelectedValue('#searchmethod');
let query = View.getInputText('#searchfor');
if (method == 'expr') {
return query;
}
let querytype = View.getSelectedValue('#searchtype');
if (querytype == 'request') {
querytype = 'any';
}
if (method == 'like') {
return [querytype, 'LIKE', query];
}
return (querytype == 'any' ? 'search' : querytype) + ' =~ "' + query.replace(/"/g, '""') + '"';
}
appendPredicate(command, predicate) {
command = command.slice(0);
if (predicate instanceof Array) {
return command.concat(predicate);
}
return Pianod.assembleCommand(command, 'WHERE ' + predicate);
}
performQuery(command, predicate, partialflag = false) {
let request = this.appendPredicate(command, predicate);
this.executeControlled(request, (response) => {
if (response.ok) {
this.clear();
this.last_query = predicate;
this.render(response.record);
if (partialflag) {
this.error('INFO_SEARCH_RESULTS_OMITTED', this.partial_reason);
}
}
else if (!partialflag && command.length >= 3 && command[2] == 'AUTHORITATIVE') {
this.partial_reason = translate(response);
command[2] = 'DISCRETIONARY';
this.performQuery(command, predicate, true);
}
else {
this.commError(response);
}
});
}
updatePlaylists(playlists) {
let list = $('#addfromsearchtarget');
list.find("option").slice(1).remove();
for (let p of playlists) {
list.append($('<option />').prop('value', p.id)
.text(p.unique_name));
}
}
performMenuAction(command, success_message) {
let cmd = command.slice(0);
if (typeof (this.menu_item) != 'undefined') {
cmd.push('ID', this.menu_item['id']);
}
else {
cmd = this.appendPredicate(cmd, this.last_query);
}
this.execute(cmd, success_message);
}
search() {
if (View.getInputText('#searchfor') === '') {
this.error('ERROR_NEED_SEARCH_QUERY');
return;
}
let command = ['FIND', 'SUGGESTION', 'AUTHORITATIVE'];
if ($('#searchtype').val() == 'request') {
command = ['SONG', 'LIST'];
}
this.performQuery(command, this.getPredicate());
}
selectMethod() {
let show = $('#searchmethod').val() != 'expr';
$('#searchtype').toggle(show);
$('label[for=searchmethod]').toggle(show);
}
request() {
if (typeof (this.last_query) == 'undefined') {
this.error('ERROR_NEED_SEARCH_QUERY');
return;
}
this.execute(this.appendPredicate(['SONG', 'REQUEST'], this.last_query), 'CONFIRM_QUEUED');
}
playTransient() {
if (typeof (this.last_query) == 'undefined') {
this.error('ERROR_NEED_SEARCH_QUERY');
return;
}
this.execute(this.appendPredicate(['PLAY', 'FROM'], this.last_query));
}
showSeedMenu(item) {
this.menu_item = item;
$('#addfromsearchname').text(this.menu_item.name);
$('#addfromsearchtype').text(translate('Cap_' + this.menu_item.type));
$('#searchinfosource').text(this.menu_item['source']);
$('#searchinfoname').text(this.menu_item['sourcename']);
$('#createfromsearchtype').hide();
$('#searchaction').show();
if (!this.isShown) {
this.show();
}
}
showAllResultsMenu() {
if (typeof (this.last_query) == 'undefined') {
this.error('ERROR_NEED_SEARCH_QUERY');
return;
}
delete this.menu_item;
$('#addfromsearchname').text(translate(''));
$('#addfromsearchtype').text(translate('MESSAGE_CREATE_WITH_ALL_RESULTS'));
$('#searchinfosource').text('');
$('#searchinfoname').text(translate('MESSAGE_CREATE_APPLIES_TARGET'));
$('#createfromsearchtypestandard').prop('checked', 'checked');
$('#createfromsearchtype').show();
$('#searchaction').show();
}
addSeed() {
let playlist = View.getInputText('#addfromsearchtarget');
if (playlist === '') {
alert(translate('ERROR_CHOOSE_PLAYLIST'));
}
else {
let playlistname = $('#addfromsearchtarget option:selected').text();
let name = (this.menu_item || {}).name || translate('MESSAGE_CREATE_WITH_ALL_RESULTS');
this.performMenuAction(['PLAYLIST', 'MODIFY', 'ID', playlist, 'ADD', 'SEED',
'DISCRETIONARY'], translate('CONFIRM_ADDED_TO', name, playlistname));
$('#searchaction').hide();
}
}
createPlaylist() {
let playlist = View.getInputText('#createfromsearchplaylist');
let targetsource = View.getSelectedValue('#createfromsearchsource');
if (playlist === '') {
alert(translate('ERROR_NEED_PLAYLIST_NAME'));
}
else {
let cmd = ['WITH', 'SOURCE', 'ID', targetsource, 'PLAYLIST', 'CREATE', 'NAME', playlist, 'FROM'];
if (typeof (this.menu_item) == 'undefined') {
let type = $('input[name="createfromsearchtype"]:checked').val();
if (type == 'smart') {
cmd.splice(6, 0, 'SMART');
}
}
this.performMenuAction(cmd, translate('CONFIRM_CREATE', playlist));
$('#searchaction').hide();
}
}
research(kind, value) {
let comparator = (kind == 'artist' ? '=~' : '==');
let kind_clause = (kind == 'artist' ?
'(TYPE=ARTIST|(!COMPILATION&TYPE=ALBUM)|(COMPILATION&TYPE=SONG))' :
'(TYPE=' + kind + '|TYPE=SONG)');
this.performQuery(['FIND', 'ALL', 'AUTHORITATIVE'], kind_clause + ' && ' + kind + comparator + '"' + value.replace(/"/g, '""') + '"');
if (!this.isShown()) {
this.show();
}
}
}
;
export class SeedView extends TableView {
constructor(name, piano) {
super(name, piano, {
min_panes: 2,
max_panes: 3,
min_height: 5,
reserved_lines: 4,
anchor: $('#seedsexisting'),
fields: ['artist', 'album', 'song', 'seedtype'],
extra_fields: ['action']
});
this.playlists = [];
this.updatePlaylists = (playlists) => {
let selected = View.getSelectedValue('#reviseplaylist');
this.playlists = playlists;
let list = $('#reviseplaylist');
list.find("option").slice(1).remove();
for (let playlist of playlists) {
list.append($('<option />').prop('value', playlist.id)
.text(playlist.unique_name));
}
let lost = true;
if (selected != '') {
let reselected = list.find('option[value=' + selected + ']');
lost = (reselected.length == 0);
if (!lost) {
reselected.prop('selected', 'selected');
}
}
if (lost) {
this.render([]);
}
};
View.getView('playlist').subscribeToPlaylists(this.updatePlaylists);
}
static getSeedType(item) {
for (let kind in SeedView.seed_kinds) {
if (item.rating[kind])
return kind;
}
return "rating";
}
removeSeed(item) {
if (confirm(translate('PROMPT_CONFIRM_DELETE_SEED', item.name))) {
let type = SeedView.getSeedType(item);
if (type == 'rating') {
this.execute(['RATE', 'SONG', 'NEUTRAL', 'ID', item.id], 'Rating removed');
}
else {
this.execute(['SEED', 'DELETE', 'ID', item.id, 'FROM', 'PLAYLIST', 'ID', this.shown_playlist.id], translate('CONFIRM_DELETE', item.name));
}
this.selectPlaylist();
}
}
renderAction(item) {
return this.createButton(item, 'UNSEED', this.removeSeed);
}
getSelectedPlaylist() {
let id = View.getSelectedValue('#reviseplaylist');
if (id === '') {
this.error('ERROR_SELECT_PLAYLIST');
return null;
}
for (let p of this.playlists) {
if (p.id == id) {
return p;
}
}
assert(!"Selected playlist not found.");
return null;
}
selectPlaylist() {
let playlist = this.getSelectedPlaylist();
if (playlist !== null) {
this.execute(['SEED', 'LIST', 'PLAYLIST', 'ID', playlist.id], (response) => {
this.shown_playlist = playlist;
for (let seed of response.record) {
let type = SeedView.getSeedType(seed);
if (type == "rating")
type += " (" + seed['rating'].rated + ")";
seed['seedtype'] = type;
}
this.render(response.record);
});
}
else {
this.render([]);
}
}
showRenamePlaylist() {
let playlist = this.getSelectedPlaylist();
if (playlist !== null) {
this.renaming_playlist = playlist;
$('#renameplaylistsourcetype').text(playlist.source);
$('#renameplaylistsourcename').text(playlist.sourcename);
$('#renameplaylistoldname').val(playlist.name);
$('#renameplaylistnewname').val(playlist.name);
$('#renameplaylist').show();
}
}
renamePlaylist() {
let new_name = View.getInputText('#renameplaylistnewname');
if (new_name == '') {
this.error('PLAYLIST_NAME');
}
else {
this.execute(['PLAYLIST', 'RENAME', 'ID', this.renaming_playlist.id, 'TO', new_name], translate('CONFIRM_RENAME', this.renaming_playlist.name, new_name));
$('#renameplaylist').hide();
}
}
removePlaylist() {
let playlist = this.getSelectedPlaylist();
if (playlist !== null) {
if (confirm(translate('PROMPT_CONFIRM_DELETE', playlist.name))) {
this.execute(['PLAYLIST', 'DELETE', 'ID', playlist.id], translate('CONFIRM_DELETE', playlist.name));
this.render([]);
}
}
}
}
SeedView.seed_kinds = {
seed: 'song',
albumseed: 'album',
artistseed: 'artist'
};
export class SourceView extends CommView {
constructor(name, piano) {
super(name, piano);
this.current = '1';
this.borrowable = [];
this.my_sources = [];
this.sources = [];
this.subscriptions = [];
this.refresh = (sources) => {
View.renderOptions('#sourcepicker', sources, 'id', 'fullname', 0);
};
this.sourcesChanged = () => {
this.execute(['SOURCE', 'LIST', 'ENABLED'], (response) => {
let sources = SourceView.extractSourceList(response);
for (let sub of this.subscriptions) {
sub(sources);
}
if (sources.length <= 1) {
this.show();
this.error('ERROR_NO_SOURCES');
}
}, null);
};
piano.subscribe(Pianod.MSG.SOURCES_CHANGED, this.sourcesChanged);
this.subscriptions.push(this.refresh);
piano.subscribe(Pianod.MSG.SELECTEDSOURCE, (msg) => {
this.current = msg.words[0];
$('#sourcepicker').val(this.current);
});
this.execute(['SOURCE', 'LIST', 'TYPE'], (response) => {
let list = $('#addsourcetype');
list.find("option").slice(1).remove();
for (let t of response.getSortedColumn('source')) {
list.append($('<option/>').text(t)
.prop('disabled', t == 'manager'));
}
}, null);
this.sourcesChanged();
this.selectAddFields();
this.selectBorrowFields();
}
static extractSourceList(response) {
let sources = response.sortOnFields(['source', 'sourcename']);
for (let src of sources) {
src.fullname = src.source + ' (' + src.sourcename + ')';
}
return sources;
}
toggleParameters(names, borrowaction, show) {
let short_prefix = (borrowaction ? 'borrowsource' : 'addsource');
let prefix = '#' + short_prefix;
let any_exist = false;
for (let field of names) {
$(prefix + 'parameters label[for=' + short_prefix + field + ']').toggle(show);
$(prefix + 'parameters label[for=' + short_prefix + field + '_enable]').toggle(show);
let enabler = $(prefix + field + '_enable');
enabler.toggle(show);
let enabled = (enabler.length < 1 || enabler.prop('checked'));
let widget = $(prefix + field);
any_exist = any_exist || (widget.length > 0 && enabled);
widget.toggle(show && enabled);
}
return any_exist;
}
selectAddFields() {
let choice = View.getSelectedValue('#addsourcetype');
for (let source in SourceView.SOURCEFIELDS) {
this.toggleParameters(SourceView.SOURCEFIELDS[source], false, false);
}
if (choice !== '') {
this.toggleParameters(SourceView.SOURCEFIELDS[choice], false, true);
}
$('#addsourcesongproxy option[value=donor]').prop('disabled', choice == 'pandora');
}
selectBorrowFields() {
let list = $('#borrowlist');
let index = list.prop('selectedIndex');
for (let source in SourceView.SOURCEFIELDS) {
this.toggleParameters(SourceView.SOURCEFIELDS[source], true, false);
}
if (index > 0) {
let item = this.borrowable[index - 1];
this.toggleParameters(SourceView.SOURCEFIELDS[item.source], true, true);
$('#borrowsourcesongproxy option[value=donor]').prop('disabled', item.source == 'pandora');
}
}
addSourceParameters(source, borrowaction, request) {
let prefix = borrowaction ? '#borrowsource' : '#addsource';
let fields = SourceView.SOURCEFIELDS[source];
for (let field of fields) {
let behaviors = (field in SourceView.FIELDS ? SourceView.FIELDS[field] : []);
if (borrowaction && $.inArray('borrow', behaviors) < 0)
continue;
let enabler = $(prefix + field + '_enable');
let enabled = (enabler.length < 1 || enabler.prop('checked'));
if (!enabled)
continue;
let domname = prefix + field;
let value;
if ($.inArray('check', behaviors) >= 0) {
value = View.isInputChecked(domname);
}
else {
if ($.inArray('text', behaviors) >= 0) {
value = View.getInputText(domname);
}
else if ($.inArray('number', behaviors) >= 0) {
value = View.getInputNumber(domname);
}
else if ($.inArray('select', behaviors) >= 0) {
value = View.getSelectedValue(domname);
}
else {
assert(false);
value = $('#' + domname).val();
}
if (value == '' && $.inArray('optional', behaviors) >= 0) {
continue;
}
if (value == '' && $.inArray('null', behaviors) < 0) {
this.error(translate('ADDSOURCE_NULL_' + field.toUpperCase()));
return false;
}
}
switch (field) {
case 'cachesize':
if (value)
request.push('CACHE', 'MINIMUM', value, 'MAXIMUM', Math.floor(value * 1.2));
break;
case 'channelize':
if (value)
request.push('DISCRETE', 'CHANNELS');
break;
case 'pandoraplus':
request.push('ACCOUNT', 'TYPE', value);
break;
case 'recentbias':
request.push('RECENT', 'BIAS', value);
break;
case 'ratingbias':
request.push('RATING', 'BIAS', value);
break;
case 'user':
case 'rescan':
request.push(field);
default:
request.push(value);
break;
}
}
let persistence = $(prefix + 'persistence').val();
if (persistence != '') {
request.push(persistence);
}
let access = $(prefix + 'access').val();
if (access !== '') {
request.push('ACCESS', access);
}
let proxy = $(prefix + 'songproxy').val();
if (proxy != '') {
request.push('SONG', 'PROXY', proxy);
}
return true;
}
addSource() {
let choice = View.getSelectedValue('#addsourcetype');
if (choice == '') {
this.error('ERROR_NEED_SOURCE_TYPE');
return;
}
let request = [choice];
if (choice == 'tonegenerator') {
request.push('ACTIVATE');
}
else if (choice == 'filesystem') {
request.push('ADD');
}
if (!this.addSourceParameters(choice, false, request))
return;
let name = View.getInputText('#addsourcename');
if (name !== "") {
request.push('NAME', name);
}
View.showView('track');
this.execute(request, 'CONFIRM_SOURCE_ADDED');
}
borrowSource() {
let list = $('#borrowlist');
let index = list.prop('selectedIndex');
if (index <= 0) {
this.error('ERROR_NEED_SOURCE_SELECTION');
}
else {
let item = this.borrowable[index - 1];
let request = [item.source, 'USE', item.sourcename];
if (this.addSourceParameters(item.source, true, request)) {
this.execute(request, 'CONFIRM_SOURCE_ADDED');
$('#borrowsourceparameters select').prop('selectedIndex', 0);
this.selectBorrowFields();
}
}
}
selectSource() {
let source = View.getSelectedValue('#sourcepicker');
this.execute(['SOURCE', 'SELECT', 'ID', source], 'CONFIRM_SOURCE_SELECTED');
}
removeSource() {
this.execute(['SOURCE', 'DISCONNECT', 'ID', this.current], 'CONFIRM_SOURCE_REMOVED');
this.closeAccordion();
}
open_forgetsource() {
this.execute(['SOURCE', 'LIST', 'MINE'], (response) => {
this.my_sources = SourceView.extractSourceList(response);
View.renderOptions('#sourceforgetlist', this.my_sources, null, 'fullname');
});
}
forgetSource() {
let list = $('#sourceforgetlist');
let index = list.prop('selectedIndex');
if (index <= 0) {
this.error('ERROR_NEED_SOURCE_SELECTION');
}
else {
let item = this.my_sources[index - 1];
if (confirm(translate('PROMPT_CONFIRM_DELETE', item.fullname))) {
this.execute(['SOURCE', 'FORGET', 'TYPE', item.source, 'NAME', item.sourcename], translate('CONFIRM_DELETE', item.fullname));
this.closeAccordion();
}
}
}
open_borrowsource() {
this.execute(['SOURCE', 'LIST', 'AVAILABLE'], (response) => {
this.borrowable = SourceView.extractSourceList(response);
View.renderOptions('#borrowlist', this.borrowable, null, 'fullname');
});
}
subscribeToSources(callback) {
this.subscriptions.push(callback);
callback(this.sources);
}
}
SourceView.SOURCEFIELDS = {
pandora: ['user', 'password', 'pandoraplus', 'cachesize'],
filesystem: ['path', 'rescan', 'ratingbias', 'recentbias'],
tonegenerator: ['ratingbias', 'recentbias', 'channelize']
};
SourceView.FIELDS = {
'cachesize': ['borrow', 'optional', 'select'],
'channelize': ['check'],
'pandoraplus': ['borrow', 'optional', 'select'],
'user': ['text'],
'password': ['text'],
'path': ['text'],
'rescan': ['borrow', 'optional', 'select'],
'ratingbias': ['borrow', 'number'],
'recentbias': ['borrow', 'number']
};
;
export class TrackView extends CommView {
constructor(name, piano) {
super(name, piano);
this.shown_index = 0;
this.start_time = 0;
this.duration = 0;
this.active_track = false;
this.stalled = false;
this.paused = false;
this.queue_mode = Pianod.MSG.STOPPED;
this.playlist_name = "";
this.playlist_type = "";
this.have_playlists = false;
this.have_sources = false;
this.updatePlaylists = (playlists) => {
this.have_playlists = (playlists.length !== 0);
let seedtarget = $('#addfromtracktarget');
seedtarget.find("option").slice(1).remove();
let playselect = $('#playlists');
playselect.html('');
for (let i = 0; i < playlists.length; i++) {
let option = $('<option />').prop('value', playlists[i].id)
.text(playlists[i].unique_name);
seedtarget.append($(option));
playselect.append(option.clone());
}
if (!this.have_playlists) {
let opt = '<option disabled>' + translate('SELECT_PLAYLISTS_NONE') + '</option>';
seedtarget.append(opt);
playselect.html(opt);
}
this.setControlVisibility();
};
this.updateSources = (sources) => {
this.have_sources = (sources.length > 1);
let target = $('#addfromtracksource');
View.renderOptions(target, sources, 'id', 'fullname', 0);
target.find("option[value=1]").remove();
if (!this.have_sources) {
target.append($('<option/>').prop('disabled', 'disabled').text(translate('ERROR_NO_USABLE_SOURCES')));
}
let items = target.find('option').clone();
target = $('#createfromsearchsource');
target.find('option').remove();
target.append(items);
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
this.updateTrackControls = (msg) => {
let code = msg.code;
this.active_track = (code == Pianod.MSG.PLAYING || code == Pianod.MSG.PAUSED ||
code == Pianod.MSG.STALLED);
this.stalled = (code == Pianod.MSG.STALLED);
this.paused = (code == Pianod.MSG.PAUSED);
if (code == Pianod.MSG.TRACKCOMPLETE) {
if (this.shown_index === 0) {
this.setShownTrack(null);
}
}
else {
let pp = $('.playpause');
pp.toggleClass('disabled', !this.active_track);
pp.html(code == Pianod.MSG.PAUSED || code == Pianod.MSG.IDLE ? '▶' : '&nbsp;||&nbsp;');
this.setControlVisibility();
}
if (code == Pianod.MSG.PLAYING) {
if (typeof (this.interval) == 'undefined') {
this.interval = setInterval(this.updateTrackTime, 1000);
}
}
else {
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
this.interval = undefined;
}
}
};
$('#trackrating').rating(0.0).click(() => {
assert(typeof (this.shown_track) != 'undefined');
let rating = $('#trackrating').rating();
this.execute(['RATE', 'SONG', rating.toString(), 'ID', this.getShownId()], (response) => {
if (this.shown_index !== 0) {
this.goToIndex(this.shown_index);
}
});
});
this.setShownTrack(null);
this.selectAddTarget();
piano.subscribe([Pianod.MSG.PLAYING, Pianod.MSG.PAUSED, Pianod.MSG.STALLED, Pianod.MSG.IDLE,
Pianod.MSG.TRACKCOMPLETE], this.updateTrackControls, false);
piano.subscribe([Pianod.MSG.PLAYING, Pianod.MSG.PAUSED, Pianod.MSG.STALLED], this.parseTrackTime, false);
piano.subscribe(Pianod.MSG.TRACKCOMPLETE, (msg) => {
if (this.shown_index !== 0) {
this.shown_index--;
this.shownIndexChanged();
}
}, false);
piano.subscribe(Pianod.MSG.SELECTEDPLAYLIST, (msg) => {
this.playlist_type = msg.words[0];
this.playlist_name = msg.words.slice(1).join(' ');
this.setControlVisibility();
});
View.getView('queue').subscribeToCurrentSong((song) => {
this.active_track = true;
if (this.shown_index == 0) {
this.setShownTrack(song);
}
});
piano.subscribe([Pianod.MSG.STOPPED, Pianod.MSG.REQUESTS, Pianod.MSG.RANDOMPLAY], (msg) => {
this.queue_mode = msg.code;
this.setControlVisibility();
}, false);
piano.subscribe([Pianod.MSG.VOLUME], (msg) => {
$('#volume').val(msg.value);
});
piano.subscribe(Pianod.MSG.QUEUERANDOMIZE, (msg) => {
$('#queuerandomize').val(msg.value);
});
View.getView('playlist').subscribeToPlaylists(this.updatePlaylists);
View.getView('source').subscribeToSources(this.updateSources);
}
setRating(rating, flags) {
$('#trackrating').rating(rating);
$('#trackname .button.seed').toggleClass('selected', flags.seed || false);
$('#albumname .button.seed').toggleClass('selected', flags.albumseed || false);
$('#artistname .button.seed').toggleClass('selected', flags.artistseed || false);
}
setActions(actions) {
$('#overplayed').toggleClass('disabled', !(actions.rate || false));
$('#trackrating').toggleClass('disabled', !(actions.rate || false));
$('#trackname .button.seed').toggleClass('disabled', !(actions.seed || false));
$('#albumname .button.seed').toggleClass('disabled', !(actions.albumseed || false));
$('#artistname .button.seed').toggleClass('disabled', !(actions.artistseed || false));
}
setShownTrack(track) {
this.shown_track = track;
if (track === null) {
track = new Pianod.MusicData();
}
$('#albumname .value').text(track.album || '');
$('#artistname .value').text(track.artist || '');
$('#trackname .value').text(track.song || '');
$('#songplaylist .value').text(track.playlist || '');
$('#albumart').attr('src', track.art || 'no-art.jpeg');
let rating = track.rating || { rated: 0.0 };
this.setRating(rating.rated, rating);
this.setActions(track.action || {});
let ai = $('#additionalinfo');
let url = track.url || '';
if (url == '') {
ai.removeAttr('href');
ai.attr('onclick', "client.trackview.moreInfo('song');");
}
else {
ai.attr('href', url);
ai.removeAttr('onclick');
}
this.shownIndexChanged();
}
getShownId() {
assert(this.shown_track !== null);
return (this.shown_track).id;
}
setControlVisibility() {
let current = (this.shown_index === 0);
let idle = current && !this.active_track;
if (idle) {
$('#albumart').attr('src', 'no-art.jpeg');
}
$('#trackdetails').css('visibility', this.active_track || !current ? 'visible' : 'hidden');
$('#controller').css('visibility', this.active_track || !current ? 'visible' : 'hidden');
$('#statusbar').css('visibility', current && this.active_track ? 'visible' : 'hidden');
$('#controller .time').css('visibility', this.active_track ? 'visible' : 'hidden');
$('#networkstatus').css('visibility', current && this.active_track && this.stalled ? 'visible' : 'hidden');
$('#pausestatus').css('visibility', current && this.active_track && this.paused ? 'visible' : 'hidden');
$('#stoppedstatus').css('visibility', idle && this.queue_mode == Pianod.MSG.STOPPED ? 'visible' : 'hidden');
$('#norequests').css('visibility', idle && this.queue_mode == Pianod.MSG.REQUESTS ? 'visible' : 'hidden');
$('#needplaylists').css('visibility', idle && this.queue_mode == Pianod.MSG.RANDOMPLAY && !this.have_playlists ?
'visible' : 'hidden');
let mix = idle && this.queue_mode == Pianod.MSG.RANDOMPLAY && this.have_playlists;
let mixPopulated = this.playlist_type != 'mix' ||
View.getView('playlist').mixSelected();
$('#needmixselections').css('visibility', mix && !mixPopulated ? 'visible' : 'hidden');
$('#go').css('visibility', mix && mixPopulated ? 'visible' : 'hidden');
let name = this.playlist_name;
if (this.queue_mode == Pianod.MSG.STOPPED) {
name = 'Stopped';
}
else if (this.queue_mode == Pianod.MSG.REQUESTS) {
name = 'Requests only';
}
else if (this.playlist_type == 'auto') {
name = translate('INDICATOR_AUTOTUNING') + name;
}
$('#playlistname option:first').text(name);
}
shownIndexChanged() {
$('#nowplaying').toggle(this.shown_index !== 0);
$('#playlistname').toggle(this.shown_index === 0);
let message = (this.shown_index < 0 ? 'RECUE' : this.shown_index > 0 ? 'DEQUEUE' : 'SKIP');
$('#skiprecuetrack').toggleClass('disabled', this.shown_track === null).
text(translate('ACTION_' + message)).
attr('title', translate('TOOLTIP_' + message));
this.setControlVisibility();
}
goToIndex(index) {
$('#skiprecuetrack').addClass('disabled');
let song = View.getView('queue').getSong(index, (updated_song) => {
if (index == this.shown_index) {
this.setShownTrack(updated_song);
}
});
if (song !== null || index == 0) {
if (song === null)
this.success(null, 'STATUS_NOTHING_PLAYING');
this.shown_index = index;
this.setShownTrack(song);
}
else {
this.error('ERROR_QUEUEHISTORY_RANGE');
}
}
selectAddTarget() {
let new_playlist = $('#addfromtracktarget').val() === '';
let action = new_playlist ? 'PROMPT_CREATE_PLAYLIST_FROM' : 'PROMPT_ADD_SEED_FROM';
$('#addfromtracknewplaylist').toggle(new_playlist);
for (let type of ['song', 'album', 'artist']) {
$('#doaddseedfrom' + type).text(translate(action, translate('lower_' + type)));
}
}
destroy() {
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
}
super.destroy();
}
goToPreviousTrack() {
this.clear();
this.goToIndex(this.shown_index - 1);
}
goToNextTrack() {
this.clear();
this.goToIndex(this.shown_index + 1);
}
goToCurrentTrack() {
this.clear();
this.goToIndex(0);
}
toggleSeed(type) {
this.execute(['SEED', 'TOGGLE', 'TYPE', type, 'ID', this.getShownId()], (response) => {
this.goToIndex(this.shown_index);
});
}
moreInfo(kind) {
View.getView('search').research(kind, this.shown_track[kind]);
View.showView('search');
}
setVolume() {
let volume = View.getInputText('#volume');
this.execute(['VOLUME', 'LEVEL', volume]);
}
showSeeds() {
let tv_playlist = $('#songplaylist .value').text();
$('#reviseplaylist option').filter(function () {
return $(this).text() == tv_playlist;
}).prop('selected', true);
View.showView('seed');
View.getView('seed').selectPlaylist();
}
showArt() {
let art = $('#albumart').attr('src');
assert(typeof (art) == 'string');
$("<img/>").attr('src', art).on('load', null, function () {
window.open(art, '_blank', 'status=0,menubar=0,scrollbars=0,resizable=0,location=0,toolbar=0,width=' + this['width'] + ',height=' + this['height']);
}).on('error', () => {
this.error('Error zooming album art.');
});
}
rateOverplayed(rating) {
assert(typeof (this.shown_track) != 'undefined');
this.execute(['RATE', 'SONG', 'OVERPLAYED', 'ID', this.getShownId()], 'CONFIRM_RATE_OVERPLAYED');
}
explainSong() {
assert(typeof (this.shown_track) != 'undefined');
this.execute(['EXPLAIN', 'SONG', this.getShownId()], (response) => {
assert(response.record.length == 1);
alert(response['explanation']);
});
}
addFromTrack(type) {
assert(typeof (this.shown_track) != 'undefined');
let target = $('#addfromtracktarget');
assert(this.shown_track != null);
let item = this.shown_track;
let choice = target.val();
let name, command, message;
if (choice === '') {
name = View.getInputText('#addfromtrackplaylist');
let source = $('#addfromtracksource').val();
if (name === '') {
this.error('ERROR_NEED_PLAYLIST_NAME');
return;
}
command = ['WITH', 'SOURCE', 'ID', source, 'PLAYLIST', 'CREATE', 'NAME', name,
'FROM', type, 'ID', item.id];
message = translate('CONFIRM_CREATE', name);
}
else {
command = ['SEED', 'ADD', 'TYPE', type, 'ID', item.id,
'TO', 'PLAYLIST', 'ID', choice];
message = translate('CONFIRM_ADD_SEED_TYPE_TO', type, target.find(':selected').text());
}
this.execute(command, (response) => {
this.success(response, message);
$('#addplaylistfromtrack').hide();
target.val("");
$('#addfromtrackplaylist').val("");
this.selectAddTarget();
if (!('playlist' in item)) {
this.goToIndex(this.shown_index);
}
});
}
selectPlaylist() {
let choice = View.getSelectedValue('#playlistname');
if (choice == 'everything' || choice == 'mix' || choice == 'auto' || choice == "request") {
this.execute(['PLAY', choice]);
}
else if (choice == 'stop') {
this.execute('STOP');
}
else if (choice === '') {
return;
}
else {
this.execute(['PLAY', 'PLAYLIST', 'ID', choice]);
}
$('#playlistname option:first').prop('selected', true);
}
skipOrRecue() {
assert(this.shown_track !== null);
let id = this.getShownId();
if (this.shown_index == 0) {
this.execute('SKIP');
}
else if (this.shown_index < 0) {
this.execute(['REQUEST', 'ID', id], 'CONFIRM_QUEUED');
}
else {
this.execute(['REQUEST', 'CANCEL', 'ID', id], 'CONFIRM_DEQUEUED');
}
$('#trackactions').hide();
}
clearQueue() {
this.execute(['request', 'clear'], "CONFIRM_CLEAR_REQUESTS");
$('#trackactions').hide();
}
selectQueueRandomize() {
let method = View.getSelectedValue('#queuerandomize');
this.execute(['QUEUE', 'RANDOMIZE', 'BY', method]);
$('#trackactions').hide();
}
setCrossfadeDuration() {
let duration = View.getInputNumber('#crossfadeduration');
$('#crossfadeseconds').text(duration);
this.execute(['CROSSFADE', 'DURATION', duration]);
}
setCrossfadeLevel() {
let level = View.getInputNumber('#crossfadelevel');
$('#crossfadedecibels').text(level);
this.execute(['CROSSFADE', 'LEVEL', level]);
}
selectRoom() {
let room = View.getSelectedValue('#rooms');
this.execute(['ROOM', 'ENTER', room]);
$('#trackactions').hide();
}
showActionPopup() {
$('#trackactions').show();
this.execute(['CROSSFADE', 'DURATION'], (response) => {
$('#crossfadeseconds').text(response['info']);
$('#crossfadeduration').val(response['info']);
}, 'Cannot retrieve crossfade duration');
this.execute(['CROSSFADE', 'LEVEL'], (response) => {
$('#crossfadedecibels').text(response['info']);
$('#crossfadelevel').val(response['info']);
}, 'Cannot retrieve crossfade duration');
this.execute(['ROOM', 'LIST'], (response) => {
$('#roomselector').toggle(response.ok && response.record.length > 1);
let current = View.getSelectedValue('#rooms');
$('#rooms').text('');
for (let room of response.record) {
let li = $('<option />').prop('value', room.room).text(room.room);
$('#rooms').append(li);
}
$('#rooms').val(current);
}, 'Can not retrieve room list');
}
}
export class UserView extends CommView {
validatedNewPassword(input1, input2) {
let password = View.getInputText(input1);
let confirmation = View.getInputText(input2);
if (password == confirmation) {
return (password);
}
this.error('ERROR_PASSWORDS_UNMATCHED');
return undefined;
}
constructor(name, piano) {
super(name, piano);
}
changePassword() {
let shadow = $('#shadowpassword').prop('checked');
let oldpw = $('#oldpassword').val();
let cmd;
if (shadow) {
cmd = ['SET', 'SHADOW', 'PASSWORD', oldpw, $('#newpassword').val()];
}
else {
let newpw = this.validatedNewPassword('#newpassword', '#confirmpassword');
if (typeof (newpw) != 'undefined') {
cmd = ['SET', 'PASSWORD', oldpw, newpw];
}
}
if (typeof (cmd) != 'undefined') {
this.execute(cmd, 'CONFIRM_PASSWORD_CHANGE');
}
}
changeShadow() {
let shadow = $('#shadowpassword').prop('checked');
$('label[for=oldpassword]').text(translate(shadow ? 'PROMPT_PIANOD_PASSWORD' : 'PROMPT_OLD_PASSWORD'));
$('label[for=newpassword]').text(translate(shadow ? 'PROMPT_SYSTEM_PASSWORD' : 'PROMPT_NEW_PASSWORD'));
$('#confirmpassword').toggle(!shadow);
$('label[for=confirmpassword]').toggle(!shadow);
}
createUser() {
let user = View.getInputText('#createusername');
let type = View.getSelectedValue('#createusertype');
let pass = this.validatedNewPassword('#createpassword', '#createconfirmpassword');
if (typeof (pass) != 'undefined') {
this.execute(['CREATE', type, user, pass], translate('CONFIRM_USER_UPDATED', user));
}
}
open_alter_user() {
this.execute(['USERS', 'LIST'], (response) => {
if (response.ok) {
let users = response.getSortedColumn('id');
let selector = $("#alterusername");
selector.find("option").slice(1).remove();
for (let user of users) {
selector.append($('<option />').text(user).
attr('value', user));
}
}
else {
this.error('ERROR_CANNOT_GET_USER_LIST');
}
});
}
selectAlterUser() {
let user = View.getSelectedValue('#alterusername');
if (user !== '') {
this.execute(['USERS', 'LIST', user], (response) => {
if (response.ok && response.data.length == 1) {
let privdata = response.data[0][Pianod.MSG.PRIVILEGES];
$('#alterusertype').val(privdata.words[0]);
let flags = privdata.flags;
for (let p of ['influence', 'service']) {
$('#privilege' + p).prop('checked', flags[p]);
}
}
});
}
}
userExecute(command, confirmation) {
let user = View.getSelectedValue('#alterusername');
if (user === '') {
this.error('ERROR_SELECT_USER');
}
else {
let index;
while ((index = command.indexOf('<user>')) >= 0) {
command[index] = user;
}
if (typeof (confirmation) == 'undefined' || confirm(translate(confirmation, user))) {
this.execute(command, translate('CONFIRM_USER_UPDATED', user));
return true;
}
}
return false;
}
changeType() {
this.userExecute(['SET', 'USER', 'RANK', '<user>', View.getSelectedValue('#alterusertype')]);
}
changePrivilege(privname) {
let value = $('#privilege' + privname).prop('checked');
this.userExecute([value ? 'GRANT' : 'REVOKE', privname, value ? 'TO' : 'FROM', '<user>']);
}
resetPassword() {
this.userExecute(['SET', 'USER', 'PASSWORD', '<user>', ''], 'PROMPT_CONFIRM_NO_PASSWORD');
}
deleteUser() {
if (this.userExecute(['DELETE', 'USER', '<user>'], 'PROMPT_CONFIRM_DELETE')) {
this.open_alter_user();
}
;
}
createKicker(user, execute) {
let label = $('<span />')
.append($('<span />').text('✖').addClass('remove'))
.append($('<span />').text(' ' + user));
let kick = $('<a />').append(label).on('click', () => {
this.execute(execute, translate('CONFIRM_KICK', user));
this.open_kick_user();
});
return $('<li/>').append(kick);
}
open_kick_user() {
this.execute(['USERS', 'ONLINE'], (response) => {
$('#onlineusers').text('');
for (let user of response.record) {
$('#onlineusers').append(this.createKicker(user.id, ['KICK', 'USER', user.id]));
}
$('#onlineusers').append(this.createKicker(translate('LABEL_KICK_VISITORS'), ['KICK', 'VISITORS']));
}, 'ERROR_CANNOT_GET_USER_LIST', null);
}
}
;
