/*
 * This file is part of DisOrder
 * Copyright (C) 2006 Richard Kettlewell
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

#include "disobedience.h"

/* Choose track ------------------------------------------------------------ */

/* We don't use the built-in tree widgets because they require that you know
 * the children of a node on demand, and we have to wait for the server to tell
 * us. */

/* Types */

struct choosenode;

struct displaydata {
  guint width;                          /* total width required */
  guint height;                         /* total height required */
};

/* instantiate the node vector type */
VECTOR_TYPE(nodevector, struct choosenode *, xrealloc)

struct choosenode {
  struct choosenode *parent;            /* parent node */
  const char *path;                     /* full path or 0  */
  const char *sort;                     /* sort key */
  const char *display;                  /* display name */
  int pending;                          /* pending resolve queries */
  unsigned flags;
#define CN_EXPANDABLE 0x0001            /* node is expandable */
#define CN_EXPANDED 0x0002              /* node is expanded */
/* Expandable items are directories; non-expandable ones are files */
#define CN_DISPLAYED 0x0004             /* widget is displayed in layout */
#define CN_SELECTED 0x0008              /* node is selected */
  struct nodevector children;           /* vector of children */
  void (*fill)(struct choosenode *);    /* request child fill or 0 for leaf */
  GtkWidget *container;                 /* the container for this row */
  GtkWidget *hbox;                      /* the hbox for this row */
  GtkWidget *arrow;                     /* arrow widget or 0 */
  GtkWidget *label;                     /* text label for this node */
  GtkWidget *marker;                    /* queued marker */
};

struct menuitem {
  /* Parameters */
  const char *name;                     /* name */

  /* Callbacks */
  void (*activate)(GtkMenuItem *menuitem, gpointer user_data);
  /* Called to activate the menu item.  The user data is the choosenode the
   * pointer is over. */

  gboolean (*sensitive)(struct choosenode *cn);
  /* Called to determine whether the menu item should be sensitive.  TODO */

  /* State */
  gulong handlerid;                     /* signal handler ID */
  GtkWidget *w;                         /* menu item widget */
};

/* Variables */

static GtkWidget *chooselayout;
static GtkWidget *searchentry;          /* search terms */
static struct choosenode *root;
static struct choosenode *realroot;
static GtkWidget *menu;                 /* our popup menu */
static struct choosenode *last_click;   /* last clicked node for selection */
static int files_visible;               /* total files visible */
static int files_selected;              /* total files selected */
static int search_in_flight;            /* a search is underway */
static int search_obsolete;             /* the current search is void */
static char **searchresults;            /* search results */
static int nsearchresults;              /* number of results */

/* Forward Declarations */

static struct choosenode *newnode(struct choosenode *parent,
                                  const char *path,
                                  const char *display,
                                  const char *sort,
                                  unsigned flags,
                                  void (*fill)(struct choosenode *));
static void fill_root_node(struct choosenode *cn);
static void fill_letter_node(struct choosenode *cn);
static void fill_directory_node(struct choosenode *cn);
static void got_files(void *v, int nvec, char **vec);
static void got_resolved_file(void *v, const char *track);
static void got_dirs(void *v, int nvec, char **vec);

static void expand_node(struct choosenode *cn);
static void contract_node(struct choosenode *cn);
static void updated_node(struct choosenode *cn, int redisplay);

static void display_selection(struct choosenode *cn);
static void clear_selection(struct choosenode *cn);

static void redisplay_tree(void);
static struct displaydata display_tree(struct choosenode *cn, int x, int y);
static void undisplay_tree(struct choosenode *cn);
static void initiate_search(void);
static void delete_widgets(struct choosenode *cn);

static void clicked_choosenode(GtkWidget attribute((unused)) *widget,
                               GdkEventButton *event,
                               gpointer user_data);

static void activate_play(GtkMenuItem *menuitem, gpointer user_data);
static void activate_remove(GtkMenuItem *menuitem, gpointer user_data);
static void activate_properties(GtkMenuItem *menuitem, gpointer user_data);

static gboolean sensitive_play(struct choosenode *cn);
static gboolean sensitive_remove(struct choosenode *cn);
static gboolean sensitive_properties(struct choosenode *cn);

