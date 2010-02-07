#
# This file is part of DisOrder.
# Copyright (C) 2010 Richard Kettlewell
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

all: disorder.jar doc/index.html

SOURCE=DisorderConfig.java DisorderMisc.java DisorderParseError.java	\
DisorderProtocolError.java DisorderServer.java TrackInformation.java	\
DisorderLog.java
EXTRAS=package.html

EXTRA_DIST=${SOURCE} ${EXTRAS}

disorder.jar: ${SOURCE}
	mkdir -p jar
	javac -d jar ${SOURCE}
	jar cvf disorder.jar -C jar/ .

doc/index.html: ${SOURCE} ${EXTRAS}
	mkdir -p doc
	javadoc -d doc \
		-link http://java.sun.com/javase/6/docs/api/ \
		-doctitle "DisOrder Java support" \
		${SOURCE}

clean-local:
	rm -f *.jar
	rm -rf jar
	rm -rf doc
