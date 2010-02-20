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
package uk.org.greenend.disorder;

import java.net.*;
import java.io.*;
import java.util.*;
import javax.sound.sampled.*;

/**
 * An RTP client for use with DisOrder network play.
 *
 * <p>After construction and any other setup, call the {@link
 * #startPlayer(SourceDataLine) startPlayer()} method to activate the player
 * thread.  You can call {@link #stopPlayer()} to shut it down at any time.
 * The player will write sound to the data line it was started with.
 *
 * <p>Having started the player you must supply RTP packets.  The simplest way
 * to achieve this is to use {@link #listen(String, int) listen()}.  However it
 * is also possible to call {@link #receive(byte[], int) receive()} for each
 * packet.
 *
 * <p>The current version of the code assumes 44100KHz 16-bit stereo.  Although
 * DisOrder does aim to follow the RTP specification, no promises are made
 * about supporting streams from any other source.
 */
public class RtpClient {
  /**
   * Buffered RTP packets, sorted by sequence number.
   */
  private TreeSet<RtpPacket> buffer;

  /**
   * Next sequence number to play.
   *
   * This corresponds to the <code>timestamp</code> field of {@link RtpPacket}.
   */
  private int next;

  /**
   * Number of buffered samples
   */
  private long bufferSize;
  
  /**
   * Maximum number of silent samples to "fill in".
   */
  private int maxSilence;

  /**
   * Maximum buffer size.
   */
  private long maxBuffer;

  /**
   * The player thread.
   */
  private Thread player;

  /**
   * The listener thread
   */
  private Thread listener;
  
  /**
   * Where to play sound to.
   */
  private SourceDataLine line;

  /**
   * Whether debugging is enabled.
   */
  private boolean debugging;

  /**
   * Create a listening socket.
   *
   * @param host Destination address to listen on
   * @param port Destination port number
   * @return Socket
   */
  private DatagramSocket createSocket(String host, int port)
    throws IOException {
    DatagramSocket sock;
    InetAddress a = InetAddress.getByName(host);
    // Create the socket
    if(a.isMulticastAddress()) {
      if(debugging)
        System.err.printf("%s port %d: multicast socket\n", host, port);
      MulticastSocket s = new MulticastSocket(port);
      s.joinGroup(a);
      sock = s;
      // SO_REUSEADDR is implicit for MulticastSocket, so we need not set it
      // here.
    } else {
      if(debugging)
        System.err.printf("%s port %d: unicast/broadcast socket\n", host, port);
      sock = new DatagramSocket();
      sock.setReuseAddress(true);
      sock.bind(new InetSocketAddress(port));
    }
    return sock;
  }

  /**
   * Construct an RTP client.
   */
  public RtpClient() {
    buffer = new TreeSet<RtpPacket>();
    maxSilence = 256;
    maxBuffer = 44100 * 2;              // i.e. 1s TODO assumes sample format
    // TODO these values should be configurable
    if(System.getProperty("DISORDER_DEBUG") != null)
      debugging = true;
  }

  /**
   * Receive RTP packets over the network.
   *
   * <p>Normally <code>host</code> would be the multicast address that the
   * DisOrder server transmits RTP packets to.  Other configurations are
   * possible in principle, but are poorly tested.
   *
   * <p><code>port</code> similarly is the port number that RTP packets are
   * sent to.
   *
   * <p>This method doesn't return (though it might terminate due to an
   * exception).
   *
   * <p>You must have already started the player (with
   * <code>startPlayer()</code>).
   *
   * <p>For a more flexible (but less friendly) interface, see {@link
   * #receive(byte[], int) receive()}.
   *
   * @param host Destination address to listen on
   * @param port Destination port number
   * @param timeout Socket timeout
   */
  public void listen(String host, int port, int timeout)
    throws IOException {
    DatagramSocket sock = createSocket(host, port);
    try {
      if(timeout > 0)
        sock.setSoTimeout(timeout);
      DatagramPacket dp = new DatagramPacket(new byte[4096], 4096);
      for(;;) {
        if(Thread.interrupted())
          return;
        try {
          sock.receive(dp);
        } catch(SocketTimeoutException ex) {
          continue;
        }
        receive(dp.getData(), dp.getLength());
        // We'll need a new buffer
        dp.setData(new byte[4096]);
      }
    } finally {
      sock.close();
    }
  }

  /**
   * Receive RTP packets over the network.
   *
   * <p>Normally <code>host</code> would be the multicast address that the
   * DisOrder server transmits RTP packets to.  Other configurations are
   * possible in principle, but are poorly tested.
   *
   * <p><code>port</code> similarly is the port number that RTP packets are
   * sent to.
   *
   * <p>This method doesn't return (though it might terminate due to an
   * exception).
   *
   * <p>You must have already started the player (with
   * <code>startPlayer()</code>).
   *
   * <p>For a more flexible (but less friendly) interface, see {@link
   * #receive(byte[], int) receive()}.
   *
   * @param host Destination address to listen on
   * @param port Destination port number
   */
  public void listen(String host, int port) throws IOException {
    listen(host, port, 0);
  }
  