static struct menuitem menuitems[] = {
  { "Play", activate_play, sensitive_play, 0, 0 },
  { "Remove", activate_remove, sensitive_remove, 0, 0 },
  { "Properties", activate_properties, sensitive_properties, 0, 0 },
};

#define NMENUITEMS (int)(sizeof menuitems / sizeof *menuitems)

/* Maintaining the data structure ------------------------------------------ */

/* Create a new node */
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

/* Fill the root */
static void fill_root_node(struct choosenode *cn) {
  int ch;
  char *name;
  struct callbackdata *cbd;

  D(("fill_root_node"));
  if(choosealpha) {
    if(!cn->children.nvec) {              /* Only need to do this once */
      for(ch = 'A'; ch <= 'Z'; ++ch) {
        byte_xasprintf(&name, "%c", ch);
        newnode(cn, "<letter>", name, name, CN_EXPANDABLE, fill_letter_node);
      }
      newnode(cn, "<letter>", "*", "~", CN_EXPANDABLE, fill_letter_node);
    }
    updated_node(cn, 1);
  } else {
    /* More de-duping possible here */
    gtk_label_set_text(GTK_LABEL(report_label), "getting files");
    cbd = xmalloc(sizeof *cbd);
    cbd->u.choosenode = cn;
    disorder_eclient_dirs(client, got_dirs, "", 0, cbd);
    cbd = xmalloc(sizeof *cbd);
    cbd->u.choosenode = cn;
    disorder_eclient_files(client, got_files, "", 0, cbd);
  }
}

/* Clear all the children of CN */
static void clear_children(struct choosenode *cn) {
  int n;

  D(("clear_children %s", cn->path));
  /* Recursively clear subtrees */
  for(n = 0; n < cn->children.nvec; ++n) {
    clear_children(cn->children.vec[n]);
    if(cn->children.vec[n]->container) {
      if(cn->children.vec[n]->arrow)
        gtk_widget_destroy(cn->children.vec[n]->arrow);
      gtk_widget_destroy(cn->children.vec[n]->label);
      if(cn->children.vec[n]->marker)
        gtk_widget_destroy(cn->children.vec[n]->marker);
      gtk_widget_destroy(cn->children.vec[n]->hbox);
      gtk_widget_destroy(cn->children.vec[n]->container);
    }
  }
  cn->children.nvec = 0;
}

/* Fill a child node */
static void fill_letter_node(struct choosenode *cn) {
  const char *regexp;
  struct callbackdata *cbd;

  D(("fill_letter_node %s", cn->display));
  switch(cn->display[0]) {
  default:
    byte_xasprintf((char **)&regexp, "^(the )?%c", tolower(cn->display[0]));
    break;
  case 'T':
    regexp = "^(?!the [^t])t";
    break;
  case '*':
    regexp = "^[^a-z]";
    break;
  }
  /* TODO: caching */
  /* TODO: de-dupe against fill_directory_node */
  gtk_label_set_text(GTK_LABEL(report_label), "getting files");
  clear_children(cn);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_dirs(client, got_dirs, "", regexp, cbd);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_files(client, got_files, "", regexp, cbd);
}

/* Called with a list of files just below some node */
static void got_files(void *v, int nvec, char **vec) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode;
  int n;

  D(("got_files %d files for %s", nvec, cn->path));
  /* Complicated by the need to resolve aliases.  We can save a bit of effort
   * by re-using cbd though. */
  cn->pending = nvec;
  for(n = 0; n < nvec; ++n)
    disorder_eclient_resolve(client, got_resolved_file, vec[n], cbd);
}

static void got_resolved_file(void *v, const char *track) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode, *file_cn;

  file_cn = newnode(cn, track,
                    trackname_transform("track", track, "display"),
                    trackname_transform("track", track, "sort"),
                    0/*flags*/, 0/*fill*/);
  /* Only bother updating when we've got the lot */
  if(--cn->pending == 0)
    updated_node(cn, 1);
}

