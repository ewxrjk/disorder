/*
 * Automatically generated file, see scripts/protocol
 *
 * DO NOT EDIT.
 */
/*
 * This file is part of DisOrder.
 * Copyright (C) 2010-11, 13 Richard Kettlewell
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
/* Generated synchronous client API implemenattion */

using System;
using System.Collections.Generic;
using System.Threading;

namespace uk.org.greenend.DisOrder
{
  public partial class Connection
  {
    /// <summary>Adopt a track</summary>
    /// <remarks>
    /// <para>Makes the calling user owner of a randomly picked track.</para>
    /// </remarks>
    public int Adopt(string id) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "adopt", id);
    }

    /// <summary>Adopt a track (asynchronous)</summary>
    /// <remarks>
    /// <para>Makes the calling user owner of a randomly picked track.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void AdoptAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Adopt(id);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Create a user</summary>
    /// <remarks>
    /// <para>Create a new user.  Requires the 'admin' right.  Email addresses etc must be filled in in separate commands.</para>
    /// </remarks>
    public int Adduser(string user, string password, string rights) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "adduser", user, password, rights);
    }

    /// <summary>Create a user (asynchronous)</summary>
    /// <remarks>
    /// <para>Create a new user.  Requires the 'admin' right.  Email addresses etc must be filled in in separate commands.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void AdduserAsync(string user, string password, string rights, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Adduser(user, password, rights);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List files and directories in a directory</summary>
    /// <remarks>
    /// <para>See 'files' and 'dirs' for more specific lists.</para>
    /// </remarks>
    public int Allfiles(out IList<string> files, string dir, string re) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "allfiles", dir, re);
      files = lines;
      return rc;
    }

    /// <summary>List files and directories in a directory (asynchronous)</summary>
    /// <remarks>
    /// <para>See 'files' and 'dirs' for more specific lists.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void AllfilesAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Allfiles(out files, dir, re);
          if(callback != null) {
            try {
              callback(rc, files);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Confirm registration</summary>
    /// <remarks>
    /// <para>The confirmation string must have been created with 'register'.  The username is returned so the caller knows who they are.</para>
    /// </remarks>
    public int Confirm(string confirmation) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "confirm", confirmation);
      return rc;
    }

    /// <summary>Confirm registration (asynchronous)</summary>
    /// <remarks>
    /// <para>The confirmation string must have been created with 'register'.  The username is returned so the caller knows who they are.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ConfirmAsync(string confirmation, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Confirm(confirmation);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Log in with a cookie</summary>
    /// <remarks>
    /// <para>The cookie must have been created with 'make-cookie'.  The username is returned so the caller knows who they are.</para>
    /// </remarks>
    public int Cookie(string cookie) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "cookie", cookie);
      return rc;
    }

    /// <summary>Log in with a cookie (asynchronous)</summary>
    /// <remarks>
    /// <para>The cookie must have been created with 'make-cookie'.  The username is returned so the caller knows who they are.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void CookieAsync(string cookie, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Cookie(cookie);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Delete user</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// </remarks>
    public int Deluser(string user) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "deluser", user);
    }

    /// <summary>Delete user (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void DeluserAsync(string user, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Deluser(user);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List directories in a directory</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Dirs(out IList<string> files, string dir, string re) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "dirs", dir, re);
      files = lines;
      return rc;
    }

    /// <summary>List directories in a directory (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void DirsAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Dirs(out files, dir, re);
          if(callback != null) {
            try {
              callback(rc, files);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Disable play</summary>
    /// <remarks>
    /// <para>Play will stop at the end of the current track, if one is playing.  Requires the 'global prefs' right.</para>
    /// </remarks>
    public int Disable() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "disable");
    }

    /// <summary>Disable play (asynchronous)</summary>
    /// <remarks>
    /// <para>Play will stop at the end of the current track, if one is playing.  Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void DisableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Disable();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set a user property</summary>
    /// <remarks>
    /// <para>With the 'admin' right you can do anything.  Otherwise you need the 'userinfo' right and can only set 'email' and 'password'.</para>
    /// </remarks>
    public int Edituser(string username, string property, string value) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "edituser", username, property, value);
    }

    /// <summary>Set a user property (asynchronous)</summary>
    /// <remarks>
    /// <para>With the 'admin' right you can do anything.  Otherwise you need the 'userinfo' right and can only set 'email' and 'password'.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void EdituserAsync(string username, string property, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Edituser(username, property, value);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Enable play</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// </remarks>
    public int Enable() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "enable");
    }

    /// <summary>Enable play (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void EnableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Enable();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Detect whether play is enabled</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Enabled(out bool enabled) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    /// <summary>Detect whether play is enabled (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void EnabledAsync(Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool enabled;
          int rc = Enabled(out enabled);
          if(callback != null) {
            try {
              callback(rc, enabled);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Test whether a track exists</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Exists(out bool exists, string track) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "exists", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'exists'");
      exists = (bits[0] == "yes");
      return rc;
    }

    /// <summary>Test whether a track exists (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ExistsAsync(string track, Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool exists;
          int rc = Exists(out exists, track);
          if(callback != null) {
            try {
              callback(rc, exists);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List files in a directory</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Files(out IList<string> files, string dir, string re) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "files", dir, re);
      files = lines;
      return rc;
    }

    /// <summary>List files in a directory (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void FilesAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Files(out files, dir, re);
          if(callback != null) {
            try {
              callback(rc, files);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a track preference</summary>
    /// <remarks>
    /// <para>If the track does not exist that is an error.  If the track exists but the preference does not then a null value is returned.</para>
    /// </remarks>
    public int Get(out string value, string track, string pref) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "get", track, pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get'");
      value = bits[0];
      return rc;
    }

    /// <summary>Get a track preference (asynchronous)</summary>
    /// <remarks>
    /// <para>If the track does not exist that is an error.  If the track exists but the preference does not then a null value is returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void GetAsync(string track, string pref, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = Get(out value, track, pref);
          if(callback != null) {
            try {
              callback(rc, value);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a global preference</summary>
    /// <remarks>
    /// <para>If the preference does exist not then a null value is returned.</para>
    /// </remarks>
    public int GetGlobal(out string value, string pref) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "get-global", pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get-global'");
      value = bits[0];
      return rc;
    }

    /// <summary>Get a global preference (asynchronous)</summary>
    /// <remarks>
    /// <para>If the preference does exist not then a null value is returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void GetGlobalAsync(string pref, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = GetGlobal(out value, pref);
          if(callback != null) {
            try {
              callback(rc, value);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a track's length</summary>
    /// <remarks>
    /// <para>If the track does not exist an error is returned.</para>
    /// </remarks>
    public int Length(out int length, string track) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "length", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'length'");
      length = int.Parse(bits[0]);
      return rc;
    }

    /// <summary>Get a track's length (asynchronous)</summary>
    /// <remarks>
    /// <para>If the track does not exist an error is returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void LengthAsync(string track, Action<int,int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int length;
          int rc = Length(out length, track);
          if(callback != null) {
            try {
              callback(rc, length);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Create a login cookie for this user</summary>
    /// <remarks>
    /// <para>The cookie may be redeemed via the 'cookie' command</para>
    /// </remarks>
    public int MakeCookie(out string cookie) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "make-cookie");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'make-cookie'");
      cookie = bits[0];
      return rc;
    }

    /// <summary>Create a login cookie for this user (asynchronous)</summary>
    /// <remarks>
    /// <para>The cookie may be redeemed via the 'cookie' command</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void MakeCookieAsync(Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string cookie;
          int rc = MakeCookie(out cookie);
          if(callback != null) {
            try {
              callback(rc, cookie);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Move a track</summary>
    /// <remarks>
    /// <para>Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.</para>
    /// </remarks>
    public int Move(string track, int delta) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "move", track, delta);
    }

    /// <summary>Move a track (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void MoveAsync(string track, int delta, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Move(track, delta);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Move multiple tracks</summary>
    /// <remarks>
    /// <para>Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.</para>
    /// </remarks>
    public int Moveafter(string target, IList<string> ids) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "moveafter", target, ids);
    }

    /// <summary>Move multiple tracks (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires one of the 'move mine', 'move random' or 'move any' rights depending on how the track came to be added to the queue.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void MoveafterAsync(string target, IList<string> ids, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Moveafter(target, ids);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List recently added tracks</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int NewTracks(out IList<string> tracks, int max) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "new", max);
      tracks = lines;
      return rc;
    }

    /// <summary>List recently added tracks (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void NewTracksAsync(int max, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = NewTracks(out tracks, max);
          if(callback != null) {
            try {
              callback(rc, tracks);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Do nothing</summary>
    /// <remarks>
    /// <para>Used as a keepalive.  No authentication required.</para>
    /// </remarks>
    public int Nop() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "nop");
    }

    /// <summary>Do nothing (asynchronous)</summary>
    /// <remarks>
    /// <para>Used as a keepalive.  No authentication required.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void NopAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Nop();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a track name part</summary>
    /// <remarks>
    /// <para>If the name part cannot be constructed an empty string is returned.</para>
    /// </remarks>
    public int Part(out string part, string track, string context, string namepart) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "part", track, context, namepart);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'part'");
      part = bits[0];
      return rc;
    }

    /// <summary>Get a track name part (asynchronous)</summary>
    /// <remarks>
    /// <para>If the name part cannot be constructed an empty string is returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PartAsync(string track, string context, string namepart, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string part;
          int rc = Part(out part, track, context, namepart);
          if(callback != null) {
            try {
              callback(rc, part);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Pause the currently playing track</summary>
    /// <remarks>
    /// <para>Requires the 'pause' right.</para>
    /// </remarks>
    public int Pause() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "pause");
    }

    /// <summary>Pause the currently playing track (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'pause' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PauseAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Pause();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Play a track</summary>
    /// <remarks>
    /// <para>Requires the 'play' right.</para>
    /// </remarks>
    public int Play(out string id, string track) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "play", track);
      id = response;
      return rc;
    }

    /// <summary>Play a track (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlayAsync(string track, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string id;
          int rc = Play(out id, track);
          if(callback != null) {
            try {
              callback(rc, id);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Play multiple tracks</summary>
    /// <remarks>
    /// <para>Requires the 'play' right.</para>
    /// </remarks>
    public int Playafter(string target, IList<string> tracks) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playafter", target, tracks);
    }

    /// <summary>Play multiple tracks (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlayafterAsync(string target, IList<string> tracks, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Playafter(target, tracks);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Retrieve the playing track</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Playing(QueueEntry playing) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "playing");
      playing.Set(response);
      return rc;
    }

    /// <summary>Retrieve the playing track (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlayingAsync(Action<int,QueueEntry> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          QueueEntry playing = new QueueEntry();
          int rc = Playing(playing);
          if(callback != null) {
            try {
              callback(rc, playing);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Delete a playlist</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.</para>
    /// </remarks>
    public int PlaylistDelete(string playlist) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playlist-delete", playlist);
    }

    /// <summary>Delete a playlist (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistDeleteAsync(string playlist, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistDelete(playlist);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List the contents of a playlist</summary>
    /// <remarks>
    /// <para>Requires the 'read' right and oermission to read the playlist.</para>
    /// </remarks>
    public int PlaylistGet(out IList<string> tracks, string playlist) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "playlist-get", playlist);
      tracks = lines;
      return rc;
    }

    /// <summary>List the contents of a playlist (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'read' right and oermission to read the playlist.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistGetAsync(string playlist, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = PlaylistGet(out tracks, playlist);
          if(callback != null) {
            try {
              callback(rc, tracks);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a playlist's sharing status</summary>
    /// <remarks>
    /// <para>Requires the 'read' right and permission to read the playlist.</para>
    /// </remarks>
    public int PlaylistGetShare(out string share, string playlist) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "playlist-get-share", playlist);
      share = response;
      return rc;
    }

    /// <summary>Get a playlist's sharing status (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'read' right and permission to read the playlist.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistGetShareAsync(string playlist, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string share;
          int rc = PlaylistGetShare(out share, playlist);
          if(callback != null) {
            try {
              callback(rc, share);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Lock a playlist</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.  A given connection may lock at most one playlist.</para>
    /// </remarks>
    public int PlaylistLock(string playlist) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playlist-lock", playlist);
    }

    /// <summary>Lock a playlist (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.  A given connection may lock at most one playlist.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistLockAsync(string playlist, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistLock(playlist);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set the contents of a playlist</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist, which must be locked.</para>
    /// </remarks>
    public int PlaylistSet(string playlist, IList<string> tracks) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playlist-set", playlist, new SendAsBody(tracks));
    }

    /// <summary>Set the contents of a playlist (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist, which must be locked.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistSetAsync(string playlist, IList<string> tracks, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistSet(playlist, tracks);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set a playlist's sharing status</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.</para>
    /// </remarks>
    public int PlaylistSetShare(string playlist, string share) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playlist-set-share", playlist, share);
    }

    /// <summary>Set a playlist's sharing status (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'play' right and permission to modify the playlist.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistSetShareAsync(string playlist, string share, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistSetShare(playlist, share);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Unlock the locked playlist playlist</summary>
    /// <remarks>
    /// <para>The playlist to unlock is implicit in the connection.</para>
    /// </remarks>
    public int PlaylistUnlock() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "playlist-unlock");
    }

    /// <summary>Unlock the locked playlist playlist (asynchronous)</summary>
    /// <remarks>
    /// <para>The playlist to unlock is implicit in the connection.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistUnlockAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistUnlock();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List playlists</summary>
    /// <remarks>
    /// <para>Requires the 'read' right.  Only playlists that you have permission to read are returned.</para>
    /// </remarks>
    public int Playlists(out IList<string> playlists) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "playlists");
      playlists = lines;
      return rc;
    }

    /// <summary>List playlists (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'read' right.  Only playlists that you have permission to read are returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PlaylistsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> playlists = new List<string>();
          int rc = Playlists(out playlists);
          if(callback != null) {
            try {
              callback(rc, playlists);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get all the preferences for a track</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Prefs(out IDictionary<string,string> prefs, string track) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "prefs", track);
      prefs = BodyToPairs(lines);
      return rc;
    }

    /// <summary>Get all the preferences for a track (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void PrefsAsync(string track, Action<int,IDictionary<string,string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IDictionary<string,string> prefs = new Dictionary<string,string>();
          int rc = Prefs(out prefs, track);
          if(callback != null) {
            try {
              callback(rc, prefs);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List the queue</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Queue(out IList<QueueEntry> queue) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "queue");
      queue = BodyToQueue(lines);
      return rc;
    }

    /// <summary>List the queue (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void QueueAsync(Action<int,IList<QueueEntry>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<QueueEntry> queue = new List<QueueEntry>();
          int rc = Queue(out queue);
          if(callback != null) {
            try {
              callback(rc, queue);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Disable random play</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// </remarks>
    public int RandomDisable() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "random-disable");
    }

    /// <summary>Disable random play (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RandomDisableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RandomDisable();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Enable random play</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// </remarks>
    public int RandomEnable() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "random-enable");
    }

    /// <summary>Enable random play (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RandomEnableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RandomEnable();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Detect whether random play is enabled</summary>
    /// <remarks>
    /// <para>Random play counts as enabled even if play is disabled.</para>
    /// </remarks>
    public int RandomEnabled(out bool enabled) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "random-enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'random-enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    /// <summary>Detect whether random play is enabled (asynchronous)</summary>
    /// <remarks>
    /// <para>Random play counts as enabled even if play is disabled.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RandomEnabledAsync(Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool enabled;
          int rc = RandomEnabled(out enabled);
          if(callback != null) {
            try {
              callback(rc, enabled);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List recently played tracks</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Recent(out IList<QueueEntry> recent) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "recent");
      recent = BodyToQueue(lines);
      return rc;
    }

    /// <summary>List recently played tracks (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RecentAsync(Action<int,IList<QueueEntry>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<QueueEntry> recent = new List<QueueEntry>();
          int rc = Recent(out recent);
          if(callback != null) {
            try {
              callback(rc, recent);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Re-read configuraiton file.</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// </remarks>
    public int Reconfigure() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "reconfigure");
    }

    /// <summary>Re-read configuraiton file. (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ReconfigureAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Reconfigure();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Register a new user</summary>
    /// <remarks>
    /// <para>Requires the 'register' right which is usually only available to the 'guest' user.  Redeem the confirmation string via 'confirm' to complete registration.</para>
    /// </remarks>
    public int Register(out string confirmation, string username, string password, string email) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "register", username, password, email);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'register'");
      confirmation = bits[0];
      return rc;
    }

    /// <summary>Register a new user (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'register' right which is usually only available to the 'guest' user.  Redeem the confirmation string via 'confirm' to complete registration.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RegisterAsync(string username, string password, string email, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string confirmation;
          int rc = Register(out confirmation, username, password, email);
          if(callback != null) {
            try {
              callback(rc, confirmation);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Send a password reminder.</summary>
    /// <remarks>
    /// <para>If the user has no valid email address, or no password, or a reminder has been sent too recently, then no reminder will be sent.</para>
    /// </remarks>
    public int Reminder(string username) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "reminder", username);
    }

    /// <summary>Send a password reminder. (asynchronous)</summary>
    /// <remarks>
    /// <para>If the user has no valid email address, or no password, or a reminder has been sent too recently, then no reminder will be sent.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ReminderAsync(string username, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Reminder(username);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Remove a track form the queue.</summary>
    /// <remarks>
    /// <para>Requires one of the 'remove mine', 'remove random' or 'remove any' rights depending on how the track came to be added to the queue.</para>
    /// </remarks>
    public int Remove(string id) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "remove", id);
    }

    /// <summary>Remove a track form the queue. (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires one of the 'remove mine', 'remove random' or 'remove any' rights depending on how the track came to be added to the queue.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RemoveAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Remove(id);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Rescan all collections for new or obsolete tracks.</summary>
    /// <remarks>
    /// <para>Requires the 'rescan' right.</para>
    /// </remarks>
    public int Rescan() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "rescan");
    }

    /// <summary>Rescan all collections for new or obsolete tracks. (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'rescan' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RescanAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Rescan();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Resolve a track name</summary>
    /// <remarks>
    /// <para>Converts aliases to non-alias track names</para>
    /// </remarks>
    public int Resolve(out string resolved, string track) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "resolve", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'resolve'");
      resolved = bits[0];
      return rc;
    }

    /// <summary>Resolve a track name (asynchronous)</summary>
    /// <remarks>
    /// <para>Converts aliases to non-alias track names</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ResolveAsync(string track, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string resolved;
          int rc = Resolve(out resolved, track);
          if(callback != null) {
            try {
              callback(rc, resolved);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Resume the currently playing track</summary>
    /// <remarks>
    /// <para>Requires the 'pause' right.</para>
    /// </remarks>
    public int Resume() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "resume");
    }

    /// <summary>Resume the currently playing track (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'pause' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ResumeAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Resume();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Revoke a cookie.</summary>
    /// <remarks>
    /// <para>It will not subsequently be possible to log in with the cookie.</para>
    /// </remarks>
    public int Revoke() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "revoke");
    }

    /// <summary>Revoke a cookie. (asynchronous)</summary>
    /// <remarks>
    /// <para>It will not subsequently be possible to log in with the cookie.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RevokeAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Revoke();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get the server's RTP address information</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int RtpAddress(out string address, out string port) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "rtp-address");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'rtp-address'");
      address = bits[0];
      port = bits[1];
      return rc;
    }

    /// <summary>Get the server's RTP address information (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RtpAddressAsync(Action<int,string,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string address;
          string port;
          int rc = RtpAddress(out address, out port);
          if(callback != null) {
            try {
              callback(rc, address, port);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Cancel RTP stream</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int RtpCancel() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "rtp-cancel");
    }

    /// <summary>Cancel RTP stream (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RtpCancelAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RtpCancel();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Request a unicast RTP stream</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int RtpRequest(string address, string port) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "rtp-request", address, port);
    }

    /// <summary>Request a unicast RTP stream (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void RtpRequestAsync(string address, string port, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RtpRequest(address, port);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Terminate the playing track.</summary>
    /// <remarks>
    /// <para>Requires one of the 'scratch mine', 'scratch random' or 'scratch any' rights depending on how the track came to be added to the queue.</para>
    /// </remarks>
    public int Scratch(string id) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "scratch", id);
    }

    /// <summary>Terminate the playing track. (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires one of the 'scratch mine', 'scratch random' or 'scratch any' rights depending on how the track came to be added to the queue.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScratchAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Scratch(id);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Schedule a track to play in the future</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int ScheduleAddPlay(DateTime when, string priority, string track) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "schedule-add", when, priority, "play", track);
    }

    /// <summary>Schedule a track to play in the future (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleAddPlayAsync(DateTime when, string priority, string track, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddPlay(when, priority, track);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Schedule a global setting to be changed in the future</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int ScheduleAddSetGlobal(DateTime when, string priority, string pref, string value) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "schedule-add", when, priority, "set-global", pref, value);
    }

    /// <summary>Schedule a global setting to be changed in the future (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleAddSetGlobalAsync(DateTime when, string priority, string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddSetGlobal(when, priority, pref, value);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Schedule a global setting to be unset in the future</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int ScheduleAddUnsetGlobal(DateTime when, string priority, string pref) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "schedule-add", when, priority, "set-global", pref);
    }

    /// <summary>Schedule a global setting to be unset in the future (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleAddUnsetGlobalAsync(DateTime when, string priority, string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddUnsetGlobal(when, priority, pref);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Delete a scheduled event.</summary>
    /// <remarks>
    /// <para>Users can always delete their own scheduled events; with the admin right you can delete any event.</para>
    /// </remarks>
    public int ScheduleDel(string id) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "schedule-del", id);
    }

    /// <summary>Delete a scheduled event. (asynchronous)</summary>
    /// <remarks>
    /// <para>Users can always delete their own scheduled events; with the admin right you can delete any event.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleDelAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleDel(id);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get the details of scheduled event</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int ScheduleGet(out IDictionary<string,string> actiondata, string id) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "schedule-get", id);
      actiondata = BodyToPairs(lines);
      return rc;
    }

    /// <summary>Get the details of scheduled event (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleGetAsync(string id, Action<int,IDictionary<string,string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IDictionary<string,string> actiondata = new Dictionary<string,string>();
          int rc = ScheduleGet(out actiondata, id);
          if(callback != null) {
            try {
              callback(rc, actiondata);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>List scheduled events</summary>
    /// <remarks>
    /// <para>This just lists IDs.  Use 'schedule-get' to retrieve more detail</para>
    /// </remarks>
    public int ScheduleList(out IList<string> ids) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "schedule-list");
      ids = lines;
      return rc;
    }

    /// <summary>List scheduled events (asynchronous)</summary>
    /// <remarks>
    /// <para>This just lists IDs.  Use 'schedule-get' to retrieve more detail</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ScheduleListAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> ids = new List<string>();
          int rc = ScheduleList(out ids);
          if(callback != null) {
            try {
              callback(rc, ids);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Search for tracks</summary>
    /// <remarks>
    /// <para>Terms are either keywords or tags formatted as 'tag:TAG-NAME'.</para>
    /// </remarks>
    public int Search(out IList<string> tracks, string terms) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "search", terms);
      tracks = lines;
      return rc;
    }

    /// <summary>Search for tracks (asynchronous)</summary>
    /// <remarks>
    /// <para>Terms are either keywords or tags formatted as 'tag:TAG-NAME'.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void SearchAsync(string terms, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = Search(out tracks, terms);
          if(callback != null) {
            try {
              callback(rc, tracks);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set a track preference</summary>
    /// <remarks>
    /// <para>Requires the 'prefs' right.</para>
    /// </remarks>
    public int Set(string track, string pref, string value) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "set", track, pref, value);
    }

    /// <summary>Set a track preference (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void SetAsync(string track, string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Set(track, pref, value);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set a global preference</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// </remarks>
    public int SetGlobal(string pref, string value) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "set-global", pref, value);
    }

    /// <summary>Set a global preference (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void SetGlobalAsync(string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = SetGlobal(pref, value);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Request server shutdown</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// </remarks>
    public int Shutdown() {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "shutdown");
    }

    /// <summary>Request server shutdown (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'admin' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void ShutdownAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Shutdown();
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get server statistics</summary>
    /// <remarks>
    /// <para>The details of what the server reports are not really defined.  The returned strings are intended to be printed out one to a line.</para>
    /// </remarks>
    public int Stats(out IList<string> stats) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "stats");
      stats = lines;
      return rc;
    }

    /// <summary>Get server statistics (asynchronous)</summary>
    /// <remarks>
    /// <para>The details of what the server reports are not really defined.  The returned strings are intended to be printed out one to a line.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void StatsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> stats = new List<string>();
          int rc = Stats(out stats);
          if(callback != null) {
            try {
              callback(rc, stats);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a list of known tags</summary>
    /// <remarks>
    /// <para>Only tags which apply to at least one track are returned.</para>
    /// </remarks>
    public int Tags(out IList<string> tags) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "tags");
      tags = lines;
      return rc;
    }

    /// <summary>Get a list of known tags (asynchronous)</summary>
    /// <remarks>
    /// <para>Only tags which apply to at least one track are returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void TagsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tags = new List<string>();
          int rc = Tags(out tags);
          if(callback != null) {
            try {
              callback(rc, tags);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Unset a track preference</summary>
    /// <remarks>
    /// <para>Requires the 'prefs' right.</para>
    /// </remarks>
    public int Unset(string track, string pref) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "unset", track, pref);
    }

    /// <summary>Unset a track preference (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void UnsetAsync(string track, string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Unset(track, pref);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set a global preference</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// </remarks>
    public int UnsetGlobal(string pref) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "unset-global", pref);
    }

    /// <summary>Set a global preference (asynchronous)</summary>
    /// <remarks>
    /// <para>Requires the 'global prefs' right.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void UnsetGlobalAsync(string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = UnsetGlobal(pref);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a user property.</summary>
    /// <remarks>
    /// <para>If the user does not exist an error is returned, if the user exists but the property does not then a null value is returned.</para>
    /// </remarks>
    public int Userinfo(out string value, string username, string property) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "userinfo", username, property);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'userinfo'");
      value = bits[0];
      return rc;
    }

    /// <summary>Get a user property. (asynchronous)</summary>
    /// <remarks>
    /// <para>If the user does not exist an error is returned, if the user exists but the property does not then a null value is returned.</para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void UserinfoAsync(string username, string property, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = Userinfo(out value, username, property);
          if(callback != null) {
            try {
              callback(rc, value);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get a list of users</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Users(out IList<string> users) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "users");
      users = lines;
      return rc;
    }

    /// <summary>Get a list of users (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void UsersAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> users = new List<string>();
          int rc = Users(out users);
          if(callback != null) {
            try {
              callback(rc, users);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get the server version</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int Version(out string version) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "version");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'version'");
      version = bits[0];
      return rc;
    }

    /// <summary>Get the server version (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void VersionAsync(Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string version;
          int rc = Version(out version);
          if(callback != null) {
            try {
              callback(rc, version);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Set the volume</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int SetVolume(int left, int right) {
      IList<string> lines;
      string response;
      return Transact(out response, out lines, "volume", left, right);
    }

    /// <summary>Set the volume (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void SetVolumeAsync(int left, int right, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = SetVolume(left, right);
          if(callback != null) {
            try {
              callback(rc);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    /// <summary>Get the volume</summary>
    /// <remarks>
    /// <para></para>
    /// </remarks>
    public int GetVolume(out int left, out int right) {
      IList<string> lines;
      string response;
      int rc = Transact(out response, out lines, "volume");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'volume'");
      left = int.Parse(bits[0]);
      right = int.Parse(bits[1]);
      return rc;
    }

    /// <summary>Get the volume (asynchronous)</summary>
    /// <remarks>
    /// <para></para>
    /// <para>On success, calls callback in a background thread, if it is not null.</para>
    /// <para>On error, calls exception in a background thread, if it is not null.</para>
    /// </remarks>
    public void GetVolumeAsync(Action<int,int,int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int left;
          int right;
          int rc = GetVolume(out left, out right);
          if(callback != null) {
            try {
              callback(rc, left, right);
            } catch { /* nom */ }
          }
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

  }
}
