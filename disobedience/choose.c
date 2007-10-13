/*
 * This file is part of DisOrder
 * Copyright (C) 2006, 2007 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
/** @file disobedience/choose.c
 * @brief Hierarchical track selection and search
 *
 * We don't use the built-in tree widgets because they require that you know
 * the children of a node on demand, and we have to wait for the server to tell
 * us.
 */

#include "disobedience.h"

/* Choose track ------------------------------------------------------------ */

WT(label);
WT(event_box);
WT(menu);
WT(menu_item);
WT(layout);
WT(vbox);
WT(arrow);
WT(hbox);
WT(button);
WT(image);
WT(entry);

/* Types */

struct choosenode;

/** @brief Accumulated information about the tree widget */
struct displaydata {
  /** @brief Maximum width required */
  guint width;
  /** @brief Maximum height required */
  guint height;
};

/* instantiate the node vector type */

VECTOR_TYPE(nodevector, struct choosenode *, xrealloc);

/** @brief Signature of function called when a choosenode is filled */
typedef void (when_filled_callback)(struct choosenode *cn,
                                    void *wfu);

/** @brief One node in the virtual filesystem */
struct choosenode {
  struct choosenode *parent;            /**< @brief parent node */
  const char *path;                     /**< @brief full path or 0  */
  const char *sort;                     /**< @brief sort key */
  const char *display;                  /**< @brief display name */
  int pending;                          /**< @brief pending resolve queries */
  unsigned flags;
#define CN_EXPANDABLE 0x0001            /**< @brief node is expandable */
#define CN_EXPANDED 0x0002              /**< @brief node is expanded
                                         *
                                         * Expandable items are directories;
                                         * non-expandable ones are files. */
#define CN_DISPLAYED 0x0004             /**< @brief widget is displayed in layout */
#define CN_SELECTED 0x0008              /**< @brief node is selected */
#define CN_GETTING_FILES 0x0010         /**< @brief files inbound */
#define CN_RESOLVING_FILES 0x0020       /**< @brief resolved files inbound */
#define CN_GETTING_DIRS 0x0040          /**< @brief directories inbound */
#define CN_GETTING_ANY 0x0070           /**< @brief getting something */
#define CN_CONTINGENT 0x0080            /**< @brief expansion contingent on search */
  struct nodevector children;           /**< @brief vector of children */
  void (*fill)(struct choosenode *);    /**< @brief request child fill or 0 for leaf */
  GtkWidget *container;                 /**< @brief the container for this row */
  GtkWidget *hbox;                      /**< @brief the hbox for this row */
  GtkWidget *arrow;                     /**< @brief arrow widget or 0 */
  GtkWidget *label;                     /**< @brief text label for this node */
  GtkWidget *marker;                    /**< @brief queued marker */

  when_filled_callback *whenfilled;     /**< @brief called when filled or 0 */
  void *wfu;                            /**< @brief passed to @c whenfilled */
  int ymin;                             /**< @brief least Y value */
  int ymax;                             /**< @brief greatest Y value */
};

/** @brief One item in the popup menu */
struct choose_menuitem {
  /* Parameters */
  const char *name;                     /**< @brief name */

  /* Callbacks */
  void (*activate)(GtkMenuItem *menuitem, gpointer user_data);
  /**< @brief Called to activate the menu item.
   *
   * @p user_data is the choosenode the mouse pointer is over. */

  gboolean (*sensitive)(struct choosenode *cn);
  /* @brief Called to determine whether the menu item should be sensitive.
   *
   * TODO? */

  /* State */
  gulong handlerid;                     /**< @brief signal handler ID */
  GtkWidget *w;                         /**< @brief menu item widget */
};

/* Variables */

static GtkWidget *chooselayout;
static GtkAdjustment *vadjust;
static GtkWidget *searchentry;          /**< @brief search terms */
static GtkWidget *nextsearch;           /**< @brief next search result */
static GtkWidget *prevsearch;           /**< @brief previous search result */
static struct choosenode *root;
static GtkWidget *track_menu;           /**< @brief track popup menu */
static GtkWidget *dir_menu;             /**< @brief directory popup menu */
static struct choosenode *last_click;   /**< @brief last clicked node for selection */
static int files_visible;               /**< @brief total files visible */
static int files_selected;              /**< @brief total files selected */
static int gets_in_flight;              /**< @brief total gets in flight */
static int search_in_flight;            /**< @brief a search is underway */
static int search_obsolete;             /**< @brief the current search is void */
static char **searchresults;            /**< @brief search results */
static int nsearchresults;              /**< @brief number of results */
static int nsearchvisible;      /**< @brief number of search results visible */
static struct hash *searchhash;         /**< @brief hash of search results */
static struct progress_window *spw;     /**< @brief progress window */
static struct choosenode **searchnodes; /**< @brief choosenodes of search results */

/* Forward Declarations */

static void clear_children(struct choosenode *cn);
static struct choosenode *newnode(struct choosenode *parent,
                                  const char *path,
                                  const char *display,
                                  const char *sort,
                                  unsigned flags,
                                  void (*fill)(struct choosenode *));
static void fill_root_node(struct choosenode *cn);
static void fill_directory_node(struct choosenode *cn);
static void got_files(void *v, int nvec, char **vec);
static void got_resolved_file(void *v, const char *track);
static void got_dirs(void *v, int nvec, char **vec);

static void expand_node(struct choosenode *cn, int contingent);
static void contract_node(struct choosenode *cn);
static void updated_node(struct choosenode *cn, int redisplay);

static void display_selection(struct choosenode *cn);
static void clear_selection(struct choosenode *cn);

static void redisplay_tree(void);
static struct displaydata display_tree(struct choosenode *cn, int x, int y);
static void undisplay_tree(struct choosenode *cn);
static void initiate_search(void);
static void delete_widgets(struct choosenode *cn);
static void expand_from(struct choosenode *cn);
static struct choosenode *first_search_result(struct choosenode *cn);

static void clicked_choosenode(GtkWidget attribute((unused)) *widget,
                               GdkEventButton *event,
                               gpointer user_data);

static void activate_track_play(GtkMenuItem *menuitem, gpointer user_data);
static void activate_track_properties(GtkMenuItem *menuitem, gpointer user_data);