/* Called with a list of directories just below some node */
static void got_dirs(void *v, int nvec, char **vec) {
  struct callbackdata *cbd = v;
  struct choosenode *cn = cbd->u.choosenode;
  int n;

  D(("got_dirs %d dirs for %s", nvec, cn->path));
  for(n = 0; n < nvec; ++n)
    newnode(cn, vec[n],
            trackname_transform("dir", vec[n], "display"),
            trackname_transform("dir", vec[n], "sort"),
            CN_EXPANDABLE, fill_directory_node);
  updated_node(cn, 1);
}
  
/* Fill a child node */
static void fill_directory_node(struct choosenode *cn) {
  struct callbackdata *cbd;

  D(("fill_directory_node %s", cn->path));
  /* TODO: caching */
  /* TODO: de-dupe against fill_letter_node */
  assert(report_label != 0);
  gtk_label_set_text(GTK_LABEL(report_label), "getting files");
  cn->children.nvec = 0;
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_dirs(client, got_dirs, cn->path, 0, cbd);
  cbd = xmalloc(sizeof *cbd);
  cbd->u.choosenode = cn;
  disorder_eclient_files(client, got_files, cn->path, 0, cbd);
}

/* Expand a node */
static void expand_node(struct choosenode *cn) {
  D(("expand_node %s", cn->path));
  assert(cn->flags & CN_EXPANDABLE);
  /* If node is already expanded do nothing. */
  if(cn->flags & CN_EXPANDED) return;
  /* We mark the node as expanded and request that it fill itself.  When it has
   * completed it will called updated_node() and we can redraw at that
   * point. */
  cn->flags |= CN_EXPANDED;
  /* TODO: visual feedback */
  cn->fill(cn);
}

/* Contract a node */
static void contract_node(struct choosenode *cn) {
  D(("contract_node %s", cn->path));
  assert(cn->flags & CN_EXPANDABLE);
  /* If node is already contracted do nothing. */
  if(!(cn->flags & CN_EXPANDED)) return;
  cn->flags &= ~CN_EXPANDED;
  /* Clear selection below this node */
  clear_selection(cn);
  /* We can contract a node immediately. */
  redisplay_tree();
}

/* qsort callback for ordering choosenodes */
static int compare_choosenode(const void *av, const void *bv) {
  const struct choosenode *const *aa = av, *const *bb = bv;
  const struct choosenode *a = *aa, *b = *bb;

  return compare_tracks(a->sort, b->sort,
			a->display, b->display,
			a->path, b->path);
}

/* Called when an expandable node is updated.   */
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

static int compare_track_for_qsort(const void *a, const void *b) {
  return compare_path(*(char **)a, *(char **)b);
}

/* Return true iff FILE is a child of DIR */
static int is_child(const char *dir, const char *file) {
  const size_t dlen = strlen(dir);

  return (!strncmp(file, dir, dlen)
          && file[dlen] == '/'
          && strchr(file + dlen + 1, '/') == 0);
}

/* Return true iff FILE is a descendant of DIR */
static int is_descendant(const char *dir, const char *file) {
  const size_t dlen = strlen(dir);

  return !strncmp(file, dir, dlen) && file[dlen] == '/';
}

/* Called to fill a node in the search results tree */
static void fill_search_node(struct choosenode *cn) {
  int n;
  const size_t plen = strlen(cn->path);
  const char *s;
  char *dir, *last = 0;

  D(("fill_search_node %s", cn->path));
  /* We depend on the search results being sorted as by compare_path(). */
  cn->children.nvec = 0;
  for(n = 0; n < nsearchresults; ++n) {
    /* We only care about descendants of CN */
    if(!is_descendant(cn->path, searchresults[n]))
       continue;
    s = strchr(searchresults[n] + plen + 1, '/');
    if(s) {
      /* We've identified a subdirectory of CN. */
      dir = xstrndup(searchresults[n], s - searchresults[n]);
      if(!last || strcmp(dir, last)) {
        /* Not a duplicate */
        last = dir;
        newnode(cn, dir,
                trackname_transform("dir", dir, "display"),
                trackname_transform("dir", dir, "sort"),
                CN_EXPANDABLE, fill_search_node);
      }
    } else {
      /* We've identified a file in CN */
      newnode(cn, searchresults[n],
              trackname_transform("track", searchresults[n], "display"),
              trackname_transform("track", searchresults[n], "sort"),
              0/*flags*/, 0/*fill*/);
    }
  }
  updated_node(cn, 1);
}

