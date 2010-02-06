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

all: disorder.jar GetVersion.class doc/index.html

SOURCE:=$(wildcard ${srcdir}/uk/org/greenend/disorder/*.java)
EXTRAS:=$(wildcard ${srcdir}/uk/org/greenend/disorder/*.html)

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

GetVersion.class: disorder.jar
	javac -cp disorder.jar GetVersion.java

clean-local:
	rm -f *.jar
	rm -f *.class
	rm -rf jar
	rm -rf doc