static gboolean sensitive_track_play(struct choosenode *cn);
static gboolean sensitive_track_properties(struct choosenode *cn);

static void activate_dir_play(GtkMenuItem *menuitem, gpointer user_data);
static void activate_dir_properties(GtkMenuItem *menuitem, gpointer user_data);
static void activate_dir_select(GtkMenuItem *menuitem, gpointer user_data);

static gboolean sensitive_dir_play(struct choosenode *cn);
static gboolean sensitive_dir_properties(struct choosenode *cn);
static gboolean sensitive_dir_select(struct choosenode *cn);

/** @brief Track menu items */
static struct choose_menuitem track_menuitems[] = {
  { "Play track", activate_track_play, sensitive_track_play, 0, 0 },
  { "Track properties", activate_track_properties, sensitive_track_properties, 0, 0 },
  { 0, 0, 0, 0, 0 }
};

/** @brief Directory menu items */
static struct choose_menuitem dir_menuitems[] = {
  { "Play all tracks", activate_dir_play, sensitive_dir_play, 0, 0 },
  { "Track properties", activate_dir_properties, sensitive_dir_properties, 0, 0 },
  { "Select all tracks", activate_dir_select, sensitive_dir_select, 0, 0 },
  { 0, 0, 0, 0, 0 }
};

/* Maintaining the data structure ------------------------------------------ */

static char *cnflags(const struct choosenode *cn) {
  unsigned f = cn->flags, n;
  struct dynstr d[1];
  
  static const char *bits[] = {
    "expandable",
    "expanded",
    "displayed",
    "selected",
    "getting_files",
    "resolving_files",
    "getting_dirs",
    "contingent"
  };
#define NBITS (sizeof bits / sizeof *bits)

  dynstr_init(d);
  if(!f)
    dynstr_append(d, '0');
  else {
    for(n = 0; n < NBITS; ++n) {
      const unsigned bit = 1 << n;
      if(f & bit) {
        if(d->nvec)
          dynstr_append(d, '|');
        dynstr_append_string(d, bits[n]);
        f ^= bit;
      }
    }
    if(f) {
      char buf[32];
      if(d->nvec)
        dynstr_append(d, '|');
      sprintf(buf, "%#x", f);
      dynstr_append_string(d, buf);
    }
  }
  dynstr_terminate(d);
  return d->vec;
}

/** @brief Create a new node */
static struct choosenode *newnode(struct choosenode *parent,
                                  const char *path,
                                  const char *display,
                                  const char *sort,
                                  unsigned flags,
                                  void (*fill)(struct choosenode *)) {
  struct choosenode *const n = xmalloc(sizeof *n);

  D(("newnode %s %s", path, display));
  if(flags & CN_EXPANDABLE)
    assert(fill);
  else
    assert(!fill);
  n->parent = parent;
  n->path = path;
  n->display = display;
  n->sort = sort;
  n->flags = flags;
  nodevector_init(&n->children);
  n->fill = fill;
  if(parent)
    nodevector_append(&parent->children, n);
  return n;
}

/** @brief Called when a node has been filled
 *
 * Response for calling @c whenfilled.
 */
static void filled(struct choosenode *cn) {
  when_filled_callback *const whenfilled = cn->whenfilled;

  if(whenfilled) {
    cn->whenfilled = 0;
    whenfilled(cn, cn->wfu);
  }
  if(nsearchvisible < nsearchresults) {
    /* There is still search expansion work to do */
    D(("filled %s %d/%d", cn->path, nsearchvisible, nsearchresults));
    expand_from(cn);
  }
  if(gets_in_flight == 0 && nsearchvisible < nsearchresults)
    expand_from(root);
}

/** @brief Fill the root */
static void fill_root_node(struct choosenode *cn) {
  struct callbackdata *cbd;

  D(("fill_root_node"));
  clear_children(cn);
  /* More de-duping possible here */
  if(cn->flags & CN_GETTING_ANY)
    return;
  gtk_label_set_text(GTK_LABEL(report_label), "getting files");
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_dirs(client, got_dirs, "", 0, cbd);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_files(client, got_files, "", 0, cbd);
  cn->flags |= CN_GETTING_FILES|CN_GETTING_DIRS;
  gets_in_flight += 2;
}

/** @brief Delete all the widgets owned by @p cn */
static void delete_cn_widgets(struct choosenode *cn) {
  if(cn->arrow) {
    DW(arrow);
    gtk_widget_destroy(cn->arrow);
    cn->arrow = 0;
  }
  if(cn->label) {
    DW(label);
    gtk_widget_destroy(cn->label);
    cn->label = 0;
  }
  if(cn->marker) {
    DW(image);
    gtk_widget_destroy(cn->marker);
    cn->marker = 0;
  }
  if(cn->hbox) {
    DW(hbox);
    gtk_widget_destroy(cn->hbox);
    cn->hbox = 0;
  }
  if(cn->container) {
    DW(event_box);
    gtk_widget_destroy(cn->container);
    cn->container = 0;
  }
}

/** @brief Recursively clear all the children of @p cn
 *
 * All the widgets at or below @p cn are deleted.  All choosenodes below
 * it are emptied. i.e. we prune the tree at @p cn.
 */
static void clear_children(struct choosenode *cn) {
  int n;

  D(("clear_children %s", cn->path));
  /* Recursively clear subtrees */
  for(n = 0; n < cn->children.nvec; ++n) {
    clear_children(cn->children.vec[n]);
    delete_cn_widgets(cn->children.vec[n]);
  }
  cn->children.nvec = 0;
}

/** @brief Called with a list of files just below some node */
static void got_files(void *v, int nvec, char **vec) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode;
  int n;

  D(("got_files %d files for %s %s", nvec, cn->path, cnflags(cn)));
  /* Complicated by the need to resolve aliases.  We can save a bit of effort
   * by re-using cbd though. */
  cn->flags &= ~CN_GETTING_FILES;
  --gets_in_flight;
  if((cn->pending = nvec)) {
    cn->flags |= CN_RESOLVING_FILES;
    for(n = 0; n < nvec; ++n) {
      disorder_eclient_resolve(client, got_resolved_file, vec[n], cbd);
      ++gets_in_flight;
    }
  }
  /* If there are no files and the directories are all read by now, we're
   * done */
  if(!(cn->flags & CN_GETTING_ANY))
    filled(cn);
}