/* This is called from eclient with a (possibly empty) list of search results,
 * and also from initiate_seatch with an always empty list to indicate that
 * we're not searching for anything in particular. */
static void search_completed(void attribute((unused)) *v,
                             int nvec, char **vec) {
  struct choosenode *cn;
  int n;
  const char *dir;

  search_in_flight = 0;
  if(search_obsolete) {
    /* This search has been obsoleted by user input since it started.
     * Therefore we throw away the result and search again. */
    search_obsolete = 0;
    initiate_search();
  } else {
    if(nvec) {
      /* We will replace the choose tree with a tree structured view of search
       * results.  First we must disabled the choose tree's widgets. */
      delete_widgets(root);
      /* Put the tracks into order, grouped by directory.  They'll probably
       * come back this way anyway in current versions of the server, but it's
       * cheap not to rely on it (compared with the massive effort we expend
       * later on) */
      qsort(vec, nvec, sizeof(char *), compare_track_for_qsort);
      searchresults = vec;
      nsearchresults = nvec;
      cn = root = newnode(0/*parent*/, "", "Search results", "",
                  CN_EXPANDABLE|CN_EXPANDED, fill_search_node);
      /* Construct the initial tree.  We do this in a single pass and expand
       * everything, so you can actually see your search results. */
      for(n = 0; n < nsearchresults; ++n) {
        /* Firstly we might need to go up a few directories to each an ancestor
         * of this track */
        while(!is_descendant(cn->path, searchresults[n])) {
          /* We report the update on each node the last time we see it (With
           * display=0, the main purpose of this is to get the order of the
           * children right.) */
          updated_node(cn, 0);
          cn = cn->parent;
        }
        /* Secondly we might need to insert some new directories */
        while(!is_child(cn->path, searchresults[n])) {
          /* Figure out the subdirectory */
          dir = xstrndup(searchresults[n],
                         strchr(searchresults[n] + strlen(cn->path) + 1,
                                '/') - searchresults[n]);
          cn = newnode(cn, dir,
                       trackname_transform("dir", dir, "display"),
                       trackname_transform("dir", dir, "sort"),
                       CN_EXPANDABLE|CN_EXPANDED, fill_search_node);
        }
        /* Finally we can insert the track as a child of the current
         * directory */
        newnode(cn, searchresults[n],
                trackname_transform("track", searchresults[n], "display"),
                trackname_transform("track", searchresults[n], "sort"),
                0/*flags*/, 0/*fill*/);
      }
      while(cn) {
        /* Update all the nodes back up to the root */
        updated_node(cn, 0);
        cn = cn->parent;
      }
      /* Now it's worth displaying the tree */
      redisplay_tree();
    } else if(root != realroot) {
      delete_widgets(root);
      root = realroot;
      redisplay_tree();
    }
  }
}

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

/* Called when the cancel search button is clicked */
static void clearsearch_clicked(GtkButton attribute((unused)) *button,
                                gpointer attribute((unused)) userdata) {
  gtk_entry_set_text(GTK_ENTRY(searchentry), "");
}

/* Display functions ------------------------------------------------------- */

/* Delete all the widgets in the tree */
static void delete_widgets(struct choosenode *cn) {
  int n;

  if(cn->container) {
    gtk_widget_destroy(cn->container);
    cn->container = 0;
  }
  for(n = 0; n < cn->children.nvec; ++n)
    delete_widgets(cn->children.vec[n]);
  cn->flags &= ~(CN_DISPLAYED|CN_SELECTED);
  files_selected = 0;
}

