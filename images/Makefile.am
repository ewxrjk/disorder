#
# This file is part of DisOrder.
# Copyright (C) 2005-2010 Richard Kettlewell
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

pkghttp_DATA=cross.png down.png downdown.png edit.png nocross.png	\
nodown.png nodowndown.png noup.png noupup.png tick.png up.png upup.png	\
notes.png play.png pause.png random.png randomcross.png notescross.png	\
propagate.png speaker.png speakercross.png directory.png logo.png	\
logosmall.png logo256.png

EXTRA_DIST=$(pkghttp_DATA) duck.png disobedience16x16.xpm		\
disobedience32x32.xpm cross.svg go.svg notes.svg noteson.svg pause.svg	\
query.svg queryon.svg speaker.svg speakeron.svg cross32.png		\
pause32.png play32.png playdisabled32.png playenabled32.png		\
randomdisabled32.png randomenabled32.png rtpdisabled32.png		\
rtpenabled32.png duck55.png cards24.png cards48.png 			\
cards-simple-fanned.svg cards-thin.svg disobedience128x128.png 		\
disobedience256x256.png disobedience48x48.png disobedience64x64.png

DISOBEDIENCE_IMAGES=up.png down.png cards24.png logo256.png duck.png	\
propagate.png

if GTK
noinst_HEADERS=images.h

images.h: $(DISOBEDIENCE_IMAGES)
	set -e;								\
	exec > @$.new;							\
	for png in $^; do						\
	  name=`echo $$png | $(GNUSED) 's,.*/,,;s,\.png,,;'`;		\
	  gdk-pixbuf-csource --raw --name=image_$$name $$png;		\
	done;								\
	echo "static const struct image images[] = {";			\
	for png in `echo $^`; do					\
	  name=`echo $$png | $(GNUSED) 's,.*/,,;s,\.png,,;'`;		\
	  echo "  { \"$$name.png\", image_$$name },";			\
	done|sort;							\
	echo "};"
	mv @$.new $@
endif

CLEANFILES=$(SEDFILES) images.h
