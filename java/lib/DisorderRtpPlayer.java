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
import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;

class DisorderRtpPlayer {
  static private void listen(String host, int port)
    throws IOException,
           LineUnavailableException {
    line.open();
    line.start();
    client.startPlayer(line);
    client.startListener(host, port);
  }

  static private JTextField hostField;
  static private JTextField portField;
  static private JCheckBox active;

  static private SourceDataLine line;
  static private RtpClient client = new RtpClient();
  static private String runningHost = null;
  static private int runningPort = -1;
  static private boolean running = false;

  static class DetailsChanged implements ItemListener, DocumentListener {
    public void itemStateChanged(ItemEvent e) {
      //System.err.format("itemStateChanged: %s\n", e.toString());
      somethingChanged();
    }
    public void changedUpdate(DocumentEvent e) {
      //System.err.format("changedUpdate: %s\n", e.toString());
      somethingChanged();
    }
    public void insertUpdate(DocumentEvent e) {
      //System.err.format("insertUpdate: %s\n", e.toString());
      somethingChanged();
    }
    public void removeUpdate(DocumentEvent e) {
      //System.err.format("removeUpdate: %s\n", e.toString());
      somethingChanged();
    }
  }

  static private void somethingChanged() {
    String newHost = hostField.getText();
    int newPort;
    boolean newRunning = active.isSelected();

    try {
      newPort = Integer.parseInt(portField.getText());
    } catch(NumberFormatException ex) {
      // TODO visual feedback that the number is bad
      return;
    }
    /*System.err.format("newRunning=%s  newPort=%d  newHost=%s\n",
                      newRunning ? "true" : "false",
                      newPort,
                      newHost == null ? "(null)" : newHost);*/
    if(running == newRunning
       && ((runningHost == null
            && newHost == null)
           || (runningHost != null
               && newHost != null
               && runningHost.equals(newHost)))
       && runningPort == newPort) {
      // Nothing changed
      return;
    }
    if(running) {
      try {
        client.stopListener();
        client.stopPlayer();
      } catch(InterruptedException ex) {
        return;
      }
      line.stop();
    }
    if(newRunning) {
      line.start();
      client.startPlayer(line);
      try {
        client.startListener(newHost, newPort);
      } catch(IOException ex) {
        // TODO
      }
    }
    running = newRunning;
    runningHost = newHost;
    runningPort = newPort;
  }

  static private void createUI() {
    // The top-level window
    JFrame frame = new JFrame("DisOrder Network Play");
    frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

    hostField = new JTextField(16);
    portField = new JTextField();
    active = new JCheckBox();

    // TODO This is a really poor layout but it gets us up and running
    GridLayout layout = new GridLayout(3, 2);
    frame.setLayout(layout);

    frame.add(new JLabel("Address", JLabel.RIGHT));
    frame.add(hostField);
    frame.add(new JLabel("Port", JLabel.RIGHT));
    frame.add(portField);
    frame.add(new JLabel("Play", JLabel.RIGHT));
    frame.add(active);
    frame.pack();
    frame.setVisible(true);

    DetailsChanged d = new DetailsChanged();
    hostField.getDocument().addDocumentListener(d);
    portField.getDocument().addDocumentListener(d);
    active.addItemListener(d);
  }

  public static void main(String[] args) throws IOException,
                                                LineUnavailableException {
    // Initialize output
    AudioFormat af = new AudioFormat(AudioFormat.Encoding.PCM_SIGNED,
                                     44100,
                                     16,
                                     2,
                                     4,
                                     44100,
                                     true);
    DataLine.Info dli = new DataLine.Info(SourceDataLine.class,
                                          af);
    line = (SourceDataLine)AudioSystem.getLine(dli);
    line.open();

    switch(args.length) {
    case 0: {
      javax.swing.SwingUtilities.invokeLater(new Runnable() {
          public void run() {
            createUI();
          }
        });
      break;
    }
    case 2: {
      listen(args[0], Integer.parseInt(args[1]));
      break;
    }
    default:
      System.err.println("Usage: DisorderRtpPlayer [ADDRESS PORT]");
      System.exit(1);
    }
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

