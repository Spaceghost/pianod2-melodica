export var Pianod;
(function (Pianod) {
let EVENT;
(function (EVENT) {
EVENT[EVENT["TRACK_COMPLETE"] = 4] = "TRACK_COMPLETE";
EVENT[EVENT["MIX_CHANGED"] = 21] = "MIX_CHANGED";
EVENT[EVENT["PLAYLISTS_CHANGED"] = 22] = "PLAYLISTS_CHANGED";
EVENT[EVENT["USERRATINGS_CHANGED"] = 23] = "USERRATINGS_CHANGED";
EVENT[EVENT["SOURCES_CHANGED"] = 24] = "SOURCES_CHANGED";
EVENT[EVENT["SONGRATING_CHANGED"] = 25] = "SONGRATING_CHANGED";
EVENT[EVENT["QUEUE_CHANGED"] = 26] = "QUEUE_CHANGED";
EVENT[EVENT["YELL"] = 31] = "YELL";
EVENT[EVENT["ACTIVITY"] = 32] = "ACTIVITY";
EVENT[EVENT["STATUS"] = 33] = "STATUS";
EVENT[EVENT["SOURCESTATUS"] = 34] = "SOURCESTATUS";
EVENT[EVENT["FAILURE_START"] = 500] = "FAILURE_START";
EVENT[EVENT["FAILURE_PLAYER_EMPTY"] = 501] = "FAILURE_PLAYER_EMPTY";
EVENT[EVENT["FAILURE_END"] = 599] = "FAILURE_END";
EVENT[EVENT["FAILURE_ANY"] = 599] = "FAILURE_ANY";
})(EVENT = Pianod.EVENT || (Pianod.EVENT = {}));
;
let STATUS;
(function (STATUS) {
STATUS[STATUS["S_OK"] = 200] = "S_OK";
STATUS[STATUS["DATA_START"] = 203] = "DATA_START";
STATUS[STATUS["DATA_END"] = 204] = "DATA_END";
STATUS[STATUS["S_MATCH"] = 206] = "S_MATCH";
STATUS[STATUS["S_ROUNDING"] = 207] = "S_ROUNDING";
STATUS[STATUS["S_PENDING"] = 210] = "S_PENDING";
STATUS[STATUS["MEDIA_ACTION_UNSUPPORTED"] = 460] = "MEDIA_ACTION_UNSUPPORTED";
})(STATUS = Pianod.STATUS || (Pianod.STATUS = {}));
;
;
;
;
;
;
;
;
;
class SubscriptionManager {
constructor() {
this.subscriptions = {};
}
add(triggers, callback) {
if (!Array.isArray(triggers)) {
triggers = [triggers];
}
for (let trigger of triggers) {
if (!(trigger in this.subscriptions)) {
this.subscriptions[trigger] = [];
}
this.subscriptions[trigger].push(callback);
if (Connection.debug) {
console.log('subscribed ', callback, ' to ', trigger);
}
}
}
invoke(triggers, notification) {
if (!Array.isArray(triggers)) {
triggers = [triggers];
}
for (let trigger of triggers) {
if (trigger in this.subscriptions) {
for (let subscriber of this.subscriptions[trigger]) {
try {
subscriber(notification);
}
catch (err) {
let message = "Trigger " + trigger + ": a handler threw " + err.message;
console.log(message);
if (Connection.debug) {
alert(message);
}
;
}
}
}
}
}
}
;
;
;
;
;
class Command {
constructor(command_name, params = {}) {
this.cmd = {};
this.cmd[command_name] = params;
}
;
add(control_option) {
if ("username" in control_option) {
this.cmd.asUser = control_option;
}
else if ("room" in control_option) {
this.cmd.inRoom = control_option;
}
else {
this.cmd.withSource = control_option;
}
}
toJSON() {
return JSON.stringify(this.cmd);
}
;
}
Pianod.Command = Command;
class Connection {
constructor(piano_url, successcb, disconnectcb) {
this.commands = { 0: {
'id': 0,
'command': 'connect',
'handler': () => void {}
} };
this.command_id = 1;
this.response_id = 0;
this.event_subscriptions = new SubscriptionManager();
this.state_subscriptions = new SubscriptionManager();
this.current_song_subscriptions = new SubscriptionManager();
this.state = {};
this.current_song = null;
this.close_functions = [];
if (typeof (disconnectcb) == 'function') {
this.close_functions.push(disconnectcb);
}
if (typeof (successcb) == 'function') {
this.commands[0].handler = successcb;
}
this.socket = new WebSocket(piano_url + "?protocol=json");
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
closed(event) {
for (let func of this.close_functions) {
func(event);
}
}
dispatchEvents(events) {
for (let event of events) {
this.event_subscriptions.invoke(event.code, event);
}
}
;
static mergeDictionaryUpdates(from, into) {
let merged_keys = [];
for (let key in from) {
if (from.hasOwnProperty(key)) {
if (!(key in into) || into[key] != from[key]) {
into[key] = from[key];
merged_keys.push(key);
}
}
}
return merged_keys;
}
receiveMessage(event) {
if (Connection.debug) {
console.log(event.data);
}
let message = JSON.parse(event.data);
if ("code" in message) {
let cmd = this.commands[this.response_id];
if (typeof (cmd) == 'undefined') {
alert('Excess replies received!');
console.log('Excess replies received!');
}
message.command = cmd.command;
delete this.commands[this.response_id++];
if (Connection.debug) {
console.log("Received response: ", message);
}
assert(typeof (cmd.handler) == 'function');
cmd.handler(message);
}
if (typeof message.events !== "undefined") {
this.dispatchEvents(message.events);
}
if (typeof message.errors !== "undefined") {
this.dispatchEvents(message.errors);
}
if (typeof message.state !== "undefined") {
let merged_fields = Connection.mergeDictionaryUpdates(message.state, this.state);
merged_fields.push("*");
this.state_subscriptions.invoke(merged_fields, this.state);
}
if (typeof message.currentSong != "undefined") {
let merged_fields = [];
if (message.currentSong == null) {
this.current_song = null;
}
else {
if (this.current_song == null) {
this.current_song = {};
}
merged_fields = Connection.mergeDictionaryUpdates(message.currentSong, this.current_song);
}
merged_fields.push("*");
this.current_song_subscriptions.invoke(merged_fields, this.current_song);
}
}
sendCommand(command, func) {
if (typeof command == "string") {
command = new Command(command);
}
let cmd = {
'id': this.command_id,
'command': command.toJSON(),
'handler': func || function () { }
};
this.commands[this.command_id++] = cmd;
this.socket.send(cmd.command);
}
;
onClose(func) {
this.close_functions.push(func);
}
subscribeToEvent(func, codes) {
this.event_subscriptions.add(codes, func);
;
}
subscribeToState(func, field = "*") {
this.state_subscriptions.add(field, func);
if (Object.keys(this.state).length != 0) {
func(this.state);
}
}
subscribeToSong(func, field = "*") {
this.current_song_subscriptions.add(field, func);
func(this.current_song);
}
}
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
return null;
if (url.fromweb)
return null;
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
if (url == null) {
url = 'ws://house.perette.barella.org:4446/pianod';
}
return url;
}
}
Pianod.Finder = Finder;
;
})(Pianod || (Pianod = {}));
export class Viewer {
constructor(url, instancename = "pianod") {
this.instance = null;
this.start_time = 0;
this.duration = 0;
this.retryInterval = null;
this.interval = null;
this.updateTrackTime = () => {
let now = new Date();
let point = Math.round((now.getTime() - this.start_time) / 1000);
this.setAllElementsWithClass('timepoint', this.formatTime(point));
let percent = 0;
if (this.duration > 0) {
percent = Math.round(point * 10000 / this.duration) / 100;
if (percent > 100)
percent = 100;
}
this.setAllElementsWithClass('progressbar', percent + '%');
};
this.stateChange = (state) => {
this.hide('offline');
this.toggle('idle', state.playbackState == 'idle');
this.toggle(['playbackstatus', 'paused'], state.playbackState == 'paused');
this.toggle(['playbackstatus', 'stalled'], state.playbackState == 'stalled');
if (state.playbackState == 'playing') {
if (this.interval == null) {
this.interval = setInterval(this.updateTrackTime, 1000);
}
}
else {
if (typeof (this.interval) != 'undefined') {
clearInterval(this.interval);
this.interval = null;
}
}
this.setAllElementsWithClass('playlistname', state.selectedPlaylist.name);
this.show("playlistname");
};
this.currentSongChanged = (song) => {
this.toggle('idle', song == null);
this.toggle('playing', song != null);
if (song == null) {
this.clearFields();
this.duration = 0;
}
else {
for (let field in Viewer.field_to_location) {
if ((field in song) && (song[field] != null)) {
let loc = Viewer.field_to_location[field];
this.setAllElementsWithClass(loc, song[field]);
this.show(loc);
}
}
this.setAllElementsWithClass('songsourcetype', song.source.type);
this.setAllElementsWithClass('songsourcename', song.source.name);
this.show(["songsourcetype", "songsourcename"]);
this.duration = song.duration;
this.setAllElementsWithClass('duration', this.formatTime(this.duration));
if (typeof song.timeIndex != "undefined" && song.timeIndex != null) {
let now = new Date();
this.start_time = now.getTime() - song.timeIndex * 1000;
}
}
this.updateTrackTime();
};
this.url = url;
this.instance = document.getElementById(instancename);
if (this.instance == null) {
alert("Instance " + instancename + " not found.");
return;
}
this.hide(['idle', 'playing']);
this.show('offline');
this.instance.style.display = "block";
this.clearFields();
this.openPiano();
this.retryInterval = setInterval(() => {
if (typeof (this.piano) == 'undefined') {
this.openPiano();
}
}, 7037);
}
setAllElementsWithClass(target, value) {
if (typeof value == "number") {
value = value.toString();
}
if (this.instance != null) {
let targets = this.instance.getElementsByClassName(target);
for (let element of targets) {
if (target == 'albumart') {
element.setAttribute('src', value === "" ? 'no-art.jpeg' : value);
}
else if (target == 'progressbar') {
element.style.width = value;
}
else {
let place = element.getElementsByClassName("value");
if (place.length == 0) {
element.innerText = value;
}
else {
place[0].innerText = value;
}
}
}
}
}
toggle(target, shown) {
if (!Array.isArray(target)) {
target = [target];
}
;
if (this.instance != null) {
for (let item of target) {
let elements = this.instance.getElementsByClassName(item);
for (let element of elements) {
element.style.display = (shown ? (element.tagName == "div" ? "block" : null) : "none");
}
}
}
}
show(target) {
this.toggle(target, true);
}
hide(target) {
this.toggle(target, false);
}
;
clearFields() {
for (let field in Viewer.field_to_location) {
let loc = Viewer.field_to_location[field];
this.setAllElementsWithClass(loc, "");
if (loc != "albumart") {
this.hide(loc);
}
}
this.hide(["songsourcetype", "songsourcename", "playlistname"]);
}
formatTime(time) {
time = Math.round(time);
let min = Math.floor(time / 60);
let sec = time % 60;
return (min + ':' + (sec <= 9 ? '0' : '') + sec);
}
openPiano() {
this.piano = new Pianod.Connection(this.url, () => {
this.hide(['offline', 'playing']);
this.show('idle');
this.piano.subscribeToState(this.stateChange);
this.piano.subscribeToSong(this.currentSongChanged);
}, () => {
this.hide(['idle', 'playing']);
this.show('offline');
delete this.piano;
});
}
destroy() {
clearInterval(this.retryInterval);
if (this.interval != null) {
clearInterval(this.interval);
this.interval = null;
}
if (this.instance != null) {
this.instance.style.display = "none";
}
}
}
Viewer.field_to_location = {
artistName: 'artistname',
albumName: 'albumname',
albumArtUrl: 'albumart',
trackName: 'trackname',
playlistName: 'songplaylist',
genre: 'genre',
year: 'year'
};
Viewer.Finder = Pianod.Finder;
function assert(check) {
if (!check) {
alert("Assertion; check console.");
throw new Error('Assertion failed.');
}
}
export { assert };