/** @brief Called with an alias resolved filename */
static void got_resolved_file(void *v, const char *track) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode, *file_cn;

  D(("resolved %s %s %d left", cn->path, cnflags(cn), cn->pending - 1));
  /* TODO as below */
  file_cn = newnode(cn, track,
                    trackname_transform("track", track, "display"),
                    trackname_transform("track", track, "sort"),
                    0/*flags*/, 0/*fill*/);
  --gets_in_flight;
  /* Only bother updating when we've got the lot */
  if(--cn->pending == 0) {
    cn->flags &= ~CN_RESOLVING_FILES;
    updated_node(cn, 1);
    if(!(cn->flags & CN_GETTING_ANY))
      filled(cn);
  }
}

/** @brief Called with a list of directories just below some node */
static void got_dirs(void *v, int nvec, char **vec) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode;
  int n;

  D(("got_dirs %d dirs for %s %s", nvec, cn->path, cnflags(cn)));
  /* TODO this depends on local configuration for trackname_transform().
   * This will work, since the defaults are now built-in, but it'll be
   * (potentially) different to the server's configured settings.
   *
   * Really we want a variant of files/dirs that produces both the
   * raw filename and the transformed name for a chosen context.
   */
  --gets_in_flight;
  for(n = 0; n < nvec; ++n)
    newnode(cn, vec[n],
            trackname_transform("dir", vec[n], "display"),
            trackname_transform("dir", vec[n], "sort"),
            CN_EXPANDABLE, fill_directory_node);
  updated_node(cn, 1);
  cn->flags &= ~CN_GETTING_DIRS;
  if(!(cn->flags & CN_GETTING_ANY))
    filled(cn);
}
  
/** @brief Fill a child node */
static void fill_directory_node(struct choosenode *cn) {
  struct callbackdata *cbd;

  D(("fill_directory_node %s", cn->path));
  /* TODO: caching */
  if(cn->flags & CN_GETTING_ANY)
    return;
  assert(report_label != 0);
  gtk_label_set_text(GTK_LABEL(report_label), "getting files");
  clear_children(cn);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_dirs(client, got_dirs, cn->path, 0, cbd);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_files(client, got_files, cn->path, 0, cbd);
  cn->flags |= CN_GETTING_FILES|CN_GETTING_DIRS;
  gets_in_flight += 2;
}

/** @brief Expand a node */
static void expand_node(struct choosenode *cn, int contingent) {
  D(("expand_node %s %d %s", cn->path, contingent, cnflags(cn)));
  assert(cn->flags & CN_EXPANDABLE);
  /* If node is already expanded do nothing. */
  if(cn->flags & CN_EXPANDED) return;
  /* We mark the node as expanded and request that it fill itself.  When it has
   * completed it will called updated_node() and we can redraw at that
   * point. */
  cn->flags |= CN_EXPANDED;
  if(contingent)
    cn->flags |= CN_CONTINGENT;
  else
    cn->flags &= ~CN_CONTINGENT;
  /* If this node is not contingently expanded, mark all its parents back to
   * the root as not contingent either, so they won't be contracted when the
   * search results change */
  if(!contingent) {
    struct choosenode *cnp;

    for(cnp = cn->parent; cnp; cnp = cnp->parent)
      cnp->flags &= ~CN_CONTINGENT;
  }
  /* TODO: visual feedback */
  cn->fill(cn);
}

/** @brief Make sure all the search results below @p cn are expanded
 * @param cn Node to start at
 */
static void expand_from(struct choosenode *cn) {
  int n;

  if(nsearchvisible == nsearchresults)
    /* We're done */
    return;
  /* Are any of the search tracks at/below this point? */
  if(!(cn == root || hash_find(searchhash, cn->path)))
    return;
  D(("expand_from %d/%d visible %s", 
     nsearchvisible, nsearchresults, cn->path));
  if(cn->flags & CN_EXPANDABLE) {
    if(cn->flags & CN_EXPANDED)
      /* This node is marked as expanded already.  children.nvec might be 0,
       * indicating that expansion is still underway.  We should get another
       * callback when it is expanded. */
      for(n = 0; n < cn->children.nvec && gets_in_flight < 10; ++n)
        expand_from(cn->children.vec[n]);
    else {
      /* This node is not expanded yet */
      expand_node(cn, 1);
    }
  } else {
    /* This is an actual search result */
    ++nsearchvisible;
    progress_window_progress(spw, nsearchvisible, nsearchresults);
    if(nsearchvisible == nsearchresults) {
      /* We've got the lot.  We make sure the first result is visible. */
      cn = first_search_result(root);
      gtk_adjustment_clamp_page(vadjust, cn->ymin, cn->ymax);
    }
  }
}

/** @brief Contract all contingently expanded nodes below @p cn */
static void contract_contingent(struct choosenode *cn) {
  int n;

  if(cn->flags & CN_CONTINGENT)
    contract_node(cn);
  else
    for(n = 0; n < cn->children.nvec; ++n)
      contract_contingent(cn->children.vec[n]);
}

/** @brief Contract a node */
static void contract_node(struct choosenode *cn) {
  D(("contract_node %s", cn->path));
  assert(cn->flags & CN_EXPANDABLE);
  /* If node is already contracted do nothing. */
  if(!(cn->flags & CN_EXPANDED)) return;
  cn->flags &= ~(CN_EXPANDED|CN_CONTINGENT);
  /* Clear selection below this node */
  clear_selection(cn);
  /* Zot children.  We never used to do this but the result would be that over
   * time you'd end up with the entire tree pulled into memory.  If the server
   * is over a slow network it will make interactivity slightly worse; if
   * anyone complains we can make it an option. */
  clear_children(cn);
  /* We can contract a node immediately. */
  redisplay_tree();
}

/** @brief qsort() callback for ordering choosenodes */
static int compare_choosenode(const void *av, const void *bv) {
  const struct choosenode *const *aa = av, *const *bb = bv;
  const struct choosenode *a = *aa, *b = *bb;

  return compare_tracks(a->sort, b->sort,
			a->display, b->display,
			a->path, b->path);
}