/* Update the display */
static void redisplay_tree(void) {
  struct displaydata d;
  guint oldwidth, oldheight;

  D(("redisplay_tree"));
  /* We'll count these up empirically each time */
  files_selected = 0;
  files_visible = 0;
  /* Correct the layout and find out how much space it uses */
  d = display_tree(root, 0, 0);
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

/* Make sure all displayed widgets from CN down exist and are in their proper
 * place and return the vertical space used. */
static struct displaydata display_tree(struct choosenode *cn, int x, int y) {
  int n, aw;
  GtkRequisition req;
  struct displaydata d, cd;
  GdkPixbuf *pb;
  
  D(("display_tree %s %d,%d", cn->path, x, y));

  /* An expandable item contains an arrow and a text label.  When you press the
   * button it flips its expand state.
   *
   * A non-expandable item has just a text label and no arrow.
   */
  if(!cn->container) {
    /* Widgets need to be created */
    cn->hbox = gtk_hbox_new(FALSE, 1);
    if(cn->flags & CN_EXPANDABLE) {
      cn->arrow = gtk_arrow_new(cn->flags & CN_EXPANDED ? GTK_ARROW_DOWN
                                                        : GTK_ARROW_RIGHT,
                                GTK_SHADOW_NONE);
      cn->marker = 0;
    } else {
      cn->arrow = 0;
      if((pb = find_image("notes.png")))
        cn->marker = gtk_image_new_from_pixbuf(pb);
    }
    cn->label = gtk_label_new(cn->display);
    if(cn->arrow)
      gtk_container_add(GTK_CONTAINER(cn->hbox), cn->arrow);
    gtk_container_add(GTK_CONTAINER(cn->hbox), cn->label);
    if(cn->marker)
      gtk_container_add(GTK_CONTAINER(cn->hbox), cn->marker);
    cn->container = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(cn->container), cn->hbox);
    g_signal_connect(cn->container, "button-release-event", 
                     G_CALLBACK(clicked_choosenode), cn);
    g_signal_connect(cn->container, "button-press-event", 
                     G_CALLBACK(clicked_choosenode), cn);
    g_object_ref(cn->container);
    gtk_widget_set_name(cn->label, "choose");
    gtk_widget_set_name(cn->container, "choose");
    /* Show everything by default */
    gtk_widget_show_all(cn->container);
  }
  assert(cn->container);
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
  }
  /* Set the widget's selection status */
  if(!(cn->flags & CN_EXPANDABLE))
    display_selection(cn);
  /* Find the size used so we can get vertical positioning right. */
  gtk_widget_size_request(cn->container, &req);
  d.width = x + req.width;
  d.height = y + req.height;
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
  /* report back how much space we used */
  D(("display_tree %s %d,%d total size %dx%d", cn->path, x, y,
     d.width, d.height));
  return d;
}

/* Remove widgets for newly hidden nodes */
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

static void display_selection(struct choosenode *cn) {
  /* Need foreground and background colors */
  gtk_widget_set_state(cn->label, (cn->flags & CN_SELECTED
                                   ? GTK_STATE_SELECTED : GTK_STATE_NORMAL));
  gtk_widget_set_state(cn->container, (cn->flags & CN_SELECTED
                                       ? GTK_STATE_SELECTED : GTK_STATE_NORMAL));
}

/* Set the selection state of a widget.  Directories can never be selected, we
 * just ignore attempts to do so. */
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

/* Recursively clear all selection bits from CN down */
static void clear_selection(struct choosenode *cn) {
  int n;

  set_selection(cn, 0);
  for(n = 0; n < cn->children.nvec; ++n)
    clear_selection(cn->children.vec[n]);
}

/* User actions ------------------------------------------------------------ */

