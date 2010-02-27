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
/** 
 * Java support for <a
 * href="http://www.greenend.org.uk/rjk/disorder/" target=_top>DisOrder</a>.
 * <b>Currently a work in progress.</b>
 *
 * <h3>Example</h3>
 *
 * <pre class=example>import uk.org.greenend.disorder.*;
 *import java.io.*;
 *
 *class GetVersion {
 *  public static void main(String[] args) throws DisorderParseException,
 *                                                DisorderIOException,
 *                                                DisorderProtocolException,
 *                                                InterruptedException,
 *                                                IOException {
 *    DisorderServer d = new DisorderServer();
 *    String v = d.version();
 *    System.out.println(v);
 *  }
 *}</pre>
 *
 * <h3>Copyright</h3>
 *
 * <p>Copyright &copy; 2010 Richard Kettlewell
 *
 * <p>This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * <p>This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * <p>You should have received a copy of the GNU General Public License
 * along with this program.  If not, see &lt;<a href="http://www.gnu.org/licenses/" target=_top>http://www.gnu.org/licenses/</a>>.
 */
package uk.org.greenend.disorder;