/** @brief Called when an expandable node is updated.   */
static void updated_node(struct choosenode *cn, int redisplay) {
  D(("updated_node %s", cn->path));
  assert(cn->flags & CN_EXPANDABLE);
  /* It might be that the node has been de-expanded since we requested the
   * update.  In that case we ignore this notification. */
  if(!(cn->flags & CN_EXPANDED)) return;
  /* Sort children */
  qsort(cn->children.vec, cn->children.nvec, sizeof (struct choosenode *),
        compare_choosenode);
  if(redisplay)
    redisplay_tree();
}

/* Searching --------------------------------------------------------------- */

/** @brief Return true if @p track is a search result
 *
 * In particular the return value is one more than the index of the track
 * @p searchresults.
 */
static int is_search_result(const char *track) {
  void *r;

  if(searchhash && (r = hash_find(searchhash, track)))
    return 1 + *(int *)r;
  else
    return 0;
}

/** @brief Return the first search result at or below @p cn */
static struct choosenode *first_search_result(struct choosenode *cn) {
  int n;
  struct choosenode *r;

  if(cn->flags & CN_EXPANDABLE) {
    for(n = 0; n < cn->children.nvec; ++n)
      if((r = first_search_result(cn->children.vec[n])))
        return r;
  } else if(is_search_result(cn->path))
    return cn;
  return 0;
}

/** @brief Called with a list of search results
 *
 * This is called from eclient with a (possibly empty) list of search results,
 * and also from initiate_seatch with an always empty list to indicate that
 * we're not searching for anything in particular. */
static void search_completed(void attribute((unused)) *v,
                             int nvec, char **vec) {
  int n;
  char *s;

  search_in_flight = 0;
  /* Contract any choosenodes that were only expanded to show search
   * results */
  contract_contingent(root);
  if(search_obsolete) {
    /* This search has been obsoleted by user input since it started.
     * Therefore we throw away the result and search again. */
    search_obsolete = 0;
    initiate_search();
  } else {
    /* Stash the search results */
    searchresults = vec;
    nsearchresults = nvec;
    if(nvec) {
      /* Create a new search hash for fast identification of results */
      searchhash = hash_new(sizeof(int));
      for(n = 0; n < nvec; ++n) {
        int *const ip = xmalloc(sizeof (int *));
        static const int minus_1 = -1;
        *ip = n;
        /* The filename itself lives in the hash */
        hash_add(searchhash, vec[n], ip, HASH_INSERT_OR_REPLACE);
        /* So do its ancestor directories */
        for(s = vec[n] + 1; *s; ++s) {
          if(*s == '/') {
            *s = 0;
            hash_add(searchhash, vec[n], &minus_1, HASH_INSERT_OR_REPLACE);
            *s = '/';
          }
        }
      }
      /* We don't yet know that the results are visible */
      nsearchvisible = 0;
      if(spw) {
        progress_window_progress(spw, 0, 0);
        spw = 0;
      }
      if(nsearchresults > 50)
        spw = progress_window_new("Fetching search results");
      /* Initiate expansion */
      expand_from(root);
      /* The search results buttons are usable */
      gtk_widget_set_sensitive(nextsearch, 1);
      gtk_widget_set_sensitive(prevsearch, 1);
    } else {
      searchhash = 0;                   /* for the gc */
      redisplay_tree();                 /* remove search markers */
      /* The search results buttons are not usable */
      gtk_widget_set_sensitive(nextsearch, 0);
      gtk_widget_set_sensitive(prevsearch, 0);
    }
  }
}

/** @brief Initiate a search 
 *
 * If a search is underway we set @ref search_obsolete and restart the search
 * in search_completed() above.
 */
static void initiate_search(void) {
  char *terms, *e;

  /* Find out what the user is after */
  terms = xstrdup(gtk_entry_get_text(GTK_ENTRY(searchentry)));
  /* Strip leading and trailing space */
  while(*terms == ' ') ++terms;
  e = terms + strlen(terms);
  while(e > terms && e[-1] == ' ') --e;
  *e = 0;
  /* If a search is already underway then mark it as obsolete.  We'll revisit
   * when it returns. */
  if(search_in_flight) {
    search_obsolete = 1;
    return;
  }
  if(*terms) {
    /* There's still something left.  Initiate the search. */
    if(disorder_eclient_search(client, search_completed, terms, 0)) {
      /* The search terms are bad!  We treat this as if there were no search
       * terms at all.  Some kind of feedback would be handy. */
      fprintf(stderr, "bad terms [%s]\n", terms); /* TODO */
      search_completed(0, 0, 0);
    } else {
      search_in_flight = 1;
    }
  } else {
    /* No search terms - we want to see all tracks */
    search_completed(0, 0, 0);
  }
}

/** @brief Called when the cancel search button is clicked */
static void clearsearch_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  gtk_entry_set_text(GTK_ENTRY(searchentry), "");
}

/** @brief Called when the 'next search result' button is clicked */
static void next_clicked(GtkButton attribute((unused)) *button,
                         gpointer attribute((unused)) userdata) {
  /* We want to find the highest (lowest ymax) track that is below the current
   * visible range */
  int n;
  const gdouble bottom = gtk_adjustment_get_value(vadjust) + vadjust->page_size;
  const struct choosenode *candidate = 0;

  for(n = 0; n < nsearchresults; ++n) {
    const struct choosenode *const cn = searchnodes[n];

    if(cn
       && cn->ymax > bottom
       && (candidate == 0
           || cn->ymax < candidate->ymax))
      candidate = cn;
  }
  if(candidate)
    gtk_adjustment_clamp_page(vadjust, candidate->ymin, candidate->ymax);
}

/** @brief Called when the 'previous search result' button is clicked */
static void prev_clicked(GtkButton attribute((unused)) *button,
                         gpointer attribute((unused)) userdata) {
  /* We want to find the lowest (greated ymax) track that is above the current
   * visible range */
  int n;
  const gdouble top = gtk_adjustment_get_value(vadjust);
  const struct choosenode *candidate = 0;

  for(n = 0; n < nsearchresults; ++n) {
    const struct choosenode *const cn = searchnodes[n];

    if(cn
       && cn->ymin  < top
       && (candidate == 0
           || cn->ymax > candidate->ymax))
      candidate = cn;
  }
  if(candidate)
    gtk_adjustment_clamp_page(vadjust, candidate->ymin, candidate->ymax);
}