/* Clicked on something */
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
        expand_node(cn);
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
    /* Right click.  Pop up a menu. */
    /* If the current file isn't selected, switch the selection to just that.
     * (If we're looking at a directory then leave the selection alone.) */
    if(!(cn->flags & CN_EXPANDABLE) && !(cn->flags & CN_SELECTED)) {
      clear_selection(root);
      set_selection(cn, 1);
      last_click = cn;
    }
    /* Set the item sensitivity and callbacks */
    for(n = 0; n < NMENUITEMS; ++n) {
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

static void searchentry_changed(GtkEditable attribute((unused)) *editable,
                                gpointer attribute((unused)) user_data) {
  initiate_search();
}

/* Menu items -------------------------------------------------------------- */

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

static char **gather_selected(int *ntracks) {
  struct vector v;

  vector_init(&v);
  recurse_selected(root, &v);
  vector_terminate(&v);
  if(ntracks) *ntracks = v.nvec;
  return v.vec;
}

static void activate_play(GtkMenuItem attribute((unused)) *menuitem,
                          gpointer attribute((unused)) user_data) {
  char **tracks = gather_selected(0);
  int n;
  
  gtk_label_set_text(GTK_LABEL(report_label), "adding track to queue");
  for(n = 0; tracks[n]; ++n)
    disorder_eclient_play(client, tracks[n], 0, 0);
}

static void activate_remove(GtkMenuItem attribute((unused)) *menuitem,
                            gpointer attribute((unused)) user_data) {
  /* TODO remove all selected tracks */
}

static void activate_properties(GtkMenuItem attribute((unused)) *menuitem,
                                gpointer attribute((unused)) user_data) {
  int ntracks;
  char **tracks = gather_selected(&ntracks);

  properties(ntracks, tracks);
}

static gboolean sensitive_play(struct choosenode attribute((unused)) *cn) {
  return !!files_selected;
}

static gboolean sensitive_remove(struct choosenode attribute((unused)) *cn) {
  return FALSE;                         /* not implemented yet */
}

static gboolean sensitive_properties(struct choosenode attribute((unused)) *cn) {
  return !!files_selected;
}

/* Main menu plumbing ------------------------------------------------------ */

static int choose_properties_sensitive(GtkWidget attribute((unused)) *w) {
  return !!files_selected;
}

static int choose_selectall_sensitive(GtkWidget attribute((unused)) *w) {
  return FALSE;                         /* TODO */
}

static void choose_properties_activate(GtkWidget attribute((unused)) *w) {
  activate_properties(0, 0);
}

static void choose_selectall_activate(GtkWidget attribute((unused)) *w) {
  /* TODO */
}

static const struct tabtype tabtype_choose = {
  choose_properties_sensitive,
  choose_selectall_sensitive,
  choose_properties_activate,
  choose_selectall_activate,
};

/* Public entry points ----------------------------------------------------- */

/* Create a track choice widget */
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
  searchentry = gtk_entry_new();
  g_signal_connect(searchentry, "changed", G_CALLBACK(searchentry_changed), 0);

  /* Cancel button to clear the search */
  clearsearch = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  g_signal_connect(G_OBJECT(clearsearch), "clicked",
                   G_CALLBACK(clearsearch_clicked), 0);

  /* hbox packs the search box and the cancel button together on a line */
  hbox = gtk_hbox_new(FALSE/*homogeneous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(hbox), searchentry,
                     TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);
  gtk_box_pack_end(GTK_BOX(hbox), clearsearch,
                   FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  
  /* chooselayout contains the currently visible subset of the track
   * namespace */
  chooselayout = gtk_layout_new(0, 0);
  root = newnode(0/*parent*/, "<root>", "All files", "",
                 CN_EXPANDABLE, fill_root_node);
  realroot = root;
  expand_node(root);                    /* will call redisplay_tree */
  /* Create the popup menu */
  menu = gtk_menu_new();
  g_signal_connect(menu, "destroy", G_CALLBACK(gtk_widget_destroyed), &menu);
  for(n = 0; n < NMENUITEMS; ++n) {
    menuitems[n].w = gtk_menu_item_new_with_label(menuitems[n].name);
    gtk_menu_attach(GTK_MENU(menu), menuitems[n].w, 0, 1, n, n + 1);
  }
  /* The layout is scrollable */
  scrolled = scroll_widget(GTK_WIDGET(chooselayout), "choose");

  /* The scrollable layout and the search hbox go together in a vbox */
  vbox = gtk_vbox_new(FALSE/*homogenous*/, 1/*spacing*/);
  gtk_box_pack_start(GTK_BOX(vbox), hbox,
                     FALSE/*expand*/, FALSE/*fill*/, 0/*padding*/);
  gtk_box_pack_end(GTK_BOX(vbox), scrolled,
                   TRUE/*expand*/, TRUE/*fill*/, 0/*padding*/);

  g_object_set_data(G_OBJECT(vbox), "type", (void *)&tabtype_choose);
  return vbox;
}

/* Called when something we care about here might have changed */
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