  /**
   * Receive one packet.
   *
   * <p>May keep a reference to <code>data</code>, so don't re-use it.
   * Packets from the past or the far future are ignored.
   *
   * <p>You must have already started the player (with
   * <code>startPlayer()</code>).
   *
   * <p>For a friendlier, but less flexible, interface see {@link
   * #listen(String, int) listen()}.
   *
   * @param data Buffer containing packet
   * @param length Length of packet
   */
  public void receive(byte[] data, int length) {
    RtpPacket p = new RtpPacket(data, length);
    // TODO ignore packets with wrong payload type
    // TODO extensions
    synchronized(this) {
      if(buffer.size() > 0) {
        // Ignore packets "in the past" - or ridiculously far in the future
        if(RtpPacket.compareTimestamp(p.timestamp, next) < 0)
          return;
      } else {
        // If this is the first packet we've seen, it's where we'll start.
        // Similarly if the buffer drains down to empty we'll restart at the
        // next packet we see whatever it is.
        next = p.timestamp;
      }
      buffer.add(p);                  // Stash the packet
      bufferSize += (p.length - p.offset)/2; // Update buffer usage
      // TODO assumes sample format
      notify();                         // Wake up the player
    }
  }

  /**
   * The player thread.
   *
   * The player thread will play data as it arrives.
   */
  protected void play() {
    byte[] silence = new byte[maxSilence];
    byte[] bytes;                     // what to play
    int offset;                       // offset in bytes
    int length;                       // maximum bytes to bytes

    if(debugging)
      System.err.println("Player thread up and running");
    for(;;) {
      synchronized(this) {
        if(Thread.interrupted())
          return;
        // If there's nothing to play then wait for something to change
        if(buffer.size() == 0) {
          try {
            wait();
          } catch(InterruptedException e) {
            return;
          }
          continue;
        }
        // Get the first available packet
        RtpPacket p = buffer.first();
        // ...and calculate how many samples it contains
        final int samples = (p.length - p.offset)/2;
        // If p is entirely in the past then junk it and go round again
        if(RtpPacket.compareTimestamp(p.timestamp + samples, next) <= 0) {
          buffer.remove(p);
          bufferSize -= (p.length - p.offset)/2; // TODO assumes sample size
          continue;
        }
        if(RtpPacket.compareTimestamp(next, p.timestamp) < 0) {
          // If p is entirely in the future then we'll play some silence
          int gap = 2 * (p.timestamp - next); // TODO assumes sample format
          if(gap > maxSilence || gap < 0)
            gap = maxSilence;
          bytes = silence;
          offset = 0;
          length = gap;
          if(debugging)
            System.err.printf("Insert %d bytes silence\n", gap);
        } else {
          // next must point somewhere within p
          bytes = p.data;
          offset = p.offset + 2 * (next - p.timestamp);
          length = p.length - offset;
        }
        if(bufferSize > maxBuffer) {
          // If the buffer is getting too full then we skip leading silence in
          // order to catch up.  Note that we do this even if we're filling in
          // silence due to lost packets - the ideal time to do so, in fact.
          //
          // We hope that we shorten inter-track gaps by this means rather than
          // truncating silences within a track; if it results in audible
          // artefacts then we can try something more sophisticate.
          int dropped = 0;
          while(length > 0
                && bytes[offset] == 0
                && bytes[offset + 1] == 0
                && bytes[offset + 2] == 0
                && bytes[offset + 3] == 0) {
            next += 2;
            offset += 4;
            length -= 4;
            dropped += 4;
          }
          // TODO assumes sample format
          // TODO ...tune what 'too full' means
          if(debugging && dropped > 0) {
            System.err.printf("Dropped %d bytes of silence because %d > %d\n",
                              dropped, bufferSize, maxBuffer);
          }
          if(length == 0) {
            // Ran out of packet.  Go round again; we'll junk the packet
            // immediately.
            continue;
          }
        }
      }
      // Make some noise
      int bytesPlayed = line.write(bytes, offset, length);
      synchronized(this) {
        next += bytesPlayed / 2;
        // TODO assumes sample format
      }
    }
  }

  /**
   * Start the player thread.
   *
   * If a player thread is already running, does nothing.
   *
   * <p>The caller should have opened and started <code>line</code>.
   *
   * <p>You must not call <code>receive()</code> or <code>listen()</code>
   * concurrently with this method.
   *
   * @param sdl Data line to use to play samples
   */
  public void startPlayer(SourceDataLine sdl) {
    if(player != null)
      return;
    player = new Thread() {
        @SuppressWarnings("super")
        @Override
        public void run() {
          play();
        }
      };
    this.line = sdl;
    player.start();
  }
  
  /**
   * Stop the player thread.
   * If no player thread is running, does nothing.
   *
   * <p>The data line (<code>line</code> as passed to
   * <code>startPlayer()</code>) is drained, but not stopped or closed.
   *
   * <p>You must not call <code>receive()</code> or <code>listen()</code>
   * concurrently with this method.
   */
  public void stopPlayer() throws InterruptedException {
    if(player == null)
      return;
    player.interrupt();
    player.join();
    line.drain();
    line = null;
    player = null;
  }

  /**
   * Start the listener thread
   *
   * @param host Destination address to listen on
   * @param port Destination port number
   */
  public void startListener(final String host,
                            final int port) {
    if(listener != null)
      return;
    listener = new Thread() {
        @SuppressWarnings("super")
        @Override
        public void run() {
          try {
            listen(host, port, 500);
          } catch(IOException e) {
            // ignored
          }
        }
      };
    listener.start();
  }

  /**
   * Stop the listener thread
   */
  public void stopListener() throws InterruptedException {
    if(listener == null)
      return;
    listener.interrupt();
    listener.join();
    listener = null;
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