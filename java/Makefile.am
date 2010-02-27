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

bin_SCRIPTS=DisorderRtpPlayer
jar_DATA=disorder.jar
noinst_DATA=GetQueue.class GetVersion.class

SOURCE=DisorderConfig.java DisorderMisc.java			\
DisorderParseException.java DisorderProtocolException.java	\
DisorderServer.java TrackInformation.java DisorderLog.java	\
RtpPacket.java RtpClient.java DisorderRtpPlayer.java		\
DisorderIOException.java package-info.java

EXTRA_DIST=${SOURCE} GetVersion.java GetQueue.java \
	DisorderRtpPlayer.in DisorderRtpPlayer.plist.in

disorder.jar: ${SOURCE}
	${MKDIR_P} jar
	${JAVAC} ${JAVACFLAGS} -d jar ${SOURCE}
	${JAR} cvf disorder.jar -C jar/ .

%.class: %.java
	${JAVAC} ${JAVACFLAGS} -cp disorder.jar $<

GetQueue.class GetVersion.class: disorder.jar

SEDFILES=DisorderRtpPlayer DisorderRtpPlayer.plist

include ${top_srcdir}/scripts/sedfiles.make

doc/index.html: ${SOURCE}
	${MKDIR_P} doc
	${JAVADOC} -d doc \
		-link http://java.sun.com/javase/6/docs/api/ \
		-doctitle "DisOrder Java support" \
		${SOURCE}

if BUNDLES
all: DisorderRtpPlayer.app
endif

DisorderRtpPlayer.app: disorder.jar DisorderRtpPlayer DisorderRtpPlayer.plist duck.icns
	$(MKDIR_P) new_$@/Contents/MacOS
	$(MKDIR_P) new_$@/Contents/Resources/Java
	$(INSTALL) PkgInfo new_$@/Contents/PkgInfo
	$(INSTALL) /System/Library/Frameworks/JavaVM.framework/Versions/Current/Resources/MacOS/JavaApplicationStub new_$@/Contents/MacOS/JavaApplicationStub
	$(INSTALL_DATA) disorder.jar new_$@/Contents/Resources/Java/disorder.jar
	$(INSTALL_DATA) DisorderRtpPlayer.plist new_$@/Contents/Info.plist
	$(INSTALL_DATA) duck.icns new_$@/Contents/Resources/disorder.icns
	rm -rf $@
	mv new_$@ $@

duck.icns: ${top_srcdir}/images/duck.png
	sips -s format icns $^ --out $@

clean-local:
	rm -f *.jar
	rm -f *.class
	rm -rf jar
	rm -rf doc
