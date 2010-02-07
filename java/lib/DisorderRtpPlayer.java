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
import uk.org.greenend.disorder.RtpClient;
import javax.sound.sampled.*;
import java.io.*;

class DisorderRtpPlayer {
  public static void main(String[] args) throws IOException,
                                                LineUnavailableException {
    if(args.length != 2) {
      System.err.println("Usage: DisorderRtpPlayer ADDRESS PORT");
      System.exit(1);
    }
    AudioFormat af = new AudioFormat(AudioFormat.Encoding.PCM_SIGNED,
                                     44100,
                                     16,
                                     2,
                                     4,
                                     44100,
                                     true);
    SourceDataLine sdl;
    DataLine.Info dli = new DataLine.Info(SourceDataLine.class,
                                          af);
    sdl = (SourceDataLine)AudioSystem.getLine(dli);
    sdl.open();
    sdl.start();
    RtpClient client = new RtpClient();
    client.startPlayer(sdl);
    client.listen(args[0], Integer.parseInt(args[1]));
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