/* Display functions ------------------------------------------------------- */

/** @brief Delete all the widgets in the tree */
static void delete_widgets(struct choosenode *cn) {
  int n;

  delete_cn_widgets(cn);
  for(n = 0; n < cn->children.nvec; ++n)
    delete_widgets(cn->children.vec[n]);
  cn->flags &= ~(CN_DISPLAYED|CN_SELECTED);
  files_selected = 0;
}

/** @brief Update the display */
static void redisplay_tree(void) {
  struct displaydata d;
  guint oldwidth, oldheight;

  D(("redisplay_tree"));
  /* We'll count these up empirically each time */
  files_selected = 0;
  files_visible = 0;
  /* Correct the layout and find out how much space it uses */
  MTAG_PUSH("display_tree");
  searchnodes = nsearchresults ? xcalloc(nsearchresults, 
                                         sizeof (struct choosenode *)) : 0;
  d = display_tree(root, 0, 0);
  MTAG_POP();
  /* We must set the total size or scrolling will not work (it wouldn't be hard
   * for GtkLayout to figure it out for itself but presumably you're supposed
   * to be able to have widgets off the edge of the layuot.)
   *
   * There is a problem: if we shrink the size then the part of the screen that
   * is outside the new size but inside the old one is not updated.  I think
   * this is arguably bug in GTK+ but it's easy to force a redraw if this
   * region is nonempty.
   */
  gtk_layout_get_size(GTK_LAYOUT(chooselayout), &oldwidth, &oldheight);
  if(oldwidth > d.width || oldheight > d.height)
    gtk_widget_queue_draw(chooselayout);
  gtk_layout_set_size(GTK_LAYOUT(chooselayout), d.width, d.height);
  /* Notify the main menu of any recent changes */
  menu_update(-1);
}

/** @brief Recursive step for redisplay_tree()
 * @param cn Node to display
 * @param x X coordinate for @p cn
 * @param y Y coordinate for @p cn
 *
 * Makes sure all displayed widgets from CN down exist and are in their proper
 * place and return the maximum space used.
 */
static struct displaydata display_tree(struct choosenode *cn, int x, int y) {
  int n, aw;
  GtkRequisition req;
  struct displaydata d, cd;
  GdkPixbuf *pb;
  const char *name;
  const int search_result = is_search_result(cn->path);
  
  D(("display_tree %s %d,%d", cn->path, x, y));

  /* An expandable item contains an arrow and a text label.  When you press the
   * button it flips its expand state.
   *
   * A non-expandable item has just a text label and no arrow.
   */
  if(!cn->container) {
    MTAG_PUSH("make_widgets_1");
    /* Widgets need to be created */
    NW(hbox);
    cn->hbox = gtk_hbox_new(FALSE, 1);
    if(cn->flags & CN_EXPANDABLE) {
      NW(arrow);
      cn->arrow = gtk_arrow_new(cn->flags & CN_EXPANDED ? GTK_ARROW_DOWN
                                                        : GTK_ARROW_RIGHT,
                                GTK_SHADOW_NONE);
      cn->marker = 0;
    } else {
      cn->arrow = 0;
      if((pb = find_image("notes.png"))) {
        NW(image);
        cn->marker = gtk_image_new_from_pixbuf(pb);
      }
    }
    MTAG_POP();
    MTAG_PUSH("make_widgets_2");
    NW(label);
    cn->label = gtk_label_new(cn->display);
    if(cn->arrow)
      gtk_container_add(GTK_CONTAINER(cn->hbox), cn->arrow);
    gtk_container_add(GTK_CONTAINER(cn->hbox), cn->label);
    if(cn->marker)
      gtk_container_add(GTK_CONTAINER(cn->hbox), cn->marker);
    MTAG_POP();
    MTAG_PUSH("make_widgets_3");
    NW(event_box);
    cn->container = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(cn->container), cn->hbox);
    g_signal_connect(cn->container, "button-release-event", 
                     G_CALLBACK(clicked_choosenode), cn);
    g_signal_connect(cn->container, "button-press-event", 
                     G_CALLBACK(clicked_choosenode), cn);
    g_object_ref(cn->container);
    /* Show everything by default */
    gtk_widget_show_all(cn->container);
    MTAG_POP();
  }
  assert(cn->container);
  /* Make sure the widget name is right */
  name = (cn->flags & CN_EXPANDABLE
          ? "choose-dir"
          : search_result ? "choose-search" : "choose");
  gtk_widget_set_name(cn->label, name);
  gtk_widget_set_name(cn->container, name);
  /* Make sure the icon is right */
  if(cn->flags & CN_EXPANDABLE)
    gtk_arrow_set(GTK_ARROW(cn->arrow),
                  cn->flags & CN_EXPANDED ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  GTK_SHADOW_NONE);
  else if(cn->marker)
    /* Make sure the queued marker is right */
    /* TODO: doesn't always work */
    (queued(cn->path) ? gtk_widget_show : gtk_widget_hide)(cn->marker);
  /* Put the widget in the right place */
  if(cn->flags & CN_DISPLAYED)
    gtk_layout_move(GTK_LAYOUT(chooselayout), cn->container, x, y);
  else {
    gtk_layout_put(GTK_LAYOUT(chooselayout), cn->container, x, y);
    cn->flags |= CN_DISPLAYED;
    /* Now chooselayout has a ref to the container */
    g_object_unref(cn->container);
  }
  /* Set the widget's selection status */
  if(!(cn->flags & CN_EXPANDABLE))
    display_selection(cn);
  /* Find the size used so we can get vertical positioning right. */
  gtk_widget_size_request(cn->container, &req);
  d.width = x + req.width;
  d.height = y + req.height;
  cn->ymin = y;
  cn->ymax = d.height;
  if(cn->flags & CN_EXPANDED) {
    /* We'll offset children by the size of the arrow whatever it might be. */
    assert(cn->arrow);
    gtk_widget_size_request(cn->arrow, &req);
    aw = req.width;
    for(n = 0; n < cn->children.nvec; ++n) {
      cd = display_tree(cn->children.vec[n], x + aw, d.height);
      if(cd.width > d.width)
        d.width = cd.width;
      d.height = cd.height;
    }
  } else {
    for(n = 0; n < cn->children.nvec; ++n)
      undisplay_tree(cn->children.vec[n]);
  }
  if(!(cn->flags & CN_EXPANDABLE)) {
    ++files_visible;
    if(cn->flags & CN_SELECTED)
      ++files_selected;
  }
  /* update the search results array */
  if(search_result)
    searchnodes[search_result - 1] = cn;
  /* report back how much space we used */
  D(("display_tree %s %d,%d total size %dx%d", cn->path, x, y,
     d.width, d.height));
  return d;
}

