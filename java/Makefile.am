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

all: disorder.jar GetVersion.class

disorder.jar: $(wildcard ${srcdir}/uk/org/greenend/disorder/*.java)
	mkdir -p jar
	javac -d jar ${srcdir}/uk/org/greenend/disorder/*.java
	jar cvf disorder.jar -C jar/ .

GetVersion.class: disorder.jar
	javac -cp disorder.jar GetVersion.java

clean-local:
	rm -f *.jar
	rm -f *.class
	rm -rf jar
