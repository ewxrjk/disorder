<font color='red'><b>This is a work in progress.</b></font>

# Overview #

Audio tracks contain embedded metadata (artist, genre, ...).  DisOrder currently ignores everything except the track length.  To define some basic requirements:
  * Metadata should be retrieved when a track is detected
  * If a track is changed the metadata should be updated (eventually)
  * The way metadata is collected should be decoupled from the way it is used (so that a change to the latter does not force a change to the former)
  * Captured metadata should survive the storage medium being temporarily unavailable (since it is relatively expensive to compute)
  * User settings should beat metadata.

# Design #

Scanner plugins will output not just track names but a track generation too.  If the track generation changes that indicates that its metadata may have changed.  For the `fs` plugin the generation is the mtime.

The tracklength plugin will become a trackinfo plugin.  This will now return a list of key-value pairs computed from the track contents, plus a generation number that they correspond to.

All these pairs will be stored in the prefs database (and therefore will survive the track being unavailable, e.g. because the filesystem is not mounted).  Their keys must be prefixed `_embedded_` (making them unavailable to clients) with the exception of the track length which must have a key of `_length` and be the length in seconds in decimal.

`_embedded_artist`, `_embedded_album` and `_embedded_title` are given special significance by the server: they are used to compute track name parts for the “display” context (see [trackdb.c](http://www.greenend.org.uk/rjk/bzr/disorder.stable/lib/trackdb.c):getpart().)  The user-specified preferences beat the metadata, however, and as a special restriction, name parts from embedded metadata never take part in alias construction.

Format-specific tags are converted as follows:
  * [Vorbis comments](http://xiph.org/vorbis/doc/v-comment.html) have field names forced to lower case and prefixed.
  * [ID3 tags](http://en.wikipedia.org/wiki/ID3) have `artist`, `album` and `title` (plus prefixes) with the obvious interpretation, and `genre` using the free-text field if available, otherwise a looked-up ID3v1 genre.
  * FLAC TBD when someone has waded through the ridiculously verbose API
  * WAV will just produce a length unless and until someone points at a suitable specification.

# To-Do List #

  * A bit more detail on Vorbis and MP3 perhaps.
  * FLAC.
  * WAV, perhaps.
  * Automated construction of tags, e.g. from “genre” fields, perhaps.

# Rationale #

## Key Names ##

Putting a `_` at the start avoids clashes with the client namespace and, perhaps more importantly, eliminates the question of what happens if a client tries to modify these values.

`_length` isn’t `_embedded_length` because a track might really have an embedded `length` value which wasn't the right thing.

Using the prefs database is really part of removable device support ([issue 52](https://code.google.com/p/disorder/issues/detail?id=52)).

## Aliases ##

The current logic for aliases is:
  * if a track has no alias then it appears exactly once.
  * if a track has an alias _in the same directory_ then the alias is filtered out of track lists, leaving only the original name.
  * if a track has an alias in a different directory then both appear.

If we acquire metadata then in some cases the metadata will produce a name that differs
from the original in the directory name, leading to some directories appearing twice.

Furthermore the alias computed from the metadata may lack the sequencing information embedded in the original filename.  Having your album have more accurate names is small consolation if it plays out of order.

Technically these problems exist now: but in practice with small numbers of manually configured aliases, they don’t create a practical problem.

The best answer for now is probably: name parts from embedded metadata never produce aliases.  In the future perhaps a TrackDatabaseRedesign would be a good idea.