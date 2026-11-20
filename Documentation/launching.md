Starting `pianod2`
-----------------
### Configuration files location
`pianod` stores several files in a configuration directory, the configuration of which is set by:

* `-d directory` command-line option, with `/pianod2` appended.
* `$XDG_CONFIG_HOME/pianod2` (if `XDG_CONFIG_HOME` is set)
* `$HOME/.config/pianod2` (if `HOME` is set and not running as root)
* `$(sysconfdir)/pianod2` (if running as root).  sysconfdir is set at compile time by the configure script; a tarball install will use `/opt/local/etc`, MacPorts uses `/opt/local/etc`, and `/etc` is common when installing from a package manager.
* `./pianod2` (if not running as root).

### Configuration files
On startup, `pianod2` reads its `startscript`, executing the command therein as a `pianod` administrator.  The file uses the same syntax and commands as the socket interface.  A sample startscript is provided in the `contrib` directory.

`pianod` also has a user data file, `passwd`.  If the file does not exist, a single user `admin` (password `admin`) is created.  This user is persisted, so once you create your own administrator account be sure to delete `admin`.

### Launching at boot or login
Unlike older UNIX daemons, `pianod` does not use `fork`(2)/`exec`(2) or `daemon`(3) on startup.  Supplied sample configuration files expect `pianod` to be installed in `/usr/local/bin`; if this is not right you will need to revise the files when installing.

If `pianod` is started as root, after establishing the listener socket, `pianod` drops root privileges and takes on a user persona, by default the user 'nobody'.  It also adopts the user’s groups.  To allow data to be persisted, the configuration directory is (re)assigned to this user.  The user can be selected via `-n`; `-n root` will allow pianod to retain root privileges.  Using `-n root` may be a useful troubleshooting tool, but running any network-based service as root is risky.

The `-g` option can be used to specify a comma-separated list of groups instead of nobody’s groups.  Some platforms have a group `music` or `audio` to which users must belong to be able to open audio ports.

*`pianod` must not be installed setuid.*  Using a combination of -d and -n options, a malicious user could arbitrarily reassign file ownership.

On OS X:

* Included in `contrib` is an example `com.deviousfish.pianod.plist`.
* When installing the file, be sure to review the file as there are some file paths and user names in it.
* To launch `pianod` upon login of a particular user, install the file in that user's Library/LaunchAgents directory.
* To launch `pianod` at boot, place it in the system's /Library/LaunchDaemons directory.  Edit the file to enable the UserName key, setting it to run as your desired user.

On Linux with `systemd`(8):

* Included in `contrib` is `pianod.service` for systems using `systemd`(1).
* To launch `pianod` at boot, place it in `/usr/local/lib/systemd/system` and put the startscript in `/etc/pianod/startscript`.
* To launch `pianod` at login, install the file in the user's `.config/systemd` directory [Need verification].  You will want to remove `User=nobody` and `-i /etc/pianod/startscript` from the `[Service]` section of the file.
* After installing the file run "systemctl start pianod" to start the daemon and "systemctl enable pianod" to enable it to start on boot.
* The sample expects pianod to be installed in /usr/sbin; set your configure options or modify the start file as needed.

On systems using /etc/init.d scripts:

* Included in `contrib` is `pianod.raspian.init`.
* The script is for the Raspbian distribution but can be modified as needed.
* The sample expects pianod to be installed in /usr/sbin; set your configure options or modify the start file as needed.

For systems that use `init` without `/etc/init.d`, you'll need to edit the inittab file.  Add this to `/etc/inittab`:

	pd:23:respawn:su - username -c /usr/local/bin/pianod

On Windows, you are on your own.

### Start Options
See the [pianod(1)] manual page for the full list of command line options.

[pianod(1)]: http://deviousfish.com/pianod/pianod.txt
