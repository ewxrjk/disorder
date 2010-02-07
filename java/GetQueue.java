/*
 * This file is part of DisOrder.
 * Copyright (C) 2010 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import uk.org.greenend.disorder.*;
import java.io.*;
import java.util.*;

class GetQueue {
  public static void main(String[] args) throws DisorderParseError,
                                                DisorderProtocolError,
                                                IOException {
    DisorderServer d = new DisorderServer();
    List<TrackInformation> q = d.queue();
    for(TrackInformation t: q)
      System.out.println(t.toString());
  }
}
/*
Local Variables:
mode:java
indent-tabs-mode:nil
c-basic-offset:2
End:
*/
