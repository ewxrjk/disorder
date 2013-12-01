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
    public int Adopt(string id) {
      string response;
      return Transact(out response, "adopt", id);
    }

    public void AdoptAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Adopt(id);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Adduser(string user, string password, string rights) {
      string response;
      return Transact(out response, "adduser", user, password, rights);
    }

    public void AdduserAsync(string user, string password, string rights, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Adduser(user, password, rights);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Allfiles(IList<string> files, string dir, string re) {
      string response;
      int rc = Transact(out response, "allfiles", dir, re);
      WaitBody(files);
      return rc;
    }

    public void AllfilesAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Allfiles(files, dir, re);
          if(callback != null)
            callback(rc, files);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Confirm(string confirmation) {
      string response;
      int rc = Transact(out response, "confirm", confirmation);
      return rc;
    }

    public void ConfirmAsync(string confirmation, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Confirm(confirmation);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Cookie(string cookie) {
      string response;
      int rc = Transact(out response, "cookie", cookie);
      return rc;
    }

    public void CookieAsync(string cookie, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Cookie(cookie);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Deluser(string user) {
      string response;
      return Transact(out response, "deluser", user);
    }

    public void DeluserAsync(string user, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Deluser(user);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Dirs(IList<string> files, string dir, string re) {
      string response;
      int rc = Transact(out response, "dirs", dir, re);
      WaitBody(files);
      return rc;
    }

    public void DirsAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Dirs(files, dir, re);
          if(callback != null)
            callback(rc, files);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Disable() {
      string response;
      return Transact(out response, "disable");
    }

    public void DisableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Disable();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Edituser(string username, string property, string value) {
      string response;
      return Transact(out response, "edituser", username, property, value);
    }

    public void EdituserAsync(string username, string property, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Edituser(username, property, value);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Enable() {
      string response;
      return Transact(out response, "enable");
    }

    public void EnableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Enable();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Enabled(out bool enabled) {
      string response;
      int rc = Transact(out response, "enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    public void EnabledAsync(Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool enabled;
          int rc = Enabled(out enabled);
          if(callback != null)
            callback(rc, enabled);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Exists(out bool exists, string track) {
      string response;
      int rc = Transact(out response, "exists", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'exists'");
      exists = (bits[0] == "yes");
      return rc;
    }

    public void ExistsAsync(string track, Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool exists;
          int rc = Exists(out exists, track);
          if(callback != null)
            callback(rc, exists);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Files(IList<string> files, string dir, string re) {
      string response;
      int rc = Transact(out response, "files", dir, re);
      WaitBody(files);
      return rc;
    }

    public void FilesAsync(string dir, string re, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> files = new List<string>();
          int rc = Files(files, dir, re);
          if(callback != null)
            callback(rc, files);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Get(out string value, string track, string pref) {
      string response;
      int rc = Transact(out response, "get", track, pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get'");
      value = bits[0];
      return rc;
    }

    public void GetAsync(string track, string pref, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = Get(out value, track, pref);
          if(callback != null)
            callback(rc, value);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int GetGlobal(out string value, string pref) {
      string response;
      int rc = Transact(out response, "get-global", pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get-global'");
      value = bits[0];
      return rc;
    }

    public void GetGlobalAsync(string pref, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = GetGlobal(out value, pref);
          if(callback != null)
            callback(rc, value);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Length(out int length, string track) {
      string response;
      int rc = Transact(out response, "length", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'length'");
      length = int.Parse(bits[0]);
      return rc;
    }

    public void LengthAsync(string track, Action<int,int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int length;
          int rc = Length(out length, track);
          if(callback != null)
            callback(rc, length);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int MakeCookie(out string cookie) {
      string response;
      int rc = Transact(out response, "make-cookie");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'make-cookie'");
      cookie = bits[0];
      return rc;
    }

    public void MakeCookieAsync(Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string cookie;
          int rc = MakeCookie(out cookie);
          if(callback != null)
            callback(rc, cookie);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Move(string track, int delta) {
      string response;
      return Transact(out response, "move", track, delta);
    }

    public void MoveAsync(string track, int delta, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Move(track, delta);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Moveafter(string target, IList<string> ids) {
      string response;
      return Transact(out response, "moveafter", target, ids);
    }

    public void MoveafterAsync(string target, IList<string> ids, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Moveafter(target, ids);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int NewTracks(IList<string> tracks, int max) {
      string response;
      int rc = Transact(out response, "new", max);
      WaitBody(tracks);
      return rc;
    }

    public void NewTracksAsync(int max, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = NewTracks(tracks, max);
          if(callback != null)
            callback(rc, tracks);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Nop() {
      string response;
      return Transact(out response, "nop");
    }

    public void NopAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Nop();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Part(out string part, string track, string context, string namepart) {
      string response;
      int rc = Transact(out response, "part", track, context, namepart);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'part'");
      part = bits[0];
      return rc;
    }

    public void PartAsync(string track, string context, string namepart, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string part;
          int rc = Part(out part, track, context, namepart);
          if(callback != null)
            callback(rc, part);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Pause() {
      string response;
      return Transact(out response, "pause");
    }

    public void PauseAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Pause();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Play(out string id, string track) {
      string response;
      int rc = Transact(out response, "play", track);
      id = response;
      return rc;
    }

    public void PlayAsync(string track, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string id;
          int rc = Play(out id, track);
          if(callback != null)
            callback(rc, id);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Playafter(string target, IList<string> tracks) {
      string response;
      return Transact(out response, "playafter", target, tracks);
    }

    public void PlayafterAsync(string target, IList<string> tracks, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Playafter(target, tracks);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Playing(QueueEntry playing) {
      string response;
      int rc = Transact(out response, "playing");
      playing.Set(response);
      return rc;
    }

    public void PlayingAsync(Action<int,QueueEntry> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          QueueEntry playing = new QueueEntry();
          int rc = Playing(playing);
          if(callback != null)
            callback(rc, playing);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistDelete(string playlist) {
      string response;
      return Transact(out response, "playlist-delete", playlist);
    }

    public void PlaylistDeleteAsync(string playlist, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistDelete(playlist);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistGet(IList<string> tracks, string playlist) {
      string response;
      int rc = Transact(out response, "playlist-get", playlist);
      WaitBody(tracks);
      return rc;
    }

    public void PlaylistGetAsync(string playlist, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = PlaylistGet(tracks, playlist);
          if(callback != null)
            callback(rc, tracks);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistGetShare(out string share, string playlist) {
      string response;
      int rc = Transact(out response, "playlist-get-share", playlist);
      share = response;
      return rc;
    }

    public void PlaylistGetShareAsync(string playlist, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string share;
          int rc = PlaylistGetShare(out share, playlist);
          if(callback != null)
            callback(rc, share);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistLock(string playlist) {
      string response;
      return Transact(out response, "playlist-lock", playlist);
    }

    public void PlaylistLockAsync(string playlist, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistLock(playlist);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistSet(string playlist, IList<string> tracks) {
      string response;
      return Transact(out response, "playlist-set", playlist, new SendAsBody(tracks));
    }

    public void PlaylistSetAsync(string playlist, IList<string> tracks, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistSet(playlist, tracks);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistSetShare(string playlist, string share) {
      string response;
      return Transact(out response, "playlist-set-share", playlist, share);
    }

    public void PlaylistSetShareAsync(string playlist, string share, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistSetShare(playlist, share);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int PlaylistUnlock() {
      string response;
      return Transact(out response, "playlist-unlock");
    }

    public void PlaylistUnlockAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = PlaylistUnlock();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Playlists(IList<string> playlists) {
      string response;
      int rc = Transact(out response, "playlists");
      WaitBody(playlists);
      return rc;
    }

    public void PlaylistsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> playlists = new List<string>();
          int rc = Playlists(playlists);
          if(callback != null)
            callback(rc, playlists);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Prefs(IDictionary<string,string> prefs, string track) {
      string response;
      int rc = Transact(out response, "prefs", track);
      WaitBodyPairs(prefs);
      return rc;
    }

    public void PrefsAsync(string track, Action<int,IDictionary<string,string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IDictionary<string,string> prefs = new Dictionary<string,string>();
          int rc = Prefs(prefs, track);
          if(callback != null)
            callback(rc, prefs);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Queue(IList<QueueEntry> queue) {
      string response;
      int rc = Transact(out response, "queue");
      WaitBodyQueue(queue);
      return rc;
    }

    public void QueueAsync(Action<int,IList<QueueEntry>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<QueueEntry> queue = new List<QueueEntry>();
          int rc = Queue(queue);
          if(callback != null)
            callback(rc, queue);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RandomDisable() {
      string response;
      return Transact(out response, "random-disable");
    }

    public void RandomDisableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RandomDisable();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RandomEnable() {
      string response;
      return Transact(out response, "random-enable");
    }

    public void RandomEnableAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RandomEnable();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RandomEnabled(out bool enabled) {
      string response;
      int rc = Transact(out response, "random-enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'random-enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    public void RandomEnabledAsync(Action<int,bool> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          bool enabled;
          int rc = RandomEnabled(out enabled);
          if(callback != null)
            callback(rc, enabled);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Recent(IList<QueueEntry> recent) {
      string response;
      int rc = Transact(out response, "recent");
      WaitBodyQueue(recent);
      return rc;
    }

    public void RecentAsync(Action<int,IList<QueueEntry>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<QueueEntry> recent = new List<QueueEntry>();
          int rc = Recent(recent);
          if(callback != null)
            callback(rc, recent);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Reconfigure() {
      string response;
      return Transact(out response, "reconfigure");
    }

    public void ReconfigureAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Reconfigure();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Register(out string confirmation, string username, string password, string email) {
      string response;
      int rc = Transact(out response, "register", username, password, email);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'register'");
      confirmation = bits[0];
      return rc;
    }

    public void RegisterAsync(string username, string password, string email, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string confirmation;
          int rc = Register(out confirmation, username, password, email);
          if(callback != null)
            callback(rc, confirmation);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Reminder(string username) {
      string response;
      return Transact(out response, "reminder", username);
    }

    public void ReminderAsync(string username, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Reminder(username);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Remove(string id) {
      string response;
      return Transact(out response, "remove", id);
    }

    public void RemoveAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Remove(id);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Rescan() {
      string response;
      return Transact(out response, "rescan");
    }

    public void RescanAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Rescan();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Resolve(out string resolved, string track) {
      string response;
      int rc = Transact(out response, "resolve", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'resolve'");
      resolved = bits[0];
      return rc;
    }

    public void ResolveAsync(string track, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string resolved;
          int rc = Resolve(out resolved, track);
          if(callback != null)
            callback(rc, resolved);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Resume() {
      string response;
      return Transact(out response, "resume");
    }

    public void ResumeAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Resume();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Revoke() {
      string response;
      return Transact(out response, "revoke");
    }

    public void RevokeAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Revoke();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RtpAddress(out string address, out string port) {
      string response;
      int rc = Transact(out response, "rtp-address");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'rtp-address'");
      address = bits[0];
      port = bits[1];
      return rc;
    }

    public void RtpAddressAsync(Action<int,string,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string address;
          string port;
          int rc = RtpAddress(out address, out port);
          if(callback != null)
            callback(rc, address, port);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RtpCancel() {
      string response;
      return Transact(out response, "rtp-cancel");
    }

    public void RtpCancelAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RtpCancel();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int RtpRequest(string address, string port) {
      string response;
      return Transact(out response, "rtp-request", address, port);
    }

    public void RtpRequestAsync(string address, string port, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = RtpRequest(address, port);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Scratch(string id) {
      string response;
      return Transact(out response, "scratch", id);
    }

    public void ScratchAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Scratch(id);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleAddPlay(DateTime when, string priority, string track) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "play", track);
    }

    public void ScheduleAddPlayAsync(DateTime when, string priority, string track, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddPlay(when, priority, track);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleAddSetGlobal(DateTime when, string priority, string pref, string value) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "set-global", pref, value);
    }

    public void ScheduleAddSetGlobalAsync(DateTime when, string priority, string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddSetGlobal(when, priority, pref, value);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleAddUnsetGlobal(DateTime when, string priority, string pref) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "set-global", pref);
    }

    public void ScheduleAddUnsetGlobalAsync(DateTime when, string priority, string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleAddUnsetGlobal(when, priority, pref);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleDel(string id) {
      string response;
      return Transact(out response, "schedule-del", id);
    }

    public void ScheduleDelAsync(string id, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = ScheduleDel(id);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleGet(IDictionary<string,string> actiondata, string id) {
      string response;
      int rc = Transact(out response, "schedule-get", id);
      WaitBodyPairs(actiondata);
      return rc;
    }

    public void ScheduleGetAsync(string id, Action<int,IDictionary<string,string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IDictionary<string,string> actiondata = new Dictionary<string,string>();
          int rc = ScheduleGet(actiondata, id);
          if(callback != null)
            callback(rc, actiondata);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int ScheduleList(IList<string> ids) {
      string response;
      int rc = Transact(out response, "schedule-list");
      WaitBody(ids);
      return rc;
    }

    public void ScheduleListAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> ids = new List<string>();
          int rc = ScheduleList(ids);
          if(callback != null)
            callback(rc, ids);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Search(IList<string> tracks, string terms) {
      string response;
      int rc = Transact(out response, "search", terms);
      WaitBody(tracks);
      return rc;
    }

    public void SearchAsync(string terms, Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tracks = new List<string>();
          int rc = Search(tracks, terms);
          if(callback != null)
            callback(rc, tracks);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Set(string track, string pref, string value) {
      string response;
      return Transact(out response, "set", track, pref, value);
    }

    public void SetAsync(string track, string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Set(track, pref, value);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int SetGlobal(string pref, string value) {
      string response;
      return Transact(out response, "set-global", pref, value);
    }

    public void SetGlobalAsync(string pref, string value, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = SetGlobal(pref, value);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Shutdown() {
      string response;
      return Transact(out response, "shutdown");
    }

    public void ShutdownAsync(Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Shutdown();
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Stats(IList<string> stats) {
      string response;
      int rc = Transact(out response, "stats");
      WaitBody(stats);
      return rc;
    }

    public void StatsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> stats = new List<string>();
          int rc = Stats(stats);
          if(callback != null)
            callback(rc, stats);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Tags(IList<string> tags) {
      string response;
      int rc = Transact(out response, "tags");
      WaitBody(tags);
      return rc;
    }

    public void TagsAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> tags = new List<string>();
          int rc = Tags(tags);
          if(callback != null)
            callback(rc, tags);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Unset(string track, string pref) {
      string response;
      return Transact(out response, "unset", track, pref);
    }

    public void UnsetAsync(string track, string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = Unset(track, pref);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int UnsetGlobal(string pref) {
      string response;
      return Transact(out response, "unset-global", pref);
    }

    public void UnsetGlobalAsync(string pref, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = UnsetGlobal(pref);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Userinfo(out string value, string username, string property) {
      string response;
      int rc = Transact(out response, "userinfo", username, property);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'userinfo'");
      value = bits[0];
      return rc;
    }

    public void UserinfoAsync(string username, string property, Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string value;
          int rc = Userinfo(out value, username, property);
          if(callback != null)
            callback(rc, value);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Users(IList<string> users) {
      string response;
      int rc = Transact(out response, "users");
      WaitBody(users);
      return rc;
    }

    public void UsersAsync(Action<int,IList<string>> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          IList<string> users = new List<string>();
          int rc = Users(users);
          if(callback != null)
            callback(rc, users);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int Version(out string version) {
      string response;
      int rc = Transact(out response, "version");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'version'");
      version = bits[0];
      return rc;
    }

    public void VersionAsync(Action<int,string> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          string version;
          int rc = Version(out version);
          if(callback != null)
            callback(rc, version);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int SetVolume(int left, int right) {
      string response;
      return Transact(out response, "volume", left, right);
    }

    public void SetVolumeAsync(int left, int right, Action<int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int rc = SetVolume(left, right);
          if(callback != null)
            callback(rc);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

    public int GetVolume(out int left, out int right) {
      string response;
      int rc = Transact(out response, "volume");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'volume'");
      left = int.Parse(bits[0]);
      right = int.Parse(bits[1]);
      return rc;
    }

    public void GetVolumeAsync(Action<int,int,int> callback = null, Action<Exception> exception = null) {
      ThreadPool.QueueUserWorkItem((_) => {
        try {
          int left;
          int right;
          int rc = GetVolume(out left, out right);
          if(callback != null)
            callback(rc, left, right);
        } catch(Exception e) {
          if(exception != null)
            exception(e);
        }
      });
    }

  }
}