/** @brief Remove widgets for newly hidden nodes */
static void undisplay_tree(struct choosenode *cn) {
  int n;

  D(("undisplay_tree %s", cn->path));
  /* Remove this widget from the display */
  if(cn->flags & CN_DISPLAYED) {
    gtk_container_remove(GTK_CONTAINER(chooselayout), cn->container);
    cn->flags ^= CN_DISPLAYED;
  }
  /* Remove children too */
  for(n = 0; n < cn->children.nvec; ++n)
    undisplay_tree(cn->children.vec[n]);
}

/* Selection --------------------------------------------------------------- */

/** @brief Mark the widget @p cn according to its selection state */
static void display_selection(struct choosenode *cn) {
  /* Need foreground and background colors */
  gtk_widget_set_state(cn->label, (cn->flags & CN_SELECTED
                                   ? GTK_STATE_SELECTED : GTK_STATE_NORMAL));
  gtk_widget_set_state(cn->container, (cn->flags & CN_SELECTED
                                       ? GTK_STATE_SELECTED : GTK_STATE_NORMAL));
}

/** @brief Set the selection state of a widget
 *
 * Directories can never be selected, we just ignore attempts to do so. */
static void set_selection(struct choosenode *cn, int selected) {
  unsigned f = selected ? CN_SELECTED : 0;

  D(("set_selection %d %s", selected, cn->path));
  if(!(cn->flags & CN_EXPANDABLE) && (cn->flags & CN_SELECTED) != f) {
    cn->flags ^= CN_SELECTED;
    /* Maintain selection count */
    if(selected)
      ++files_selected;
    else
      --files_selected;
    display_selection(cn);
    /* Update main menu sensitivity */
    menu_update(-1);
  }
}

/** @brief Recursively clear all selection bits from CN down */
static void clear_selection(struct choosenode *cn) {
  int n;

  set_selection(cn, 0);
  for(n = 0; n < cn->children.nvec; ++n)
    clear_selection(cn->children.vec[n]);
}

/* User actions ------------------------------------------------------------ */

/** @brief Clicked on something
 *
 * This implements playing, all the modifiers for selection, etc.
 */
