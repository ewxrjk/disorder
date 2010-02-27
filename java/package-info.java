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
 * href="http://www.greenend.org.uk/rjk/disorder/">DisOrder</a>.
 * <b>Currently a work in progress.</b>
 *
 * <p>Example of usage:</p>
 * <pre style="border: 1px solid">import uk.org.greenend.disorder.*;
 *import java.io.*;
 *
 *class GetVersion {
 *  public static void main(String[] args) throws DisorderParseException,
 *                                                DisorderIOException,
 *                                                DisorderProtocolException,
 *                                                IOException {
 *    DisorderServer d = new DisorderServer();
 *    String v = d.version();
 *    System.out.println(v);
 *  }
 *}</pre>
 */
package uk.org.greenend.disorder;
