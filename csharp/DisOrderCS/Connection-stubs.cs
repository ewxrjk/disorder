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

namespace uk.org.greenend.DisOrder
{
  public partial class Connection
  {
    public int Adopt(string id) {
      string response;
      return Transact(out response, "adopt", id);
    }

    public int Adduser(string user, string password, string rights) {
      string response;
      return Transact(out response, "adduser", user, password, rights);
    }

    public int Allfiles(IList<string> files, string dir, string re) {
      files.Clear();
      string response;
      int rc = Transact(out response, "allfiles", dir, re);
      WaitBody(files);
      return rc;
    }

    // Confirm not yet implemented

    // Cookie not yet implemented

    public int Deluser(string user) {
      string response;
      return Transact(out response, "deluser", user);
    }

    public int Dirs(IList<string> files, string dir, string re) {
      files.Clear();
      string response;
      int rc = Transact(out response, "dirs", dir, re);
      WaitBody(files);
      return rc;
    }

    public int Disable() {
      string response;
      return Transact(out response, "disable");
    }

    public int Edituser(string username, string property, string value) {
      string response;
      return Transact(out response, "edituser", username, property, value);
    }

    public int Enable() {
      string response;
      return Transact(out response, "enable");
    }

    public int Enabled(out bool enabled) {
      enabled = false;
      string response;
      int rc = Transact(out response, "enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    public int Exists(out bool exists, string track) {
      exists = false;
      string response;
      int rc = Transact(out response, "exists", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'exists'");
      exists = (bits[0] == "yes");
      return rc;
    }

    public int Files(IList<string> files, string dir, string re) {
      files.Clear();
      string response;
      int rc = Transact(out response, "files", dir, re);
      WaitBody(files);
      return rc;
    }

    public int Get(out string value, string track, string pref) {
      value = null;
      string response;
      int rc = Transact(out response, "get", track, pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get'");
      value = bits[0];
      return rc;
    }

    public int GetGlobal(out string value, string pref) {
      value = null;
      string response;
      int rc = Transact(out response, "get-global", pref);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'get-global'");
      value = bits[0];
      return rc;
    }

    public int Length(out int length, string track) {
      length = 0;
      string response;
      int rc = Transact(out response, "length", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'length'");
      length = int.Parse(bits[0]);
      return rc;
    }

    public int MakeCookie(out string cookie) {
      cookie = null;
      string response;
      int rc = Transact(out response, "make-cookie");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'make-cookie'");
      cookie = bits[0];
      return rc;
    }

    public int Move(string track, int delta) {
      string response;
      return Transact(out response, "move", track, delta);
    }

    // Moveafter not yet implemented

    public int NewTracks(IList<string> tracks, int max) {
      tracks.Clear();
      string response;
      int rc = Transact(out response, "new", max);
      WaitBody(tracks);
      return rc;
    }

    public int Nop() {
      string response;
      return Transact(out response, "nop");
    }

    public int Part(out string part, string track, string context, string namepart) {
      part = null;
      string response;
      int rc = Transact(out response, "part", track, context, namepart);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'part'");
      part = bits[0];
      return rc;
    }

    public int Pause() {
      string response;
      return Transact(out response, "pause");
    }

    public int Play(out string id, string track) {
      id = null;
      string response;
      int rc = Transact(out response, "play", track);
      id = response;
      return rc;
    }

    // Playafter not yet implemented

    public int Playing(QueueEntry playing) {
      string response;
      int rc = Transact(out response, "playing");
      playing.Set(response);
      return rc;
    }

    public int PlaylistDelete(string playlist) {
      string response;
      return Transact(out response, "playlist-delete", playlist);
    }

    public int PlaylistGet(IList<string> tracks, string playlist) {
      tracks.Clear();
      string response;
      int rc = Transact(out response, "playlist-get", playlist);
      WaitBody(tracks);
      return rc;
    }

    public int PlaylistGetShare(out string share, string playlist) {
      share = null;
      string response;
      int rc = Transact(out response, "playlist-get-share", playlist);
      share = response;
      return rc;
    }

    public int PlaylistLock(string playlist) {
      string response;
      return Transact(out response, "playlist-lock", playlist);
    }

    // PlaylistSet not yet implemented

    public int PlaylistSetShare(string playlist, string share) {
      string response;
      return Transact(out response, "playlist-set-share", playlist, share);
    }

    public int PlaylistUnlock() {
      string response;
      return Transact(out response, "playlist-unlock");
    }

    public int Playlists(IList<string> playlists) {
      playlists.Clear();
      string response;
      int rc = Transact(out response, "playlists");
      WaitBody(playlists);
      return rc;
    }

    // Prefs not yet implemented

    public int Queue(IList<QueueEntry> queue) {
      queue.Clear();
      string response;
      int rc = Transact(out response, "queue");
      WaitBodyQueue(queue);
      return rc;
    }

    public int RandomDisable() {
      string response;
      return Transact(out response, "random-disable");
    }

    public int RandomEnable() {
      string response;
      return Transact(out response, "random-enable");
    }

    public int RandomEnabled(out bool enabled) {
      enabled = false;
      string response;
      int rc = Transact(out response, "random-enabled");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'random-enabled'");
      enabled = (bits[0] == "yes");
      return rc;
    }

    public int Recent(IList<QueueEntry> recent) {
      recent.Clear();
      string response;
      int rc = Transact(out response, "recent");
      WaitBodyQueue(recent);
      return rc;
    }

    public int Reconfigure() {
      string response;
      return Transact(out response, "reconfigure");
    }

    public int Register(out string confirmation, string username, string password, string email) {
      confirmation = null;
      string response;
      int rc = Transact(out response, "register", username, password, email);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'register'");
      confirmation = bits[0];
      return rc;
    }

    public int Reminder(string username) {
      string response;
      return Transact(out response, "reminder", username);
    }

    public int Remove(string id) {
      string response;
      return Transact(out response, "remove", id);
    }

    public int Rescan() {
      string response;
      return Transact(out response, "rescan");
    }

    public int Resolve(out string resolved, string track) {
      resolved = null;
      string response;
      int rc = Transact(out response, "resolve", track);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'resolve'");
      resolved = bits[0];
      return rc;
    }

    public int Resume() {
      string response;
      return Transact(out response, "resume");
    }

    public int Revoke() {
      string response;
      return Transact(out response, "revoke");
    }

    public int RtpAddress(out string address, out string port) {
      address = null;
      port = null;
      string response;
      int rc = Transact(out response, "rtp-address");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'rtp-address'");
      address = bits[0];
      port = bits[1];
      return rc;
    }

    public int RtpCancel() {
      string response;
      return Transact(out response, "rtp-cancel");
    }

    public int RtpRequest(string address, string port) {
      string response;
      return Transact(out response, "rtp-request", address, port);
    }

    public int Scratch(string id) {
      string response;
      return Transact(out response, "scratch", id);
    }

    public int ScheduleAddPlay(DateTime when, string priority, string track) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "play", track);
    }

    public int ScheduleAddSetGlobal(DateTime when, string priority, string pref, string value) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "set-global", pref, value);
    }

