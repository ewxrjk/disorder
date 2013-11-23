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
    public void Adopt(string id) {
      Transact("adopt", id);
    }

    public void Adduser(string user, string password, string rights) {
      Transact("adduser", user, password, rights);
    }

    // Allfiles not yet implemented

    // Confirm not yet implemented

    // Cookie not yet implemented

    public void Deluser(string user) {
      Transact("deluser", user);
    }

    // Dirs not yet implemented

    public void Disable() {
      Transact("disable");
    }

    public void Edituser(string username, string property, string value) {
      Transact("edituser", username, property, value);
    }

    public void Enable() {
      Transact("enable");
    }

    // Enabled not yet implemented

    // Exists not yet implemented

    // Files not yet implemented

    public void Get(out string value, string track, string pref) {
      string response = Transact("get", track, pref);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      value = bits[0];
    }

    public void GetGlobal(out string value, string pref) {
      string response = Transact("get-global", pref);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      value = bits[0];
    }

    public void Length(out int length, string track) {
      string response = Transact("length", track);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      length = int.Parse(bits[0]);
    }

    public void MakeCookie(out string cookie) {
      string response = Transact("make-cookie");
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      cookie = bits[0];
    }

    public void Move(string track, int delta) {
      Transact("move", track, delta);
    }

    // Moveafter not yet implemented

    // NewTracks not yet implemented

    public void Nop() {
      Transact("nop");
    }

    public void Part(out string part, string track, string context, string namepart) {
      string response = Transact("part", track, context, namepart);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      part = bits[0];
    }

    public void Pause() {
      Transact("pause");
    }

    // Play not yet implemented

    // Playafter not yet implemented

    // Playing not yet implemented

    public void PlaylistDelete(string playlist) {
      Transact("playlist-delete", playlist);
    }

    // PlaylistGet not yet implemented

    // PlaylistGetShare not yet implemented

    public void PlaylistLock(string playlist) {
      Transact("playlist-lock", playlist);
    }

    // PlaylistSet not yet implemented

    public void PlaylistSetShare(string playlist, string share) {
      Transact("playlist-set-share", playlist, share);
    }

    public void PlaylistUnlock() {
      Transact("playlist-unlock");
    }

    // Playlists not yet implemented

    // Prefs not yet implemented

    // Queue not yet implemented

    public void RandomDisable() {
      Transact("random-disable");
    }

    public void RandomEnable() {
      Transact("random-enable");
    }

    // RandomEnabled not yet implemented

    // Recent not yet implemented

    public void Reconfigure() {
      Transact("reconfigure");
    }

    public void Register(out string confirmation, string username, string password, string email) {
      string response = Transact("register", username, password, email);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      confirmation = bits[0];
    }

    public void Reminder(string username) {
      Transact("reminder", username);
    }

    public void Remove(string id) {
      Transact("remove", id);
    }

    public void Rescan() {
      Transact("rescan");
    }

    public void Resolve(out string resolved, string track) {
      string response = Transact("resolve", track);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      resolved = bits[0];
    }

    public void Resume() {
      Transact("resume");
    }

    public void Revoke() {
      Transact("revoke");
    }

    public void RtpAddress(out string address, out string port) {
      string response = Transact("rtp-address");
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 2)
        throw new Exception("malformed response from server");
      address = bits[0];
      port = bits[1];
    }

    public void RtpCancel() {
      Transact("rtp-cancel");
    }

    public void RtpRequest(string address, string port) {
      Transact("rtp-request", address, port);
    }

    public void Scratch(string id) {
      Transact("scratch", id);
    }

    public void ScheduleAddPlay(DateTime when, string priority, string track) {
      Transact("schedule-add", when, priority, "play", track);
    }

    public void ScheduleAddSetGlobal(DateTime when, string priority, string pref, string value) {
      Transact("schedule-add", when, priority, "set-global", pref, value);
    }

    public void ScheduleAddUnsetGlobal(DateTime when, string priority, string pref) {
      Transact("schedule-add", when, priority, "set-global", pref);
    }

    public void ScheduleDel(string id) {
      Transact("schedule-del", id);
    }

    // ScheduleGet not yet implemented

    // ScheduleList not yet implemented

    // Search not yet implemented

    public void Set(string track, string pref, string value) {
      Transact("set", track, pref, value);
    }

    public void SetGlobal(string pref, string value) {
      Transact("set-global", pref, value);
    }

    public void Shutdown() {
      Transact("shutdown");
    }

    // Stats not yet implemented

    // Tags not yet implemented

    public void Unset(string track, string pref) {
      Transact("unset", track, pref);
    }

    public void UnsetGlobal(string pref) {
      Transact("unset-global", pref);
    }

    public void Userinfo(out string value, string username, string property) {
      string response = Transact("userinfo", username, property);
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      value = bits[0];
    }

    // Users not yet implemented

    public void Version(out string version) {
      string response = Transact("version");
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 1)
        throw new Exception("malformed response from server");
      version = bits[0];
    }

    public void SetVolume(int left, int right) {
      Transact("volume", left, right);
    }

    public void GetVolume(out int left, out int right) {
      string response = Transact("volume");
      IList<string> bits = Configuration.Split(response, false);
      if(bits.Count != 2)
        throw new Exception("malformed response from server");
      left = int.Parse(bits[0]);
      right = int.Parse(bits[1]);
    }

  }
}
