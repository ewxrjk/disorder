#
# This file is part of DisOrder.
# Copyright (C) 2005, 2006 Richard Kettlewell
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
#

complete -r disorder 2>/dev/null || true
complete -r disorderd 2>/dev/null || true
complete -r disorder-dump 2>/dev/null || true
complete -r disobedience 2>/dev/null || true

complete -o default \
         -A file \
         -W "allfiles authorize become dirs disable disable-random
             enable enable-random files get get-volume length log move
             play playing prefs quack queue random-disable
             random-enable recent reconfigure remove rescan scratch
             search set set-volume shutdown stats unset version resolve
             part pause resume scratch-id get-global set-global unset-global
             tags new rtp-address adduser users edituser
             -h --help -H --help-commands --version -V --config -c
             --length --debug -d" \
	 disorder

complete -o default \
         -A file \
         -W "-h --help --version -V --config -c
             --debug --dump -d --undump -u --recover -r --recompute-aliases
             -a" \
	 disorder-dump

complete -o default \
         -A file \
         -W "-h --help --version -V --config -c --debug -d --foreground -f
             --pidfile -P" \
	 disorderd

complete -o default \
         -A file \
         -W "-h --help --version -V --config -c --debug -d --tufnel -t
             --gtk-module --g-fatal-warnings --gtk-debug --gtk-no-debug
             --class --name --gdk-debug --gdk-no-debug
             --display --screen --sync --gxid-host --gxid-port" \
	 disobedience

# Local Variables:
# mode:sh
# End:
