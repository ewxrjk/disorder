# The Status Quo #

Track names (basically, names in the filesystem converted to Unicode) are the primary key.

Track names are decomposed into name parts: artist, album and title.  These can be overridden by clients and if they are, they are recomposed back into aliases in the virtual filesystem.

Users find tracks either by navigating through the virtual filesystem or by using tag or word search over tracks.

It’s a bit ad-hoc and creaky, and makes MetadataSupport a bit more painful than it has to be.

# An Alternative Design #

Track names remain the primary key, but are much less visible to the user.  There are no aliases any more.

Each track is considered more explicitly as a row in a database.  Columns might include:
  * artist
  * album
  * sequence
  * title
  * length
  * genre
  * tags
  * weight
  * accession date
  * owner
  * ...

These are variously computed from the name or embedded metadata, entered or overridden by the user, read from the filesystem, etc.  In each case a decision must be made about whether clients override the stored value, edit it, or have to put up with it.

The user view is then of all the rows, sorted and filtered by some specification.

Sorting is fairly easy: we take a list of columns with an ascending/descending specification, and compare according to type.

Filtering should include exact match and word search.

# Implementation #

A sqlite backend is probably a good plan.

This is a substantial upheaval!