static void clicked_choosenode(GtkWidget attribute((unused)) *widget,
                               GdkEventButton *event,
                               gpointer user_data) {
  struct choosenode *cn = user_data;
  int ind, last_ind, n;

  D(("clicked_choosenode %s", cn->path));
  if(event->type == GDK_BUTTON_RELEASE
     && event->button == 1) {
    /* Left click */
    if(cn->flags & CN_EXPANDABLE) {
      /* This is a directory.  Flip its expansion status. */
      if(cn->flags & CN_EXPANDED)
        contract_node(cn);
      else
        expand_node(cn, 0/*!contingent*/);
      last_click = 0;
    } else {
      /* This is a file.  Adjust selection status */
      /* TODO the basic logic here is essentially the same as that in queue.c.
       * Can we share code at all? */
      switch(event->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) {
      case 0:
        clear_selection(root);
        set_selection(cn, 1);
        last_click = cn;
        break;
      case GDK_CONTROL_MASK:
        set_selection(cn, !(cn->flags & CN_SELECTED));
        last_click = cn;
        break;
      case GDK_SHIFT_MASK:
      case GDK_SHIFT_MASK|GDK_CONTROL_MASK:
        if(last_click && last_click->parent == cn->parent) {
          /* Figure out where the current and last clicks are in the list */
          ind = last_ind = -1;
          for(n = 0; n < cn->parent->children.nvec; ++n) {
            if(cn->parent->children.vec[n] == cn)
              ind = n;
            if(cn->parent->children.vec[n] == last_click)
              last_ind = n;
          }
          /* Test shouldn't ever fail, but still */
          if(ind >= 0 && last_ind >= 0) {
            if(!(event->state & GDK_CONTROL_MASK)) {
              for(n = 0; n < cn->parent->children.nvec; ++n)
                set_selection(cn->parent->children.vec[n], 0);
            }
            if(ind > last_ind)
              for(n = last_ind; n <= ind; ++n)
                set_selection(cn->parent->children.vec[n], 1);
            else
              for(n = ind; n <= last_ind; ++n)
                set_selection(cn->parent->children.vec[n], 1);
            if(event->state & GDK_CONTROL_MASK)
              last_click = cn;
          }
        }
        /* TODO trying to select a range that doesn't share a single parent
         * currently does not work, but it ought to. */
        break;
      }
    }
  } else if(event->type == GDK_BUTTON_RELEASE
     && event->button == 2) {
    /* Middle click - play the pointed track */
    if(!(cn->flags & CN_EXPANDABLE)) {
      clear_selection(root);
      set_selection(cn, 1);
      gtk_label_set_text(GTK_LABEL(report_label), "adding track to queue");
      disorder_eclient_play(client, cn->path, 0, 0);
      last_click = 0;
    }
  } else if(event->type == GDK_BUTTON_PRESS
     && event->button == 3) {
    struct choose_menuitem *const menuitems =
      (cn->flags & CN_EXPANDABLE ? dir_menuitems : track_menuitems);
    GtkWidget *const menu =
      (cn->flags & CN_EXPANDABLE ? dir_menu : track_menu);
    /* Right click.  Pop up a menu. */
    /* If the current file isn't selected, switch the selection to just that.
     * (If we're looking at a directory then leave the selection alone.) */
    if(!(cn->flags & CN_EXPANDABLE) && !(cn->flags & CN_SELECTED)) {
      clear_selection(root);
      set_selection(cn, 1);
      last_click = cn;
    }
    /* Set the item sensitivity and callbacks */
    for(n = 0; menuitems[n].name; ++n) {
      if(menuitems[n].handlerid)
        g_signal_handler_disconnect(menuitems[n].w,
                                    menuitems[n].handlerid);
      gtk_widget_set_sensitive(menuitems[n].w,
                               menuitems[n].sensitive(cn));
      menuitems[n].handlerid = g_signal_connect
        (menuitems[n].w, "activate", G_CALLBACK(menuitems[n].activate), cn);
    }
    /* Pop up the menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), 0, 0, 0, 0,
                   event->button, event->time);
  }
}

/** @brief Called BY GTK+ to tell us the search entry box has changed */
static void searchentry_changed(GtkEditable attribute((unused)) *editable,
                                gpointer attribute((unused)) user_data) {
  initiate_search();
}

/* Track menu items -------------------------------------------------------- */

/** @brief Recursive step for gather_selected() */
static void recurse_selected(struct choosenode *cn, struct vector *v) {
  int n;

  if(cn->flags & CN_EXPANDABLE) {
    if(cn->flags & CN_EXPANDED)
      for(n = 0; n < cn->children.nvec; ++n)
        recurse_selected(cn->children.vec[n], v);
  } else {
    if((cn->flags & CN_SELECTED) && cn->path)
      vector_append(v, (char *)cn->path);
  }
}

/*** @brief Get a list of all the selected tracks */
static const char **gather_selected(int *ntracks) {
  struct vector v;

  vector_init(&v);
  recurse_selected(root, &v);
  vector_terminate(&v);
  if(ntracks) *ntracks = v.nvec;
  return (const char **)v.vec;
}

/** @brief Called when the track menu's play option is activated */
static void activate_track_play(GtkMenuItem attribute((unused)) *menuitem,
                                gpointer attribute((unused)) user_data) {
  const char **tracks = gather_selected(0);
  int n;
  
  gtk_label_set_text(GTK_LABEL(report_label), "adding track to queue");
  for(n = 0; tracks[n]; ++n)
    disorder_eclient_play(client, tracks[n], 0, 0);
}

/** @brief Called when the menu's properties option is activated */
static void activate_track_properties(GtkMenuItem attribute((unused)) *menuitem,
                                      gpointer attribute((unused)) user_data) {
  int ntracks;
  const char **tracks = gather_selected(&ntracks);

  properties(ntracks, tracks);
}

/** @brief Determine whether the menu's play option should be sensitive */
static gboolean sensitive_track_play(struct choosenode attribute((unused)) *cn) {
  return (!!files_selected
          && (disorder_eclient_state(client) & DISORDER_CONNECTED));
}

/** @brief Determine whether the menu's properties option should be sensitive */
static gboolean sensitive_track_properties(struct choosenode attribute((unused)) *cn) {
  return !!files_selected && (disorder_eclient_state(client) & DISORDER_CONNECTED);
}

/* Directory menu items ---------------------------------------------------- */

/** @brief Return the file children of @p cn
 *
 * The list is terminated by a null pointer.
 */
static const char **dir_files(struct choosenode *cn, int *nfiles) {
  const char **files = xcalloc(cn->children.nvec + 1, sizeof (char *));
  int n, m;

  for(n = m = 0; n < cn->children.nvec; ++n) 
    if(!(cn->children.vec[n]->flags & CN_EXPANDABLE))
      files[m++] = cn->children.vec[n]->path;
  files[m] = 0;
  if(nfiles) *nfiles = m;
  return files;
}

static void play_dir(struct choosenode *cn,
                     void attribute((unused)) *wfu) {
  int ntracks, n;
  const char **tracks = dir_files(cn, &ntracks);
  
  gtk_label_set_text(GTK_LABEL(report_label), "adding track to queue");
  for(n = 0; n < ntracks; ++n)
    disorder_eclient_play(client, tracks[n], 0, 0);
}

static void properties_dir(struct choosenode *cn,
                           void attribute((unused)) *wfu) {
  int ntracks;
  const char **tracks = dir_files(cn, &ntracks);
  
  properties(ntracks, tracks);
}

static void select_dir(struct choosenode *cn,
                       void attribute((unused)) *wfu) {
  int n;

  clear_selection(root);
  for(n = 0; n < cn->children.nvec; ++n) 
    set_selection(cn->children.vec[n], 1);
}

/** @brief Ensure @p cn is expanded and then call @p callback */
static void call_with_dir(struct choosenode *cn,
                          when_filled_callback *whenfilled,
                          void *wfu) {
  if(!(cn->flags & CN_EXPANDABLE))
    return;                             /* something went wrong */
  if(cn->flags & CN_EXPANDED)
    /* @p cn is already open */
    whenfilled(cn, wfu);
  else {
    /* @p cn is not open, arrange for the callback to go off when it is
     * opened */
    cn->whenfilled = whenfilled;
    cn->wfu = wfu;
    expand_node(cn, 0/*not contingnet upon search*/);
  }
}

/** @brief Called when the directory menu's play option is activated */
static void activate_dir_play(GtkMenuItem attribute((unused)) *menuitem,
                              gpointer user_data) {
  struct choosenode *const cn = (struct choosenode *)user_data;

  call_with_dir(cn, play_dir, 0);
}

/** @brief Called when the directory menu's properties option is activated */
static void activate_dir_properties(GtkMenuItem attribute((unused)) *menuitem,
                                    gpointer user_data) {
  struct choosenode *const cn = (struct choosenode *)user_data;

  call_with_dir(cn, properties_dir, 0);
}

/** @brief Called when the directory menu's select option is activated */
static void activate_dir_select(GtkMenuItem attribute((unused)) *menuitem,
                                gpointer user_data) {
  struct choosenode *const cn = (struct choosenode *)user_data;

  call_with_dir(cn, select_dir,  0);
}

/** @brief Determine whether the directory menu's play option should be sensitive */
static gboolean sensitive_dir_play(struct choosenode attribute((unused)) *cn) {
  return !!(disorder_eclient_state(client) & DISORDER_CONNECTED);
}

/** @brief Determine whether the directory menu's properties option should be sensitive */
static gboolean sensitive_dir_properties(struct choosenode attribute((unused)) *cn) {
  return !!(disorder_eclient_state(client) & DISORDER_CONNECTED);
}

/** @brief Determine whether the directory menu's select option should be sensitive */
static gboolean sensitive_dir_select(struct choosenode attribute((unused)) *cn) {
  return TRUE;
}



/* Main menu plumbing ------------------------------------------------------ */

/** @brief Determine whether the edit menu's properties option should be sensitive */
static int choose_properties_sensitive(GtkWidget attribute((unused)) *w) {
  return !!files_selected && (disorder_eclient_state(client) & DISORDER_CONNECTED);
}

/** @brief Determine whether the edit menu's select all option should be sensitive
 *
 * TODO not implemented,  see also choose_selectall_activate()
 */
static int choose_selectall_sensitive(GtkWidget attribute((unused)) *w) {
  return FALSE;
}

/** @brief Called when the edit menu's properties option is activated */
static void choose_properties_activate(GtkWidget attribute((unused)) *w) {
  activate_track_properties(0, 0);
}

/** @brief Called when the edit menu's select all option is activated
 *
 * TODO not implemented, see choose_selectall_sensitive() */
static void choose_selectall_activate(GtkWidget attribute((unused)) *w) {
}

/** @brief Main menu callbacks for Choose screen */
static const struct tabtype tabtype_choose = {
  choose_properties_sensitive,
  choose_selectall_sensitive,
  choose_properties_activate,
  choose_selectall_activate,
};

/* Public entry points ----------------------------------------------------- */

/** @brief Create a track choice widget */
GtkWidget *choose_widget(void) {
  int n;
  GtkWidget *scrolled;
  GtkWidget *vbox, *hbox, *clearsearch;

  /*
   *   +--vbox-------------------------------------------------------+
   *   | +-hbox----------------------------------------------------+ |
   *   | | searchentry                               | clearsearch | |
   *   | +---------------------------------------------------------+ |
   *   | +-scrolled------------------------------------------------+ |
   *   | | +-chooselayout------------------------------------++--+ | |
   *   | | | Tree structure is manually layed out in here    ||^^| | |
   *   | | |                                                 ||  | | |
   *   | | |                                                 ||  | | |
   *   | | |                                                 ||  | | |
   *   | | |                                                 ||vv| | |
   *   | | +-------------------------------------------------++--+ | |
   *   | | +-------------------------------------------------+     | |
   *   | | |<                                               >|     | |
   *   | | +-------------------------------------------------+     | |
   *   | +---------------------------------------------------------+ |
   *   +-------------------------------------------------------------+
   */
  
  /* Text entry box for search terms */
  NW(entry);
  searchentry = gtk_entry_new();
  g_signal_connect(searchentry, "changed", G_CALLBACK(searchentry_changed), 0);
  gtk_tooltips_set_tip(tips, searchentry, "Enter search terms here; search is automatic", "");

  /* Cancel button to clear the search */
  NW(button);
  clearsearch = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  g_signal_connect(G_OBJECT(clearsearch), "clicked",
                   G_CALLBACK(clearsearch_clicked), 0);
  gtk_tooltips_set_tip(tips, clearsearch, "Clear search terms", "");

  /* Up and down buttons to find previous/next results; initially they are not
   * usable as there are no search results. */
  prevsearch = iconbutton("up.png", "Previous search result");
  g_signal_connect(G_OBJECT(prevsearch), "clicked",
                   G_CALLBACK(prev_clicked), 0);
  gtk_widget_set_sensitive(prevsearch, 0);
  nextsearch = iconbutton("down.png", "Next search result");
  g_signal_connect(G_OBJECT(nextsearch), "clicked",
                   G_CALLBACK(next_clicked), 0);
  gtk_widget_set_sensitive(nextsearch, 0);

  /* hbox packs the search tools button together on a line */
  NW(hbox);
  hbox = gtk_hbox_new(FALSE/*homogeneous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(hbox), searchentry,
                     TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), prevsearch,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), nextsearch,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_start(GTK_BOX(hbox), clearsearch,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  
  /* chooselayout contains the currently visible subset of the track
   * namespace */
  NW(layout);
  chooselayout = gtk_layout_new(0, 0);
  root = newnode(0/*parent*/, "<root>", "All files", "",
                 CN_EXPANDABLE, fill_root_node);
  expand_node(root, 0);                 /* will call redisplay_tree */
  /* Create the popup menus */
  NW(menu);
  track_menu = gtk_menu_new();
  g_signal_connect(track_menu, "destroy", G_CALLBACK(gtk_widget_destroyed),
                   &track_menu);
  for(n = 0; track_menuitems[n].name; ++n) {
    NW(menu_item);
    track_menuitems[n].w = 
      gtk_menu_item_new_with_label(track_menuitems[n].name);
    gtk_menu_attach(GTK_MENU(track_menu), track_menuitems[n].w,
                    0, 1, n, n + 1);
  }
  NW(menu);
  dir_menu = gtk_menu_new();
  g_signal_connect(dir_menu, "destroy", G_CALLBACK(gtk_widget_destroyed),
                   &dir_menu);
  for(n = 0; dir_menuitems[n].name; ++n) {
    NW(menu_item);
    dir_menuitems[n].w = 
      gtk_menu_item_new_with_label(dir_menuitems[n].name);
    gtk_menu_attach(GTK_MENU(dir_menu), dir_menuitems[n].w,
                    0, 1, n, n + 1);
  }
  /* The layout is scrollable */
  scrolled = scroll_widget(chooselayout, "choose");
  vadjust = gtk_layout_get_vadjustment(GTK_LAYOUT(chooselayout));

  /* The scrollable layout and the search hbox go together in a vbox */
  NW(vbox);
  vbox = gtk_vbox_new(FALSE/*homogenous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(vbox), hbox,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_end(GTK_BOX(vbox), scrolled,
                   TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);

  g_object_set_data(G_OBJECT(vbox), "type", (void *)&tabtype_choose);
  return vbox;
}

/** @brief Called when something we care about here might have changed */
void choose_update(void) {
  redisplay_tree();
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