    public int ScheduleAddUnsetGlobal(DateTime when, string priority, string pref) {
      string response;
      return Transact(out response, "schedule-add", when, priority, "set-global", pref);
    }

    public int ScheduleDel(string id) {
      string response;
      return Transact(out response, "schedule-del", id);
    }

    // ScheduleGet not yet implemented

    public int ScheduleList(IList<string> ids) {
      ids.Clear();
      string response;
      int rc = Transact(out response, "schedule-list");
      WaitBody(ids);
      return rc;
    }

    public int Search(IList<string> tracks, string terms) {
      tracks.Clear();
      string response;
      int rc = Transact(out response, "search", terms);
      WaitBody(tracks);
      return rc;
    }

    public int Set(string track, string pref, string value) {
      string response;
      return Transact(out response, "set", track, pref, value);
    }

    public int SetGlobal(string pref, string value) {
      string response;
      return Transact(out response, "set-global", pref, value);
    }

    public int Shutdown() {
      string response;
      return Transact(out response, "shutdown");
    }

    public int Stats(IList<string> stats) {
      stats.Clear();
      string response;
      int rc = Transact(out response, "stats");
      WaitBody(stats);
      return rc;
    }

    public int Tags(IList<string> tags) {
      tags.Clear();
      string response;
      int rc = Transact(out response, "tags");
      WaitBody(tags);
      return rc;
    }

    public int Unset(string track, string pref) {
      string response;
      return Transact(out response, "unset", track, pref);
    }

    public int UnsetGlobal(string pref) {
      string response;
      return Transact(out response, "unset-global", pref);
    }

    public int Userinfo(out string value, string username, string property) {
      value = null;
      string response;
      int rc = Transact(out response, "userinfo", username, property);
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'userinfo'");
      value = bits[0];
      return rc;
    }

    public int Users(IList<string> users) {
      users.Clear();
      string response;
      int rc = Transact(out response, "users");
      WaitBody(users);
      return rc;
    }

    public int Version(out string version) {
      version = null;
      string response;
      int rc = Transact(out response, "version");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 1)
        throw new InvalidServerResponseException("wrong number of fields in response to 'version'");
      version = bits[0];
      return rc;
    }

    public int SetVolume(int left, int right) {
      string response;
      return Transact(out response, "volume", left, right);
    }

    public int GetVolume(out int left, out int right) {
      left = 0;
      right = 0;
      string response;
      int rc = Transact(out response, "volume");
      IList<string> bits = Utils.Split(response, false);
      if(bits.Count != 2)
        throw new InvalidServerResponseException("wrong number of fields in response to 'volume'");
      left = int.Parse(bits[0]);
      right = int.Parse(bits[1]);
      return rc;
    }

  }
}
