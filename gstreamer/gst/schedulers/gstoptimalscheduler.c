/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstoptimalscheduler.c: Default scheduling code for most cases
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (debug_scheduler);
#define GST_CAT_DEFAULT debug_scheduler

#ifdef USE_COTHREADS
# include "cothreads_compat.h"
#else
# define COTHREADS_NAME_CAPITAL ""
# define COTHREADS_NAME 	""
#endif

#define GST_ELEMENT_SCHED_CONTEXT(elem)		((GstOptSchedulerCtx*) (GST_ELEMENT (elem)->sched_private))
#define GST_ELEMENT_SCHED_GROUP(elem)		(GST_ELEMENT_SCHED_CONTEXT (elem)->group)
/* need this first macro to not run into lvalue casts */
#define GST_PAD_DATAPEN(pad)			(GST_REAL_PAD(pad)->sched_private)
#define GST_PAD_DATALIST(pad)            	((GList*) GST_PAD_DATAPEN(pad))

#define GST_ELEMENT_COTHREAD_STOPPING			GST_ELEMENT_SCHEDULER_PRIVATE1
#define GST_ELEMENT_IS_COTHREAD_STOPPING(element)	GST_FLAG_IS_SET((element), GST_ELEMENT_COTHREAD_STOPPING)
#define GST_ELEMENT_VISITED				GST_ELEMENT_SCHEDULER_PRIVATE2
#define GST_ELEMENT_IS_VISITED(element)			GST_FLAG_IS_SET((element), GST_ELEMENT_VISITED)
#define GST_ELEMENT_SET_VISITED(element)		GST_FLAG_SET((element), GST_ELEMENT_VISITED)
#define GST_ELEMENT_UNSET_VISITED(element)		GST_FLAG_UNSET((element), GST_ELEMENT_VISITED)

typedef struct _GstOptScheduler GstOptScheduler;
typedef struct _GstOptSchedulerClass GstOptSchedulerClass;

#define GST_TYPE_OPT_SCHEDULER \
  (gst_opt_scheduler_get_type())
#define GST_OPT_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPT_SCHEDULER,GstOptScheduler))
#define GST_OPT_SCHEDULER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPT_SCHEDULER,GstOptSchedulerClass))
#define GST_IS_OPT_SCHEDULER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPT_SCHEDULER))
#define GST_IS_OPT_SCHEDULER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPT_SCHEDULER))

typedef enum
{
  GST_OPT_SCHEDULER_STATE_NONE,
  GST_OPT_SCHEDULER_STATE_STOPPED,
  GST_OPT_SCHEDULER_STATE_ERROR,
  GST_OPT_SCHEDULER_STATE_RUNNING,
  GST_OPT_SCHEDULER_STATE_INTERRUPTED
}
GstOptSchedulerState;

#define GST_OPT_LOCK(sched)	(g_static_rec_mutex_lock (&((GstOptScheduler *)sched)->lock))
#define GST_OPT_UNLOCK(sched)	(g_static_rec_mutex_unlock (&((GstOptScheduler *)sched)->lock))

struct _GstOptScheduler
{
  GstScheduler parent;

  GStaticRecMutex lock;

  GstOptSchedulerState state;

#ifdef USE_COTHREADS
  cothread_context *context;
#endif
  gint iterations;

  GSList *elements;
  GSList *chains;

  GList *runqueue;
  gint recursion;

  gint max_recursion;
  gint live_groups;
  gint live_chains;
  gint live_links;
};

struct _GstOptSchedulerClass
{
  GstSchedulerClass parent_class;
};

static GType _gst_opt_scheduler_type = 0;

typedef enum
{
  GST_OPT_SCHEDULER_CHAIN_DIRTY = (1 << 1),
  GST_OPT_SCHEDULER_CHAIN_DISABLED = (1 << 2),
  GST_OPT_SCHEDULER_CHAIN_RUNNING = (1 << 3)
}
GstOptSchedulerChainFlags;

#define GST_OPT_SCHEDULER_CHAIN_SET_DIRTY(chain)	((chain)->flags |= GST_OPT_SCHEDULER_CHAIN_DIRTY)
#define GST_OPT_SCHEDULER_CHAIN_SET_CLEAN(chain)	((chain)->flags &= ~GST_OPT_SCHEDULER_CHAIN_DIRTY)
#define GST_OPT_SCHEDULER_CHAIN_IS_DIRTY(chain) 	((chain)->flags & GST_OPT_SCHEDULER_CHAIN_DIRTY)

#define GST_OPT_SCHEDULER_CHAIN_DISABLE(chain) 		((chain)->flags |= GST_OPT_SCHEDULER_CHAIN_DISABLED)
#define GST_OPT_SCHEDULER_CHAIN_ENABLE(chain) 		((chain)->flags &= ~GST_OPT_SCHEDULER_CHAIN_DISABLED)
#define GST_OPT_SCHEDULER_CHAIN_IS_DISABLED(chain) 	((chain)->flags & GST_OPT_SCHEDULER_CHAIN_DISABLED)

typedef struct _GstOptSchedulerChain GstOptSchedulerChain;

struct _GstOptSchedulerChain
{
  gint refcount;

  GstOptScheduler *sched;

  GstOptSchedulerChainFlags flags;

  GSList *groups;               /* the groups in this chain */
  gint num_groups;
  gint num_enabled;
};

/* 
 * elements that are scheduled in one cothread 
 */
typedef enum
{
  GST_OPT_SCHEDULER_GROUP_DIRTY = (1 << 1),     /* this group has been modified */
  GST_OPT_SCHEDULER_GROUP_COTHREAD_STOPPING = (1 << 2), /* the group's cothread stops after one iteration */
  GST_OPT_SCHEDULER_GROUP_DISABLED = (1 << 3),  /* this group is disabled */
  GST_OPT_SCHEDULER_GROUP_RUNNING = (1 << 4),   /* this group is running */
  GST_OPT_SCHEDULER_GROUP_SCHEDULABLE = (1 << 5),       /* this group is schedulable */
  GST_OPT_SCHEDULER_GROUP_VISITED = (1 << 6)    /* this group is visited when finding links */
}
GstOptSchedulerGroupFlags;

typedef enum
{
  GST_OPT_SCHEDULER_GROUP_UNKNOWN = 3,
  GST_OPT_SCHEDULER_GROUP_GET = 1,
  GST_OPT_SCHEDULER_GROUP_LOOP = 2
}
GstOptSchedulerGroupType;

#define GST_OPT_SCHEDULER_GROUP_SET_FLAG(group,flag) 	((group)->flags |= (flag))
#define GST_OPT_SCHEDULER_GROUP_UNSET_FLAG(group,flag) 	((group)->flags &= ~(flag))
#define GST_OPT_SCHEDULER_GROUP_IS_FLAG_SET(group,flag) ((group)->flags & (flag))

#define GST_OPT_SCHEDULER_GROUP_DISABLE(group) 		((group)->flags |= GST_OPT_SCHEDULER_GROUP_DISABLED)
#define GST_OPT_SCHEDULER_GROUP_ENABLE(group) 		((group)->flags &= ~GST_OPT_SCHEDULER_GROUP_DISABLED)
#define GST_OPT_SCHEDULER_GROUP_IS_ENABLED(group) 	(!((group)->flags & GST_OPT_SCHEDULER_GROUP_DISABLED))
#define GST_OPT_SCHEDULER_GROUP_IS_DISABLED(group) 	((group)->flags & GST_OPT_SCHEDULER_GROUP_DISABLED)


typedef struct _GstOptSchedulerGroup GstOptSchedulerGroup;
typedef struct _GstOptSchedulerGroupLink GstOptSchedulerGroupLink;

/* used to keep track of links with other groups */
struct _GstOptSchedulerGroupLink
{
  GstOptSchedulerGroup *src;    /* the group we are linked with */
  GstOptSchedulerGroup *sink;   /* the group we are linked with */
  gint count;                   /* the number of links with the group */
};

#define IS_GROUP_LINK(link, srcg, sinkg)	((link->src == srcg && link->sink == sinkg) || \
		                                 (link->sink == srcg && link->src == sinkg))
#define OTHER_GROUP_LINK(link, group)		(link->src == group ? link->sink : link->src)

typedef int (*GroupScheduleFunction) (int argc, char *argv[]);

struct _GstOptSchedulerGroup
{
  GstOptSchedulerChain *chain;  /* the chain this group belongs to */
  GstOptSchedulerGroupFlags flags;      /* flags for this group */
  GstOptSchedulerGroupType type;        /* flags for this group */
  GstOptScheduler *sched;       /* the scheduler */

  gint refcount;

  GSList *elements;             /* elements of this group */
  gint num_elements;
  gint num_enabled;
  GstElement *entry;            /* the group's entry point */

  GSList *group_links;          /* other groups that are linked with this group */

#ifdef USE_COTHREADS
  cothread *cothread;           /* the cothread of this group */
#else
  GroupScheduleFunction schedulefunc;
#endif
  int argc;
  char **argv;
};

/* 
 * A group is a set of elements through which data can flow without switching
 * cothreads or without invoking the scheduler's run queue.
 */
static GstOptSchedulerGroup *ref_group (GstOptSchedulerGroup * group);
static GstOptSchedulerGroup *unref_group (GstOptSchedulerGroup * group);
static GstOptSchedulerGroup *create_group (GstOptSchedulerChain * chain,
    GstElement * element, GstOptSchedulerGroupType type);
static void destroy_group (GstOptSchedulerGroup * group);
static GstOptSchedulerGroup *add_to_group (GstOptSchedulerGroup * group,
    GstElement * element, gboolean with_links);
static GstOptSchedulerGroup *remove_from_group (GstOptSchedulerGroup * group,
    GstElement * element);
static void group_dec_links_for_element (GstOptSchedulerGroup * group,
    GstElement * element);
static void group_inc_links_for_element (GstOptSchedulerGroup * group,
    GstElement * element);
static GstOptSchedulerGroup *merge_groups (GstOptSchedulerGroup * group1,
    GstOptSchedulerGroup * group2);
static void setup_group_scheduler (GstOptScheduler * osched,
    GstOptSchedulerGroup * group);
static void destroy_group_scheduler (GstOptSchedulerGroup * group);
static void group_error_handler (GstOptSchedulerGroup * group);
static void group_element_set_enabled (GstOptSchedulerGroup * group,
    GstElement * element, gboolean enabled);
static gboolean schedule_group (GstOptSchedulerGroup * group);
static void get_group (GstElement * element, GstOptSchedulerGroup ** group);


/* 
 * A chain is a set of groups that are linked to each other.
 */
static void destroy_chain (GstOptSchedulerChain * chain);
static GstOptSchedulerChain *create_chain (GstOptScheduler * osched);
static GstOptSchedulerChain *ref_chain (GstOptSchedulerChain * chain);
static GstOptSchedulerChain *unref_chain (GstOptSchedulerChain * chain);
static GstOptSchedulerChain *add_to_chain (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group);
static GstOptSchedulerChain *remove_from_chain (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group);
static GstOptSchedulerChain *merge_chains (GstOptSchedulerChain * chain1,
    GstOptSchedulerChain * chain2);
static void chain_recursively_migrate_group (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group);
static void chain_group_set_enabled (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group, gboolean enabled);
static gboolean schedule_chain (GstOptSchedulerChain * chain);


/*
 * The schedule functions are the entry points for cothreads, or called directly
 * by gst_opt_scheduler_schedule_run_queue
 */
static int get_group_schedule_function (int argc, char *argv[]);
static int loop_group_schedule_function (int argc, char *argv[]);
static int unknown_group_schedule_function (int argc, char *argv[]);


/*
 * These wrappers are set on the pads as the chain handler (what happens when
 * gst_pad_push is called) or get handler (for gst_pad_pull).
 */
static void gst_opt_scheduler_loop_wrapper (GstPad * sinkpad, GstData * data);
static GstData *gst_opt_scheduler_get_wrapper (GstPad * srcpad);


/*
 * Without cothreads, gst_pad_push or gst_pad_pull on a loop-based group will
 * just queue the peer element on a list. We need to actually run the queue
 * instead of relying on cothreads to do the switch for us.
 */
#ifndef USE_COTHREADS
static void gst_opt_scheduler_schedule_run_queue (GstOptScheduler * osched,
    GstOptSchedulerGroup * only_group);
#endif


/* 
 * Scheduler private data for an element 
 */
typedef struct _GstOptSchedulerCtx GstOptSchedulerCtx;

typedef enum
{
  GST_OPT_SCHEDULER_CTX_DISABLED = (1 << 1)     /* the element is disabled */
}
GstOptSchedulerCtxFlags;

struct _GstOptSchedulerCtx
{
  GstOptSchedulerGroup *group;  /* the group this element belongs to */

  GstOptSchedulerCtxFlags flags;        /* flags for this element */
};


/*
 * Implementation of GstScheduler
 */
enum
{
  ARG_0,
  ARG_ITERATIONS,
  ARG_MAX_RECURSION
};

static void gst_opt_scheduler_class_init (GstOptSchedulerClass * klass);
static void gst_opt_scheduler_init (GstOptScheduler * scheduler);

static void gst_opt_scheduler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_opt_scheduler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_opt_scheduler_dispose (GObject * object);
static void gst_opt_scheduler_finalize (GObject * object);

static void gst_opt_scheduler_setup (GstScheduler * sched);
static void gst_opt_scheduler_reset (GstScheduler * sched);
static void gst_opt_scheduler_add_element (GstScheduler * sched,
    GstElement * element);
static void gst_opt_scheduler_remove_element (GstScheduler * sched,
    GstElement * element);
static GstElementStateReturn gst_opt_scheduler_state_transition (GstScheduler *
    sched, GstElement * element, gint transition);
static void gst_opt_scheduler_scheduling_change (GstScheduler * sched,
    GstElement * element);
static gboolean gst_opt_scheduler_yield (GstScheduler * sched,
    GstElement * element);
static gboolean gst_opt_scheduler_interrupt (GstScheduler * sched,
    GstElement * element);
static void gst_opt_scheduler_error (GstScheduler * sched,
    GstElement * element);
static void gst_opt_scheduler_pad_link (GstScheduler * sched, GstPad * srcpad,
    GstPad * sinkpad);
static void gst_opt_scheduler_pad_unlink (GstScheduler * sched, GstPad * srcpad,
    GstPad * sinkpad);
static GstSchedulerState gst_opt_scheduler_iterate (GstScheduler * sched);

static void gst_opt_scheduler_show (GstScheduler * sched);

static GstSchedulerClass *parent_class = NULL;

static GType
gst_opt_scheduler_get_type (void)
{
  if (!_gst_opt_scheduler_type) {
    static const GTypeInfo scheduler_info = {
      sizeof (GstOptSchedulerClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_opt_scheduler_class_init,
      NULL,
      NULL,
      sizeof (GstOptScheduler),
      0,
      (GInstanceInitFunc) gst_opt_scheduler_init,
      NULL
    };

    _gst_opt_scheduler_type = g_type_register_static (GST_TYPE_SCHEDULER,
        "GstOpt" COTHREADS_NAME_CAPITAL "Scheduler", &scheduler_info, 0);
  }
  return _gst_opt_scheduler_type;
}

static void
gst_opt_scheduler_class_init (GstOptSchedulerClass * klass)
{
  GObjectClass *gobject_class;
  GstObjectClass *gstobject_class;
  GstSchedulerClass *gstscheduler_class;

  gobject_class = (GObjectClass *) klass;
  gstobject_class = (GstObjectClass *) klass;
  gstscheduler_class = (GstSchedulerClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_SCHEDULER);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_get_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_opt_scheduler_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_opt_scheduler_finalize);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ITERATIONS,
      g_param_spec_int ("iterations", "Iterations",
          "Number of groups to schedule in one iteration (-1 == until EOS/error)",
          -1, G_MAXINT, 1, G_PARAM_READWRITE));
#ifndef USE_COTHREADS
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MAX_RECURSION,
      g_param_spec_int ("max_recursion", "Max recursion",
          "Maximum number of recursions", 1, G_MAXINT, 100, G_PARAM_READWRITE));
#endif

  gstscheduler_class->setup = GST_DEBUG_FUNCPTR (gst_opt_scheduler_setup);
  gstscheduler_class->reset = GST_DEBUG_FUNCPTR (gst_opt_scheduler_reset);
  gstscheduler_class->add_element =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_add_element);
  gstscheduler_class->remove_element =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_remove_element);
  gstscheduler_class->state_transition =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_state_transition);
  gstscheduler_class->scheduling_change =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_scheduling_change);
  gstscheduler_class->yield = GST_DEBUG_FUNCPTR (gst_opt_scheduler_yield);
  gstscheduler_class->interrupt =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_interrupt);
  gstscheduler_class->error = GST_DEBUG_FUNCPTR (gst_opt_scheduler_error);
  gstscheduler_class->pad_link = GST_DEBUG_FUNCPTR (gst_opt_scheduler_pad_link);
  gstscheduler_class->pad_unlink =
      GST_DEBUG_FUNCPTR (gst_opt_scheduler_pad_unlink);
  gstscheduler_class->clock_wait = NULL;
  gstscheduler_class->iterate = GST_DEBUG_FUNCPTR (gst_opt_scheduler_iterate);
  gstscheduler_class->show = GST_DEBUG_FUNCPTR (gst_opt_scheduler_show);

#ifdef USE_COTHREADS
  do_cothreads_init (NULL);
#endif
}

static void
gst_opt_scheduler_init (GstOptScheduler * scheduler)
{
  g_static_rec_mutex_init (&scheduler->lock);

  scheduler->elements = NULL;
  scheduler->iterations = 1;
  scheduler->max_recursion = 100;
  scheduler->live_groups = 0;
  scheduler->live_chains = 0;
  scheduler->live_links = 0;
}

static void
gst_opt_scheduler_dispose (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_opt_scheduler_finalize (GObject * object)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (object);

  g_static_rec_mutex_free (&osched->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
#ifdef USE_COTHREADS
  if (!gst_scheduler_register (plugin, "opt" COTHREADS_NAME,
          "An optimal scheduler using " COTHREADS_NAME " cothreads",
          gst_opt_scheduler_get_type ()))
#else
  if (!gst_scheduler_register (plugin, "opt",
          "An optimal scheduler using no cothreads",
          gst_opt_scheduler_get_type ()))
#endif
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (debug_scheduler, "scheduler", 0,
      "optimal scheduler");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gstopt" COTHREADS_NAME "scheduler",
    "An optimal scheduler using " COTHREADS_NAME " cothreads",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE, GST_ORIGIN);


static GstOptSchedulerChain *
ref_chain (GstOptSchedulerChain * chain)
{
  GST_LOG ("ref chain %p %d->%d", chain, chain->refcount, chain->refcount + 1);
  chain->refcount++;

  return chain;
}

static GstOptSchedulerChain *
unref_chain (GstOptSchedulerChain * chain)
{
  GST_LOG ("unref chain %p %d->%d", chain,
      chain->refcount, chain->refcount - 1);

  if (--chain->refcount == 0) {
    destroy_chain (chain);
    chain = NULL;
  }

  return chain;
}

static GstOptSchedulerChain *
create_chain (GstOptScheduler * osched)
{
  GstOptSchedulerChain *chain;

  chain = g_new0 (GstOptSchedulerChain, 1);
  chain->sched = osched;
  chain->refcount = 1;
  chain->flags = GST_OPT_SCHEDULER_CHAIN_DISABLED;
  osched->live_chains++;

  gst_object_ref (GST_OBJECT (osched));
  osched->chains = g_slist_prepend (osched->chains, chain);

  GST_LOG ("new chain %p, %d live chains now", chain, osched->live_chains);

  return chain;
}

static void
destroy_chain (GstOptSchedulerChain * chain)
{
  GstOptScheduler *osched;

  GST_LOG ("destroy chain %p", chain);

  g_assert (chain->num_groups == 0);
  g_assert (chain->groups == NULL);

  osched = chain->sched;
  osched->chains = g_slist_remove (osched->chains, chain);
  osched->live_chains--;

  GST_LOG ("%d live chains now", osched->live_chains);

  gst_object_unref (GST_OBJECT (osched));

  g_free (chain);
}

static GstOptSchedulerChain *
add_to_chain (GstOptSchedulerChain * chain, GstOptSchedulerGroup * group)
{
  gboolean enabled;

  GST_LOG ("adding group %p to chain %p", group, chain);

  g_assert (group->chain == NULL);

  group = ref_group (group);

  group->chain = ref_chain (chain);
  chain->groups = g_slist_prepend (chain->groups, group);
  chain->num_groups++;

  enabled = GST_OPT_SCHEDULER_GROUP_IS_ENABLED (group);

  if (enabled) {
    /* we can now setup the scheduling of the group */
    setup_group_scheduler (chain->sched, group);

    chain->num_enabled++;
    if (chain->num_enabled == chain->num_groups) {
      GST_LOG ("enabling chain %p after adding of enabled group", chain);
      GST_OPT_SCHEDULER_CHAIN_ENABLE (chain);
    }
  }

  /* queue a resort of the group list, which determines which group will be run
   * first. */
  GST_OPT_SCHEDULER_CHAIN_SET_DIRTY (chain);

  return chain;
}

static GstOptSchedulerChain *
remove_from_chain (GstOptSchedulerChain * chain, GstOptSchedulerGroup * group)
{
  gboolean enabled;

  GST_LOG ("removing group %p from chain %p", group, chain);

  if (!chain)
    return NULL;

  g_assert (group);
  g_assert (group->chain == chain);

  enabled = GST_OPT_SCHEDULER_GROUP_IS_ENABLED (group);

  group->chain = NULL;
  chain->groups = g_slist_remove (chain->groups, group);
  chain->num_groups--;
  unref_group (group);

  if (chain->num_groups == 0)
    chain = unref_chain (chain);
  else {
    /* removing an enabled group from the chain decrements the 
     * enabled counter */
    if (enabled) {
      chain->num_enabled--;
      if (chain->num_enabled == 0) {
        GST_LOG ("disabling chain %p after removal of the only enabled group",
            chain);
        GST_OPT_SCHEDULER_CHAIN_DISABLE (chain);
      }
    } else {
      if (chain->num_enabled == chain->num_groups) {
        GST_LOG ("enabling chain %p after removal of the only disabled group",
            chain);
        GST_OPT_SCHEDULER_CHAIN_ENABLE (chain);
      }
    }
  }

  GST_OPT_SCHEDULER_CHAIN_SET_DIRTY (chain);

  chain = unref_chain (chain);
  return chain;
}

static GstOptSchedulerChain *
merge_chains (GstOptSchedulerChain * chain1, GstOptSchedulerChain * chain2)
{
  GSList *walk;

  g_assert (chain1 != NULL);

  GST_LOG ("merging chain %p and %p", chain1, chain2);

  /* FIXME: document how chain2 can be NULL */
  if (chain1 == chain2 || chain2 == NULL)
    return chain1;

  /* switch if it's more efficient */
  if (chain1->num_groups < chain2->num_groups) {
    GstOptSchedulerChain *tmp = chain2;

    chain2 = chain1;
    chain1 = tmp;
  }

  walk = chain2->groups;
  while (walk) {
    GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) walk->data;

    walk = g_slist_next (walk);

    GST_LOG ("reparenting group %p from chain %p to %p", group, chain2, chain1);

    ref_group (group);

    remove_from_chain (chain2, group);
    add_to_chain (chain1, group);

    unref_group (group);
  }

  /* chain2 is now freed, if nothing else was referencing it before */

  return chain1;
}

/* sorts the group list so that terminal sinks come first -- prevents pileup of
 * datas in datapens */
static void
sort_chain (GstOptSchedulerChain * chain)
{
  GSList *original = chain->groups;
  GSList *new = NULL;
  GSList *walk, *links, *this;

  /* if there's only one group, just return */
  if (!original->next)
    return;
  /* otherwise, we know that all groups are somehow linked together */

  GST_LOG ("sorting chain %p (%d groups)", chain, g_slist_length (original));

  /* first find the terminal sinks */
  for (walk = original; walk;) {
    GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) walk->data;

    this = walk;
    walk = walk->next;
    if (group->group_links) {
      gboolean is_sink = TRUE;

      for (links = group->group_links; links; links = links->next)
        if (((GstOptSchedulerGroupLink *) links->data)->src == group)
          is_sink = FALSE;
      if (is_sink) {
        /* found one */
        original = g_slist_remove_link (original, this);
        new = g_slist_concat (new, this);
      }
    }
  }
  g_assert (new != NULL);

  /* now look for the elements that are linked to the terminal sinks */
  for (walk = new; walk; walk = walk->next) {
    GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) walk->data;

    for (links = group->group_links; links; links = links->next) {
      this =
          g_slist_find (original,
          ((GstOptSchedulerGroupLink *) links->data)->src);
      if (this) {
        original = g_slist_remove_link (original, this);
        new = g_slist_concat (new, this);
      }
    }
  }
  g_assert (original == NULL);

  chain->groups = new;
}

static void
chain_group_set_enabled (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group, gboolean enabled)
{
  gboolean oldstate;

  g_assert (group != NULL);
  g_assert (chain != NULL);

  GST_LOG
      ("request to %d group %p in chain %p, have %d groups enabled out of %d",
      enabled, group, chain, chain->num_enabled, chain->num_groups);

  oldstate = (GST_OPT_SCHEDULER_GROUP_IS_ENABLED (group) ? TRUE : FALSE);
  if (oldstate == enabled) {
    GST_LOG ("group %p in chain %p was in correct state", group, chain);
    return;
  }

  if (enabled)
    GST_OPT_SCHEDULER_GROUP_ENABLE (group);
  else
    GST_OPT_SCHEDULER_GROUP_DISABLE (group);

  if (enabled) {
    g_assert (chain->num_enabled < chain->num_groups);

    chain->num_enabled++;

    GST_DEBUG ("enable group %p in chain %p, now %d groups enabled out of %d",
        group, chain, chain->num_enabled, chain->num_groups);

    /* OK to call even if the scheduler (cothread context / schedulerfunc) was
       setup already -- will get destroyed when the group is destroyed */
    setup_group_scheduler (chain->sched, group);

    if (chain->num_enabled == chain->num_groups) {
      GST_DEBUG ("enable chain %p", chain);
      GST_OPT_SCHEDULER_CHAIN_ENABLE (chain);
    }
  } else {
    g_assert (chain->num_enabled > 0);

    chain->num_enabled--;
    GST_DEBUG ("disable group %p in chain %p, now %d groups enabled out of %d",
        group, chain, chain->num_enabled, chain->num_groups);

    if (chain->num_enabled == 0) {
      GST_DEBUG ("disable chain %p", chain);
      GST_OPT_SCHEDULER_CHAIN_DISABLE (chain);
    }
  }
}

/* recursively migrate the group and all connected groups into the new chain */
static void
chain_recursively_migrate_group (GstOptSchedulerChain * chain,
    GstOptSchedulerGroup * group)
{
  GSList *links;

  /* group already in chain */
  if (group->chain == chain)
    return;

  /* first remove the group from its old chain */
  remove_from_chain (group->chain, group);
  /* add to new chain */
  add_to_chain (chain, group);

  /* then follow all links */
  for (links = group->group_links; links; links = g_slist_next (links)) {
    GstOptSchedulerGroupLink *link = (GstOptSchedulerGroupLink *) links->data;

    chain_recursively_migrate_group (chain, OTHER_GROUP_LINK (link, group));
  }
}

static GstOptSchedulerGroup *
ref_group (GstOptSchedulerGroup * group)
{
  GST_LOG ("ref group %p %d->%d", group, group->refcount, group->refcount + 1);

  group->refcount++;

  return group;
}

static GstOptSchedulerGroup *
unref_group (GstOptSchedulerGroup * group)
{
  GST_LOG ("unref group %p %d->%d", group,
      group->refcount, group->refcount - 1);

  if (--group->refcount == 0) {
    destroy_group (group);
    group = NULL;
  }

  return group;
}

static GstOptSchedulerGroup *
create_group (GstOptSchedulerChain * chain, GstElement * element,
    GstOptSchedulerGroupType type)
{
  GstOptSchedulerGroup *group;

  group = g_new0 (GstOptSchedulerGroup, 1);
  GST_LOG ("new group %p, type %d", group, type);
  group->refcount = 1;          /* float... */
  group->flags = GST_OPT_SCHEDULER_GROUP_DISABLED;
  group->type = type;
  group->sched = chain->sched;
  group->sched->live_groups++;

  add_to_group (group, element, FALSE);
  add_to_chain (chain, group);
  group = unref_group (group);  /* ...and sink. */

  GST_LOG ("%d live groups now", group->sched->live_groups);
  /* group's refcount is now 2 (one for the element, one for the chain) */

  return group;
}

static void
destroy_group (GstOptSchedulerGroup * group)
{
  GST_LOG ("destroy group %p", group);

  g_assert (group != NULL);
  g_assert (group->elements == NULL);
  g_assert (group->chain == NULL);
  g_assert (group->group_links == NULL);

  if (group->flags & GST_OPT_SCHEDULER_GROUP_SCHEDULABLE)
    destroy_group_scheduler (group);

  group->sched->live_groups--;
  GST_LOG ("%d live groups now", group->sched->live_groups);

  g_free (group);
}

static GstOptSchedulerGroup *
add_to_group (GstOptSchedulerGroup * group, GstElement * element,
    gboolean with_links)
{
  g_assert (group != NULL);
  g_assert (element != NULL);

  GST_DEBUG ("adding element %p \"%s\" to group %p", element,
      GST_ELEMENT_NAME (element), group);

  if (GST_ELEMENT_IS_DECOUPLED (element)) {
    GST_DEBUG ("element \"%s\" is decoupled, not adding to group %p",
        GST_ELEMENT_NAME (element), group);
    return group;
  }

  g_assert (GST_ELEMENT_SCHED_GROUP (element) == NULL);

  /* first increment the links that this group has with other groups through
   * this element */
  if (with_links)
    group_inc_links_for_element (group, element);

  /* Ref the group... */
  GST_ELEMENT_SCHED_GROUP (element) = ref_group (group);

  gst_object_ref (GST_OBJECT (element));
  group->elements = g_slist_prepend (group->elements, element);
  group->num_elements++;

  if (gst_element_get_state (element) == GST_STATE_PLAYING) {
    group_element_set_enabled (group, element, TRUE);
  }

  return group;
}

/* we need to remove a decoupled element from the scheduler, this
 * involves finding all the groups that have this element as an
 * entry point and disabling them. */
static void
remove_decoupled (GstScheduler * sched, GstElement * element)
{
  GSList *chains;
  GList *schedulers;
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);

  GST_DEBUG_OBJECT (sched, "removing decoupled element \"%s\"",
      GST_OBJECT_NAME (element));

  for (chains = osched->chains; chains; chains = g_slist_next (chains)) {
    GstOptSchedulerChain *chain = (GstOptSchedulerChain *) chains->data;
    GSList *groups;

    for (groups = chain->groups; groups; groups = g_slist_next (groups)) {
      GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) groups->data;

      if (group->entry) {
        GST_DEBUG_OBJECT (sched, "group %p, entry %s", group,
            GST_STR_NULL (GST_OBJECT_NAME (group->entry)));
      }

      if (group->entry == element) {
        GST_DEBUG ("clearing element %p \"%s\" as entry from group %p",
            element, GST_ELEMENT_NAME (element), group);
        group->entry = NULL;
        group->type = GST_OPT_SCHEDULER_GROUP_UNKNOWN;
      }
    }
  }
  /* and remove from any child scheduler */
  for (schedulers = sched->schedulers; schedulers;
      schedulers = g_list_next (schedulers)) {
    remove_decoupled (GST_SCHEDULER (schedulers->data), element);
  }
}

static GstOptSchedulerGroup *
remove_from_group (GstOptSchedulerGroup * group, GstElement * element)
{
  GST_DEBUG ("removing element %p \"%s\" from group %p",
      element, GST_ELEMENT_NAME (element), group);

  g_assert (group != NULL);
  g_assert (element != NULL);
  /* this assert also catches the decoupled elements */
  g_assert (GST_ELEMENT_SCHED_GROUP (element) == group);

  /* first decrement the links that this group has with other groups through
   * this element */
  group_dec_links_for_element (group, element);

  if (gst_element_get_state (element) == GST_STATE_PLAYING) {
    group_element_set_enabled (group, element, FALSE);
  }

  group->elements = g_slist_remove (group->elements, element);
  group->num_elements--;

  /* if the element was an entry point in the group, clear the group's
   * entry point, and mark it as unknown */
  if (group->entry == element) {
    GST_DEBUG ("clearing element %p \"%s\" as entry from group %p",
        element, GST_ELEMENT_NAME (element), group);
    group->entry = NULL;
    group->type = GST_OPT_SCHEDULER_GROUP_UNKNOWN;
  }

  GST_ELEMENT_SCHED_GROUP (element) = NULL;
  gst_object_unref (GST_OBJECT (element));

  if (group->num_elements == 0) {
    GST_LOG ("group %p is now empty", group);
    /* don't know in what case group->chain would be NULL, but putting this here
       in deference to 0.8 -- remove me in 0.9 */
    if (group->chain) {
      GST_LOG ("removing group %p from its chain", group);
      chain_group_set_enabled (group->chain, group, FALSE);
      remove_from_chain (group->chain, group);
    }
  }
  group = unref_group (group);

  return group;
}

/* check if an element is part of the given group. We have to be carefull
 * as decoupled elements are added as entry but are not added to the elements 
 * list */
static gboolean
group_has_element (GstOptSchedulerGroup * group, GstElement * element)
{
  if (group->entry == element)
    return TRUE;

  return (g_slist_find (group->elements, element) != NULL);
}

/* FIXME need to check if the groups are of the same type -- otherwise need to
   setup the scheduler again, if it is setup */
static GstOptSchedulerGroup *
merge_groups (GstOptSchedulerGroup * group1, GstOptSchedulerGroup * group2)
{
  g_assert (group1 != NULL);

  GST_DEBUG ("merging groups %p and %p", group1, group2);

  if (group1 == group2 || group2 == NULL)
    return group1;

  /* make sure they end up in the same chain */
  merge_chains (group1->chain, group2->chain);

  while (group2 && group2->elements) {
    GstElement *element = (GstElement *) group2->elements->data;

    group2 = remove_from_group (group2, element);
    add_to_group (group1, element, TRUE);
  }

  return group1;
}

/* setup the scheduler context for a group. The right schedule function
 * is selected based on the group type and cothreads are created if 
 * needed */
static void
setup_group_scheduler (GstOptScheduler * osched, GstOptSchedulerGroup * group)
{
  GroupScheduleFunction wrapper;

  GST_DEBUG ("setup group %p scheduler, type %d", group, group->type);

  wrapper = unknown_group_schedule_function;

  /* figure out the wrapper function for this group */
  if (group->type == GST_OPT_SCHEDULER_GROUP_GET)
    wrapper = get_group_schedule_function;
  else if (group->type == GST_OPT_SCHEDULER_GROUP_LOOP)
    wrapper = loop_group_schedule_function;

#ifdef USE_COTHREADS
  /* we can only initialize the cothread stuff when we have
   * a cothread context */
  if (osched->context) {
    if (!(group->flags & GST_OPT_SCHEDULER_GROUP_SCHEDULABLE)) {
      do_cothread_create (group->cothread, osched->context,
          (cothread_func) wrapper, 0, (char **) group);
    } else {
      do_cothread_setfunc (group->cothread, osched->context,
          (cothread_func) wrapper, 0, (char **) group);
    }
  }
#else
  group->schedulefunc = wrapper;
  group->argc = 0;
  group->argv = (char **) group;
#endif
  group->flags |= GST_OPT_SCHEDULER_GROUP_SCHEDULABLE;
}

static void
destroy_group_scheduler (GstOptSchedulerGroup * group)
{
  g_assert (group);

  if (group->flags & GST_OPT_SCHEDULER_GROUP_RUNNING)
    g_warning ("destroying running group scheduler");

#ifdef USE_COTHREADS
  if (group->cothread) {
    do_cothread_destroy (group->cothread);
    group->cothread = NULL;
  }
#else
  group->schedulefunc = NULL;
  group->argc = 0;
  group->argv = NULL;
#endif

  group->flags &= ~GST_OPT_SCHEDULER_GROUP_SCHEDULABLE;
}

static void
group_error_handler (GstOptSchedulerGroup * group)
{
  GST_DEBUG ("group %p has errored", group);

  chain_group_set_enabled (group->chain, group, FALSE);
  group->chain->sched->state = GST_OPT_SCHEDULER_STATE_ERROR;
}

/* this function enables/disables an element, it will set/clear a flag on the element 
 * and tells the chain that the group is enabled if all elements inside the group are
 * enabled */
static void
group_element_set_enabled (GstOptSchedulerGroup * group, GstElement * element,
    gboolean enabled)
{
  g_assert (group != NULL);
  g_assert (element != NULL);

  GST_LOG
      ("request to %d element %s in group %p, have %d elements enabled out of %d",
      enabled, GST_ELEMENT_NAME (element), group, group->num_enabled,
      group->num_elements);

  /* Note that if an unlinked PLAYING element is added to a bin, we have to
     create a new group to hold the element, and this function will be called
     before the group is added to the chain. Thus we have a valid case for
     group->chain==NULL. */

  if (enabled) {
    g_assert (group->num_enabled < group->num_elements);

    group->num_enabled++;

    GST_DEBUG
        ("enable element %s in group %p, now %d elements enabled out of %d",
        GST_ELEMENT_NAME (element), group, group->num_enabled,
        group->num_elements);

    if (group->num_enabled == group->num_elements) {
      if (!group->chain) {
        GST_DEBUG ("enable chainless group %p", group);
        GST_OPT_SCHEDULER_GROUP_ENABLE (group);
      } else {
        GST_LOG ("enable group %p", group);
        chain_group_set_enabled (group->chain, group, TRUE);
      }
    }
  } else {
    g_assert (group->num_enabled > 0);

    group->num_enabled--;

    GST_DEBUG
        ("disable element %s in group %p, now %d elements enabled out of %d",
        GST_ELEMENT_NAME (element), group, group->num_enabled,
        group->num_elements);

    if (group->num_enabled == 0) {
      if (!group->chain) {
        GST_DEBUG ("disable chainless group %p", group);
        GST_OPT_SCHEDULER_GROUP_DISABLE (group);
      } else {
        GST_LOG ("disable group %p", group);
        chain_group_set_enabled (group->chain, group, FALSE);
      }
    }
  }
}

/* a group is scheduled by doing a cothread switch to it or
 * by calling the schedule function. In the non-cothread case
 * we cannot run already running groups so we return FALSE here
 * to indicate this to the caller */
static gboolean
schedule_group (GstOptSchedulerGroup * group)
{
  if (!group->entry) {
    GST_INFO ("not scheduling group %p without entry", group);
    /* FIXME, we return true here, while the group is actually
     * not schedulable. We might want to disable the element that caused
     * this group to be scheduled instead */
    return TRUE;
  }
#ifdef USE_COTHREADS
  if (group->cothread)
    do_cothread_switch (group->cothread);
  else
    g_warning ("(internal error): trying to schedule group without cothread");
  return TRUE;
#else
  /* cothreads automatically call the pre- and post-run functions for us;
   * without cothreads we need to call them manually */
  if (group->schedulefunc == NULL) {
    GST_INFO ("not scheduling group %p without schedulefunc", group);
    return FALSE;
  } else {
    GSList *l, *lcopy;
    GstElement *entry = NULL;

    lcopy = g_slist_copy (group->elements);
    /* also add entry point, this is made so that decoupled elements
     * are also reffed since they are not added to the list of group
     * elements. */
    if (group->entry && GST_ELEMENT_IS_DECOUPLED (group->entry)) {
      entry = group->entry;
      gst_object_ref (GST_OBJECT (entry));
    }

    for (l = lcopy; l; l = l->next) {
      GstElement *e = (GstElement *) l->data;

      gst_object_ref (GST_OBJECT (e));
      if (e->pre_run_func)
        e->pre_run_func (e);
    }

    group->schedulefunc (group->argc, group->argv);

    for (l = lcopy; l; l = l->next) {
      GstElement *e = (GstElement *) l->data;

      if (e->post_run_func)
        e->post_run_func (e);

      gst_object_unref (GST_OBJECT (e));
    }
    if (entry)
      gst_object_unref (GST_OBJECT (entry));
    g_slist_free (lcopy);
  }
  return TRUE;
#endif
}

#ifndef USE_COTHREADS
static void
gst_opt_scheduler_schedule_run_queue (GstOptScheduler * osched,
    GstOptSchedulerGroup * only_group)
{
  GST_LOG_OBJECT (osched, "running queue: %d groups, recursed %d times",
      g_list_length (osched->runqueue),
      osched->recursion, g_list_length (osched->runqueue));

  /* note that we have a ref on each group on the queue (unref after running) */

  /* make sure we don't exceed max_recursion */
  if (osched->recursion > osched->max_recursion) {
    osched->state = GST_OPT_SCHEDULER_STATE_ERROR;
    return;
  }

  osched->recursion++;

  do {
    GstOptSchedulerGroup *group;
    gboolean res;

    if (only_group)
      group = only_group;
    else
      group = (GstOptSchedulerGroup *) osched->runqueue->data;

    /* runqueue holds refcount to group */
    osched->runqueue = g_list_remove (osched->runqueue, group);

    GST_LOG_OBJECT (osched, "scheduling group %p", group);

    if (GST_OPT_SCHEDULER_GROUP_IS_ENABLED (group)) {
      res = schedule_group (group);
    } else {
      GST_INFO_OBJECT (osched,
          "group was disabled while it was on the queue, not scheduling");
      res = TRUE;
    }
    if (!res) {
      g_warning ("error scheduling group %p", group);
      group_error_handler (group);
    } else {
      GST_LOG_OBJECT (osched, "done scheduling group %p", group);
    }
    unref_group (group);
  } while (osched->runqueue && !only_group);

  GST_LOG_OBJECT (osched, "run queue length after scheduling %d",
      g_list_length (osched->runqueue));

  osched->recursion--;
}
#endif

/* a chain is scheduled by picking the first active group and scheduling it */
static gboolean
schedule_chain (GstOptSchedulerChain * chain)
{
  GSList *groups;
  GstOptScheduler *osched;
  gboolean scheduled = FALSE;

  osched = chain->sched;

  /* if the chain has changed, we need to resort the groups so we enter in the
     proper place */
  if (GST_OPT_SCHEDULER_CHAIN_IS_DIRTY (chain))
    sort_chain (chain);
  GST_OPT_SCHEDULER_CHAIN_SET_CLEAN (chain);

  /* FIXME handle the case where the groups change during scheduling */
  groups = chain->groups;
  while (groups) {
    GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) groups->data;

    if (!GST_OPT_SCHEDULER_GROUP_IS_DISABLED (group)) {
      ref_group (group);
      GST_LOG ("scheduling group %p in chain %p", group, chain);

#ifdef USE_COTHREADS
      schedule_group (group);
#else
      osched->recursion = 0;
      if (!g_list_find (osched->runqueue, group)) {
        ref_group (group);
        osched->runqueue = g_list_append (osched->runqueue, group);
      }
      gst_opt_scheduler_schedule_run_queue (osched, NULL);
#endif

      scheduled = TRUE;

      GST_LOG ("done scheduling group %p in chain %p", group, chain);
      unref_group (group);
      break;
    }

    groups = g_slist_next (groups);
  }
  return scheduled;
}

/* this function is inserted in the gethandler when you are not
 * supposed to call _pull on the pad. */
static GstData *
get_invalid_call (GstPad * pad)
{
  GST_ELEMENT_ERROR (GST_PAD_PARENT (pad), CORE, PAD, (NULL),
      ("get on pad %s:%s but the peer is operating chain based and so is not "
          "allowed to pull, fix the element.", GST_DEBUG_PAD_NAME (pad)));

  return GST_DATA (gst_event_new (GST_EVENT_INTERRUPT));
}

/* this function is inserted in the chainhandler when you are not
 * supposed to call _push on the pad. */
static void
chain_invalid_call (GstPad * pad, GstData * data)
{
  GST_ELEMENT_ERROR (GST_PAD_PARENT (pad), CORE, PAD, (NULL),
      ("chain on pad %s:%s but the pad is get based",
          GST_DEBUG_PAD_NAME (pad)));

  gst_data_unref (data);
}

/* a get-based group is scheduled by getting a buffer from the get based
 * entry point and by pushing the buffer to the peer.
 * We also set the running flag on this group for as long as this
 * function is running. */
static int
get_group_schedule_function (int argc, char *argv[])
{
  GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) argv;
  GstElement *entry = group->entry;
  const GList *pads;
  GstOptScheduler *osched;

  /* what if the entry point disappeared? */
  if (entry == NULL || group->chain == NULL)
    return 0;

  osched = group->chain->sched;

  pads = gst_element_get_pad_list (entry);

  GST_LOG ("executing get-based group %p", group);

  group->flags |= GST_OPT_SCHEDULER_GROUP_RUNNING;

  GST_OPT_UNLOCK (osched);
  while (pads) {
    GstData *data;
    GstPad *pad = GST_PAD (pads->data);

    pads = g_list_next (pads);

    /* skip sinks and ghostpads */
    if (!GST_PAD_IS_SRC (pad) || !GST_IS_REAL_PAD (pad))
      continue;

    GST_DEBUG ("doing get and push on pad \"%s:%s\" in group %p",
        GST_DEBUG_PAD_NAME (pad), group);

    data = gst_pad_call_get_function (pad);
    if (data) {
      if (GST_EVENT_IS_INTERRUPT (data)) {
        GST_DEBUG ("unreffing interrupt event %p", data);
        gst_event_unref (GST_EVENT (data));
        break;
      }
      gst_pad_push (pad, data);
    }
  }
  GST_OPT_LOCK (osched);

  group->flags &= ~GST_OPT_SCHEDULER_GROUP_RUNNING;

  return 0;
}

/* a loop-based group is scheduled by calling the loop function
 * on the entry point. 
 * We also set the running flag on this group for as long as this
 * function is running.
 * This function should be called with the scheduler lock held. */
static int
loop_group_schedule_function (int argc, char *argv[])
{
  GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) argv;
  GstElement *entry = group->entry;

  GST_LOG ("executing loop-based group %p", group);

  group->flags |= GST_OPT_SCHEDULER_GROUP_RUNNING;

  GST_DEBUG ("calling loopfunc of element %s in group %p",
      GST_ELEMENT_NAME (entry), group);

  if (group->chain == NULL)
    return 0;

  if (entry->loopfunc) {
    GstOptScheduler *osched = group->chain->sched;

    GST_OPT_UNLOCK (osched);
    entry->loopfunc (entry);
    GST_OPT_LOCK (osched);
  } else
    group_error_handler (group);

  GST_LOG ("returned from loopfunc of element %s in group %p",
      GST_ELEMENT_NAME (entry), group);

  group->flags &= ~GST_OPT_SCHEDULER_GROUP_RUNNING;

  return 0;

}

/* the function to schedule an unknown group, which just gives an error */
static int
unknown_group_schedule_function (int argc, char *argv[])
{
  GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) argv;

  g_warning ("(internal error) unknown group type %d, disabling\n",
      group->type);
  group_error_handler (group);

  return 0;
}

/* this function is called when the first element of a chain-loop or a loop-loop
 * link performs a push to the loop element. We then schedule the
 * group with the loop-based element until the datapen is empty */
static void
gst_opt_scheduler_loop_wrapper (GstPad * sinkpad, GstData * data)
{
  GstOptSchedulerGroup *group;
  GstOptScheduler *osched;
  GstRealPad *peer;

  group = GST_ELEMENT_SCHED_GROUP (GST_PAD_PARENT (sinkpad));
  osched = group->chain->sched;
  peer = GST_RPAD_PEER (sinkpad);

  GST_LOG ("chain handler for loop-based pad %" GST_PTR_FORMAT, sinkpad);

  GST_OPT_LOCK (osched);
#ifdef USE_COTHREADS
  if (GST_PAD_DATALIST (peer)) {
    g_warning ("deadlock detected, disabling group %p", group);
    group_error_handler (group);
  } else {
    GST_LOG ("queueing data %p on %s:%s's datapen", data,
        GST_DEBUG_PAD_NAME (peer));
    GST_PAD_DATAPEN (peer) = g_list_append (GST_PAD_DATALIST (peer), data);
    schedule_group (group);
  }
#else
  GST_LOG ("queueing data %p on %s:%s's datapen", data,
      GST_DEBUG_PAD_NAME (peer));
  GST_PAD_DATAPEN (peer) = g_list_append (GST_PAD_DATALIST (peer), data);
  if (!(group->flags & GST_OPT_SCHEDULER_GROUP_RUNNING)) {
    GST_LOG ("adding group %p to runqueue", group);
    if (!g_list_find (osched->runqueue, group)) {
      ref_group (group);
      osched->runqueue = g_list_append (osched->runqueue, group);
    }
  }
#endif
  GST_OPT_UNLOCK (osched);

  GST_LOG ("%d datas left on %s:%s's datapen after chain handler",
      g_list_length (GST_PAD_DATALIST (peer)), GST_DEBUG_PAD_NAME (peer));
}

/* this function is called by a loop based element that performs a
 * pull on a sinkpad. We schedule the peer group until the datapen
 * is filled with the data so that this function can return */
static GstData *
gst_opt_scheduler_get_wrapper (GstPad * srcpad)
{
  GstData *data;
  GstOptSchedulerGroup *group;
  GstOptScheduler *osched;
  gboolean disabled;

  GST_LOG ("get handler for %" GST_PTR_FORMAT, srcpad);

  /* first try to grab a queued data */
  if (GST_PAD_DATALIST (srcpad)) {
    data = GST_PAD_DATALIST (srcpad)->data;
    GST_PAD_DATAPEN (srcpad) = g_list_remove (GST_PAD_DATALIST (srcpad), data);

    GST_LOG ("returning popped queued data %p", data);

    return data;
  }
  GST_LOG ("need to schedule the peer element");

  /* else we need to schedule the peer element */
  get_group (GST_PAD_PARENT (srcpad), &group);
  if (group == NULL) {
    /* wow, peer has no group */
    GST_LOG ("peer without group detected");
    //group_error_handler (group);
    return GST_DATA (gst_event_new (GST_EVENT_INTERRUPT));
  }
  osched = group->chain->sched;
  data = NULL;
  disabled = FALSE;

  GST_OPT_LOCK (osched);
  do {
    GST_LOG ("scheduling upstream group %p to fill datapen", group);
#ifdef USE_COTHREADS
    schedule_group (group);
#else
    if (!(group->flags & GST_OPT_SCHEDULER_GROUP_RUNNING)) {
      ref_group (group);

      if (!g_list_find (osched->runqueue, group)) {
        ref_group (group);
        osched->runqueue = g_list_append (osched->runqueue, group);
      }

      GST_LOG ("recursing into scheduler group %p", group);
      gst_opt_scheduler_schedule_run_queue (osched, group);
      GST_LOG ("return from recurse group %p", group);

      /* if the other group was disabled we might have to break out of the loop */
      disabled = GST_OPT_SCHEDULER_GROUP_IS_DISABLED (group);
      group = unref_group (group);
      /* group is gone */
      if (group == NULL) {
        /* if the group was gone we also might have to break out of the loop */
        disabled = TRUE;
      }
    } else {
      /* in this case, the group was running and we wanted to swtich to it,
       * this is not allowed in the optimal scheduler (yet) */
      g_warning ("deadlock detected, disabling group %p", group);
      group_error_handler (group);
      data = GST_DATA (gst_event_new (GST_EVENT_INTERRUPT));
      goto done;
    }
#endif
    /* if the scheduler interrupted, make sure we send an INTERRUPTED event
     * to the loop based element */
    if (osched->state == GST_OPT_SCHEDULER_STATE_INTERRUPTED) {
      GST_INFO ("scheduler interrupted, return interrupt event");
      data = GST_DATA (gst_event_new (GST_EVENT_INTERRUPT));
    } else {
      if (GST_PAD_DATALIST (srcpad)) {
        data = GST_PAD_DATALIST (srcpad)->data;
        GST_PAD_DATAPEN (srcpad) =
            g_list_remove (GST_PAD_DATALIST (srcpad), data);
      } else if (disabled) {
        /* no data in queue and peer group was disabled */
        data = GST_DATA (gst_event_new (GST_EVENT_INTERRUPT));
      }
    }
  }
  while (data == NULL);

#ifndef USE_COTHREADS
done:
#endif
  GST_OPT_UNLOCK (osched);

  GST_LOG ("get handler, returning data %p, queue length %d",
      data, g_list_length (GST_PAD_DATALIST (srcpad)));

  return data;
}

static void
pad_clear_queued (GstPad * srcpad, gpointer user_data)
{
  GList *datalist = GST_PAD_DATALIST (srcpad);

  if (datalist) {
    GST_LOG ("need to clear some datas");
    g_list_foreach (datalist, (GFunc) gst_data_unref, NULL);
    g_list_free (datalist);
    GST_PAD_DATAPEN (srcpad) = NULL;
  }
}

static gboolean
gst_opt_scheduler_event_wrapper (GstPad * srcpad, GstEvent * event)
{
  gboolean flush;

  GST_DEBUG ("intercepting event type %d on pad %s:%s",
      GST_EVENT_TYPE (event), GST_DEBUG_PAD_NAME (srcpad));

  /* figure out if this is a flush event */
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH:
      flush = TRUE;
      break;
    case GST_EVENT_SEEK:
    case GST_EVENT_SEEK_SEGMENT:
      flush = GST_EVENT_SEEK_FLAGS (event) & GST_SEEK_FLAG_FLUSH;
      break;
    default:
      flush = FALSE;
      break;
  }

  if (flush) {
    GST_LOG ("event triggers a flush");

    pad_clear_queued (srcpad, NULL);
  }
  return GST_RPAD_EVENTFUNC (srcpad) (srcpad, event);
}

static GstElementStateReturn
gst_opt_scheduler_state_transition (GstScheduler * sched, GstElement * element,
    gint transition)
{
  GstOptSchedulerGroup *group;
  GstElementStateReturn res = GST_STATE_SUCCESS;

  GST_DEBUG ("element \"%s\" state change (%04x)",
      GST_ELEMENT_NAME (element) ? GST_ELEMENT_NAME (element) : "(null)",
      transition);

  GST_OPT_LOCK (sched);
  /* we check the state of the managing pipeline here */
  if (GST_IS_BIN (element)) {
    if (GST_SCHEDULER_PARENT (sched) == element) {
      GST_LOG ("parent \"%s\" changed state",
          GST_ELEMENT_NAME (element) ? GST_ELEMENT_NAME (element) : "(null)");

      switch (transition) {
        case GST_STATE_PLAYING_TO_PAUSED:
          GST_INFO ("setting scheduler state to stopped");
          GST_SCHEDULER_STATE (sched) = GST_SCHEDULER_STATE_STOPPED;
          break;
        case GST_STATE_PAUSED_TO_PLAYING:
          GST_INFO ("setting scheduler state to running");
          GST_SCHEDULER_STATE (sched) = GST_SCHEDULER_STATE_RUNNING;
          break;
        default:
          GST_LOG ("no interesting state change, doing nothing");
      }
    }
    goto done;
  }

  /* we don't care about decoupled elements after this */
  if (GST_ELEMENT_IS_DECOUPLED (element)) {
    res = GST_STATE_SUCCESS;
    goto done;
  }

  /* get the group of the element */
  group = GST_ELEMENT_SCHED_GROUP (element);

  switch (transition) {
    case GST_STATE_PAUSED_TO_PLAYING:
      /* an element without a group has to be an unlinked src, sink
       * filter element */
      if (!group) {
        GST_INFO ("element \"%s\" has no group", GST_ELEMENT_NAME (element));
      }
      /* else construct the scheduling context of this group and enable it */
      else {
        group_element_set_enabled (group, element, TRUE);
      }
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      /* if the element still has a group, we disable it */
      if (group)
        group_element_set_enabled (group, element, FALSE);
      break;
    case GST_STATE_PAUSED_TO_READY:
    {
      GList *pads = (GList *) gst_element_get_pad_list (element);

      g_list_foreach (pads, (GFunc) pad_clear_queued, NULL);
      break;
    }
    default:
      break;
  }

  //gst_scheduler_show (sched);

done:
  GST_OPT_UNLOCK (sched);

  return res;
}

static void
gst_opt_scheduler_scheduling_change (GstScheduler * sched, GstElement * element)
{
  g_warning ("scheduling change, implement me");
}

static void
get_group (GstElement * element, GstOptSchedulerGroup ** group)
{
  GstOptSchedulerCtx *ctx;

  ctx = GST_ELEMENT_SCHED_CONTEXT (element);
  if (ctx)
    *group = ctx->group;
  else
    *group = NULL;
}

/*
 * the idea is to put the two elements into the same group. 
 * - When no element is inside a group, we create a new group and add 
 *   the elements to it. 
 * - When one of the elements has a group, add the other element to 
 *   that group
 * - if both of the elements have a group, we merge the groups, which
 *   will also merge the chains.
 * Group links must be managed by the caller.
 */
static GstOptSchedulerGroup *
group_elements (GstOptScheduler * osched, GstElement * element1,
    GstElement * element2, GstOptSchedulerGroupType type)
{
  GstOptSchedulerGroup *group1, *group2, *group = NULL;

  get_group (element1, &group1);
  get_group (element2, &group2);

  /* none of the elements is added to a group, create a new group
   * and chain to add the elements to */
  if (!group1 && !group2) {
    GstOptSchedulerChain *chain;

    GST_DEBUG ("creating new group to hold \"%s\" and \"%s\"",
        GST_ELEMENT_NAME (element1), GST_ELEMENT_NAME (element2));

    chain = create_chain (osched);
    group = create_group (chain, element1, type);
    add_to_group (group, element2, TRUE);
  }
  /* the first element has a group */
  else if (group1) {
    GST_DEBUG ("adding \"%s\" to \"%s\"'s group",
        GST_ELEMENT_NAME (element2), GST_ELEMENT_NAME (element1));

    /* the second element also has a group, merge */
    if (group2)
      merge_groups (group1, group2);
    /* the second element has no group, add it to the group
     * of the first element */
    else
      add_to_group (group1, element2, TRUE);

    group = group1;
  }
  /* element1 has no group, element2 does. Add element1 to the
   * group of element2 */
  else {
    GST_DEBUG ("adding \"%s\" to \"%s\"'s group",
        GST_ELEMENT_NAME (element1), GST_ELEMENT_NAME (element2));
    add_to_group (group2, element1, TRUE);
    group = group2;
  }
  return group;
}

/*
 * increment link counts between groups -- it's important that src is actually
 * the src group, so we can introspect the topology later
 */
static void
group_inc_link (GstOptSchedulerGroup * src, GstOptSchedulerGroup * sink)
{
  GSList *links = src->group_links;
  gboolean done = FALSE;
  GstOptSchedulerGroupLink *link;

  /* first try to find a previous link */
  while (links && !done) {
    link = (GstOptSchedulerGroupLink *) links->data;
    links = g_slist_next (links);

    if (IS_GROUP_LINK (link, src, sink)) {
      /* we found a link to this group, increment the link count */
      link->count++;
      GST_LOG ("incremented group link count between %p and %p to %d",
          src, sink, link->count);
      done = TRUE;
    }
  }
  if (!done) {
    /* no link was found, create a new one */
    link = g_new0 (GstOptSchedulerGroupLink, 1);

    link->src = src;
    link->sink = sink;
    link->count = 1;

    src->group_links = g_slist_prepend (src->group_links, link);
    sink->group_links = g_slist_prepend (sink->group_links, link);

    src->sched->live_links++;

    GST_DEBUG ("added group link between %p and %p, %d live links now",
        src, sink, src->sched->live_links);
  }
}

/*
 * decrement link counts between groups, returns TRUE if the link count reaches
 * 0 -- note that the groups are not necessarily ordered as (src, sink) like
 * inc_link requires
 */
static gboolean
group_dec_link (GstOptSchedulerGroup * group1, GstOptSchedulerGroup * group2)
{
  GSList *links = group1->group_links;
  gboolean res = FALSE;
  GstOptSchedulerGroupLink *link;

  while (links) {
    link = (GstOptSchedulerGroupLink *) links->data;
    links = g_slist_next (links);

    if (IS_GROUP_LINK (link, group1, group2)) {
      g_assert (link->count > 0);
      link->count--;
      GST_LOG ("link count between %p and %p is now %d",
          group1, group2, link->count);
      if (link->count == 0) {
        GstOptSchedulerGroup *iso_group = NULL;

        group1->group_links = g_slist_remove (group1->group_links, link);
        group2->group_links = g_slist_remove (group2->group_links, link);
        group1->sched->live_links--;

        GST_LOG ("%d live links now", group1->sched->live_links);

        g_free (link);
        GST_DEBUG ("removed group link between %p and %p", group1, group2);
        if (group1->group_links == NULL) {
          /* group1 has no more links with other groups */
          iso_group = group1;
        } else if (group2->group_links == NULL) {
          /* group2 has no more links with other groups */
          iso_group = group2;
        }
        if (iso_group) {
          GstOptSchedulerChain *chain;

          GST_DEBUG ("group %p has become isolated, moving to new chain",
              iso_group);

          chain = create_chain (iso_group->chain->sched);
          remove_from_chain (iso_group->chain, iso_group);
          add_to_chain (chain, iso_group);
        }
        res = TRUE;
      }
      break;
    }
  }
  return res;
}


typedef enum
{
  GST_OPT_INVALID,
  GST_OPT_GET_TO_CHAIN,
  GST_OPT_LOOP_TO_CHAIN,
  GST_OPT_GET_TO_LOOP,
  GST_OPT_CHAIN_TO_CHAIN,
  GST_OPT_CHAIN_TO_LOOP,
  GST_OPT_LOOP_TO_LOOP
}
LinkType;

/*
 * Entry points for this scheduler.
 */
static void
gst_opt_scheduler_setup (GstScheduler * sched)
{
#ifdef USE_COTHREADS
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);

  /* first create thread context */
  if (osched->context == NULL) {
    GST_DEBUG ("initializing cothread context");
    osched->context = do_cothread_context_init ();
  }
#endif
}

static void
gst_opt_scheduler_reset (GstScheduler * sched)
{
#ifdef USE_COTHREADS
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  GSList *chains = osched->chains;

  while (chains) {
    GstOptSchedulerChain *chain = (GstOptSchedulerChain *) chains->data;
    GSList *groups = chain->groups;

    while (groups) {
      GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) groups->data;

      destroy_group_scheduler (group);
      groups = groups->next;
    }
    chains = chains->next;
  }

  if (osched->context) {
    do_cothread_context_destroy (osched->context);
    osched->context = NULL;
  }
#endif
}

static void
gst_opt_scheduler_add_element (GstScheduler * sched, GstElement * element)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  GstOptSchedulerCtx *ctx;
  const GList *pads;

  GST_DEBUG_OBJECT (sched, "adding element \"%s\"", GST_OBJECT_NAME (element));

  /* decoupled elements are not added to the scheduler lists */
  if (GST_ELEMENT_IS_DECOUPLED (element))
    return;

  ctx = g_new0 (GstOptSchedulerCtx, 1);
  GST_ELEMENT (element)->sched_private = ctx;
  ctx->flags = GST_OPT_SCHEDULER_CTX_DISABLED;

  /* set event handler on all pads here so events work unconnected too;
   * in _link, it can be overruled if need be */
  /* FIXME: we should also do this when new pads on the element are created;
     but there are no hooks, so we do it again in _link */
  pads = gst_element_get_pad_list (element);
  while (pads) {
    GstPad *pad = GST_PAD (pads->data);

    pads = g_list_next (pads);

    if (!GST_IS_REAL_PAD (pad))
      continue;
    GST_RPAD_EVENTHANDLER (pad) = GST_RPAD_EVENTFUNC (pad);
  }

  /* loop based elements *always* end up in their own group. It can eventually
   * be merged with another group when a link is made */
  if (element->loopfunc) {
    GstOptSchedulerGroup *group;
    GstOptSchedulerChain *chain;

    GST_OPT_LOCK (sched);
    chain = create_chain (osched);

    group = create_group (chain, element, GST_OPT_SCHEDULER_GROUP_LOOP);
    group->entry = element;
    GST_OPT_UNLOCK (sched);

    GST_LOG ("added element \"%s\" as loop based entry",
        GST_ELEMENT_NAME (element));
  }
}

static void
gst_opt_scheduler_remove_element (GstScheduler * sched, GstElement * element)
{
  GstOptSchedulerGroup *group;

  GST_DEBUG_OBJECT (sched, "removing element \"%s\"",
      GST_OBJECT_NAME (element));

  GST_OPT_LOCK (sched);
  /* decoupled elements are not added to the scheduler lists and should therefore
   * not be removed */
  if (GST_ELEMENT_IS_DECOUPLED (element)) {
    remove_decoupled (sched, element);
    goto done;
  }

  /* the element is guaranteed to live in it's own group/chain now */
  get_group (element, &group);
  if (group) {
    remove_from_group (group, element);
  }

  g_free (GST_ELEMENT (element)->sched_private);
  GST_ELEMENT (element)->sched_private = NULL;

done:
  GST_OPT_UNLOCK (sched);
}

static gboolean
gst_opt_scheduler_yield (GstScheduler * sched, GstElement * element)
{
#ifdef USE_COTHREADS
  /* yield hands control to the main cothread context if the requesting 
   * element is the entry point of the group */
  GstOptSchedulerGroup *group;

  get_group (element, &group);
  if (group && group->entry == element)
    do_cothread_switch (do_cothread_get_main (((GstOptScheduler *) sched)->
            context));

  return FALSE;
#else
  g_warning ("element %s performs a yield, please fix the element",
      GST_ELEMENT_NAME (element));
  return TRUE;
#endif
}

static gboolean
gst_opt_scheduler_interrupt (GstScheduler * sched, GstElement * element)
{
  GST_INFO ("interrupt from \"%s\"", GST_OBJECT_NAME (element));

#ifdef USE_COTHREADS
  do_cothread_switch (do_cothread_get_main (((GstOptScheduler *) sched)->
          context));
  return FALSE;
#else
  {
    GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);

    GST_OPT_LOCK (sched);
    GST_INFO ("scheduler set interrupted state");
    osched->state = GST_OPT_SCHEDULER_STATE_INTERRUPTED;
    GST_OPT_UNLOCK (sched);
  }
  return TRUE;
#endif
}

static void
gst_opt_scheduler_error (GstScheduler * sched, GstElement * element)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  GstOptSchedulerGroup *group;

  GST_OPT_LOCK (sched);
  get_group (element, &group);
  if (group)
    group_error_handler (group);

  osched->state = GST_OPT_SCHEDULER_STATE_ERROR;
  GST_OPT_UNLOCK (sched);
}

/* link pads, merge groups and chains */
static void
gst_opt_scheduler_pad_link (GstScheduler * sched, GstPad * srcpad,
    GstPad * sinkpad)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  LinkType type = GST_OPT_INVALID;
  GstElement *src_element, *sink_element;

  GST_INFO ("scheduling link between %s:%s and %s:%s",
      GST_DEBUG_PAD_NAME (srcpad), GST_DEBUG_PAD_NAME (sinkpad));

  src_element = GST_PAD_PARENT (srcpad);
  sink_element = GST_PAD_PARENT (sinkpad);

  GST_OPT_LOCK (sched);
  /* first we need to figure out what type of link we're dealing
   * with */
  if (src_element->loopfunc && sink_element->loopfunc)
    type = GST_OPT_LOOP_TO_LOOP;
  else {
    if (src_element->loopfunc) {
      if (GST_RPAD_CHAINFUNC (sinkpad))
        type = GST_OPT_LOOP_TO_CHAIN;
    } else if (sink_element->loopfunc) {
      if (GST_RPAD_GETFUNC (srcpad)) {
        type = GST_OPT_GET_TO_LOOP;
        /* this could be tricky, the get based source could 
         * already be part of a loop based group in another pad,
         * we assert on that for now */
        if (GST_ELEMENT_SCHED_CONTEXT (src_element) != NULL &&
            GST_ELEMENT_SCHED_GROUP (src_element) != NULL) {
          GstOptSchedulerGroup *group = GST_ELEMENT_SCHED_GROUP (src_element);

          /* if the loop based element is the entry point we're ok, if it
           * isn't then we have multiple loop based elements in this group */
          if (group->entry != sink_element) {
            g_error
                ("internal error: cannot schedule get to loop in multi-loop based group");
            goto done;
          }
        }
      } else
        type = GST_OPT_CHAIN_TO_LOOP;
    } else {
      if (GST_RPAD_GETFUNC (srcpad) && GST_RPAD_CHAINFUNC (sinkpad)) {
        type = GST_OPT_GET_TO_CHAIN;
        /* the get based source could already be part of a loop 
         * based group in another pad, we assert on that for now */
        if (GST_ELEMENT_SCHED_CONTEXT (src_element) != NULL &&
            GST_ELEMENT_SCHED_GROUP (src_element) != NULL) {
          GstOptSchedulerGroup *group = GST_ELEMENT_SCHED_GROUP (src_element);

          /* if the get based element is the entry point we're ok, if it
           * isn't then we have a mixed loop/chain based group */
          if (group->entry != src_element) {
            g_error ("internal error: cannot schedule get to chain "
                "with mixed loop/chain based group");
            goto done;
          }
        }
      } else
        type = GST_OPT_CHAIN_TO_CHAIN;
    }
  }

  /* since we can't set event handlers on pad creation after addition, it is
   * best we set all of them again to the default before linking */
  GST_RPAD_EVENTHANDLER (srcpad) = GST_RPAD_EVENTFUNC (srcpad);
  GST_RPAD_EVENTHANDLER (sinkpad) = GST_RPAD_EVENTFUNC (sinkpad);

  /* for each link type, perform specific actions */
  switch (type) {
    case GST_OPT_GET_TO_CHAIN:
    {
      GstOptSchedulerGroup *group = NULL;

      GST_LOG ("get to chain based link");

      /* setup get/chain handlers */
      GST_RPAD_GETHANDLER (srcpad) = get_invalid_call;
      GST_RPAD_CHAINHANDLER (sinkpad) = gst_pad_call_chain_function;

      /* the two elements should be put into the same group, 
       * this also means that they are in the same chain automatically */
      group = group_elements (osched, src_element, sink_element,
          GST_OPT_SCHEDULER_GROUP_GET);

      /* if there is not yet an entry in the group, select the source
       * element as the entry point and mark the group as a get based
       * group */
      if (!group->entry) {
        group->entry = src_element;
        group->type = GST_OPT_SCHEDULER_GROUP_GET;

        GST_DEBUG ("setting \"%s\" as entry point of _get-based group %p",
            GST_ELEMENT_NAME (src_element), group);

        setup_group_scheduler (osched, group);
      }
      break;
    }
    case GST_OPT_LOOP_TO_CHAIN:
    case GST_OPT_CHAIN_TO_CHAIN:
      GST_LOG ("loop/chain to chain based link");

      GST_RPAD_GETHANDLER (srcpad) = get_invalid_call;
      GST_RPAD_CHAINHANDLER (sinkpad) = gst_pad_call_chain_function;

      /* the two elements should be put into the same group, this also means
       * that they are in the same chain automatically, in case of a loop-based
       * src_element, there will be a group for src_element and sink_element
       * will be added to it. In the case a new group is created, we can't know
       * the type so we pass UNKNOWN as an arg */
      group_elements (osched, src_element, sink_element,
          GST_OPT_SCHEDULER_GROUP_UNKNOWN);
      break;
    case GST_OPT_GET_TO_LOOP:
      GST_LOG ("get to loop based link");

      GST_RPAD_GETHANDLER (srcpad) = gst_pad_call_get_function;
      GST_RPAD_CHAINHANDLER (sinkpad) = chain_invalid_call;

      /* the two elements should be put into the same group, this also means
       * that they are in the same chain automatically, sink_element is
       * loop-based so it already has a group where src_element will be added
       * to */
      group_elements (osched, src_element, sink_element,
          GST_OPT_SCHEDULER_GROUP_LOOP);
      break;
    case GST_OPT_CHAIN_TO_LOOP:
    case GST_OPT_LOOP_TO_LOOP:
    {
      GstOptSchedulerGroup *group1, *group2;

      GST_LOG ("chain/loop to loop based link");

      GST_RPAD_GETHANDLER (srcpad) = gst_opt_scheduler_get_wrapper;
      GST_RPAD_CHAINHANDLER (sinkpad) = gst_opt_scheduler_loop_wrapper;
      /* events on the srcpad have to be intercepted as we might need to
       * flush the buffer lists, so override the given eventfunc */
      GST_RPAD_EVENTHANDLER (srcpad) = gst_opt_scheduler_event_wrapper;

      group1 = GST_ELEMENT_SCHED_GROUP (src_element);
      group2 = GST_ELEMENT_SCHED_GROUP (sink_element);

      g_assert (group2 != NULL);

      /* group2 is guaranteed to exist as it contains a loop-based element.
       * group1 only exists if src_element is linked to some other element */
      if (!group1) {
        /* create a new group for src_element as it cannot be merged into another group
         * here. we create the group in the same chain as the loop-based element. note
         * that creating a new group will not increment the links with other groups */
        GST_DEBUG ("creating new group for element %s",
            GST_ELEMENT_NAME (src_element));
        group1 =
            create_group (group2->chain, src_element,
            GST_OPT_SCHEDULER_GROUP_LOOP);
      } else {
        /* both elements are already in a group, make sure they are added to
         * the same chain */
        merge_chains (group1->chain, group2->chain);
      }
      /* increment the group link counters */
      group_inc_link (group1, group2);
      break;
    }
    case GST_OPT_INVALID:
      g_error ("(internal error) invalid element link, what are you doing?");
      break;
  }
done:
  GST_OPT_UNLOCK (sched);
}

static void
group_elements_set_visited (GstOptSchedulerGroup * group, gboolean visited)
{
  GSList *elements;

  for (elements = group->elements; elements; elements = g_slist_next (elements)) {
    GstElement *element = GST_ELEMENT (elements->data);

    if (visited) {
      GST_ELEMENT_SET_VISITED (element);
    } else {
      GST_ELEMENT_UNSET_VISITED (element);
    }
  }
  /* don't forget to set any decoupled entry points that are not accounted for in the
   * element list (since they belong to two groups). */
  if (group->entry) {
    if (visited) {
      GST_ELEMENT_SET_VISITED (group->entry);
    } else {
      GST_ELEMENT_UNSET_VISITED (group->entry);
    }
  }
}

static GList *
element_get_reachables_func (GstElement * element, GstOptSchedulerGroup * group,
    GstPad * brokenpad)
{
  GList *result = NULL;
  const GList *pads;

  /* if no element or element not in group or been there, return NULL */
  if (element == NULL || !group_has_element (group, element) ||
      GST_ELEMENT_IS_VISITED (element))
    return NULL;

  GST_ELEMENT_SET_VISITED (element);

  result = g_list_prepend (result, element);

  pads = gst_element_get_pad_list (element);
  while (pads) {
    GstPad *pad = GST_PAD (pads->data);
    GstPad *peer;

    pads = g_list_next (pads);

    /* we only operate on real pads and on the pad that is not broken */
    if (!GST_IS_REAL_PAD (pad) || pad == brokenpad)
      continue;

    peer = GST_PAD_PEER (pad);
    if (!GST_IS_REAL_PAD (peer) || peer == brokenpad)
      continue;

    if (peer) {
      GstElement *parent;
      GList *res;

      parent = GST_PAD_PARENT (peer);

      res = element_get_reachables_func (parent, group, brokenpad);

      result = g_list_concat (result, res);
    }
  }

  return result;
}

static GList *
element_get_reachables (GstElement * element, GstOptSchedulerGroup * group,
    GstPad * brokenpad)
{
  GList *result;

  /* reset visited flags */
  group_elements_set_visited (group, FALSE);

  result = element_get_reachables_func (element, group, brokenpad);

  /* and reset visited flags again */
  group_elements_set_visited (group, FALSE);

  return result;
}

/* 
 * checks if a target group is still reachable from the group without taking the broken
 * group link into account.
 */
static gboolean
group_can_reach_group (GstOptSchedulerGroup * group,
    GstOptSchedulerGroup * target)
{
  gboolean reachable = FALSE;
  const GSList *links = group->group_links;

  GST_LOG ("checking if group %p can reach %p", group, target);

  /* seems like we found the target element */
  if (group == target) {
    GST_LOG ("found way to reach %p", target);
    return TRUE;
  }

  /* if the group is marked as visited, we don't need to check here */
  if (GST_OPT_SCHEDULER_GROUP_IS_FLAG_SET (group,
          GST_OPT_SCHEDULER_GROUP_VISITED)) {
    GST_LOG ("already visited %p", group);
    return FALSE;
  }

  /* mark group as visited */
  GST_OPT_SCHEDULER_GROUP_SET_FLAG (group, GST_OPT_SCHEDULER_GROUP_VISITED);

  while (links && !reachable) {
    GstOptSchedulerGroupLink *link = (GstOptSchedulerGroupLink *) links->data;
    GstOptSchedulerGroup *other;

    links = g_slist_next (links);

    /* find other group in this link */
    other = OTHER_GROUP_LINK (link, group);

    GST_LOG ("found link from %p to %p, count %d", group, other, link->count);

    /* check if we can reach the target recursively */
    reachable = group_can_reach_group (other, target);
  }
  /* unset the visited flag, note that this is not optimal as we might be checking
   * groups several times when they are reachable with a loop. An alternative would be
   * to not clear the group flag at this stage but clear all flags in the chain when
   * all groups are checked. */
  GST_OPT_SCHEDULER_GROUP_UNSET_FLAG (group, GST_OPT_SCHEDULER_GROUP_VISITED);

  GST_LOG ("leaving group %p with %s", group, (reachable ? "TRUE" : "FALSE"));

  return reachable;
}

/*
 * Go through all the pads of the given element and decrement the links that
 * this group has with the group of the peer element.  This function is mainly used
 * to update the group connections before we remove the element from the group.
 */
static void
group_dec_links_for_element (GstOptSchedulerGroup * group, GstElement * element)
{
  GList *l;
  GstPad *pad;
  GstOptSchedulerGroup *peer_group;

  for (l = GST_ELEMENT_PADS (element); l; l = l->next) {
    pad = (GstPad *) l->data;
    if (GST_IS_REAL_PAD (pad) && GST_PAD_PEER (pad)) {
      get_group (GST_PAD_PARENT (GST_PAD_PEER (pad)), &peer_group);
      if (peer_group && peer_group != group)
        group_dec_link (group, peer_group);
    }
  }
}

/*
 * Go through all the pads of the given element and increment the links that
 * this group has with the group of the peer element.  This function is mainly used
 * to update the group connections before we add the element to the group.
 */
static void
group_inc_links_for_element (GstOptSchedulerGroup * group, GstElement * element)
{
  GList *l;
  GstPad *pad;
  GstOptSchedulerGroup *peer_group;

  GST_DEBUG ("group %p, element %s ", group, gst_element_get_name (element));

  for (l = GST_ELEMENT_PADS (element); l; l = l->next) {
    pad = (GstPad *) l->data;
    if (GST_IS_REAL_PAD (pad) && GST_PAD_PEER (pad)) {
      get_group (GST_PAD_PARENT (GST_PAD_PEER (pad)), &peer_group);
      if (peer_group && peer_group != group)
        if (GST_PAD_DIRECTION (pad) == GST_PAD_SRC)
          group_inc_link (group, peer_group);
        else
          group_inc_link (peer_group, group);
    }
  }
}

static void
debug_element (GstElement * element, GstOptScheduler * osched)
{
  GST_LOG ("element %s", gst_element_get_name (element));
}

/* move this group in the chain of the groups it has links with */
static void
rechain_group (GstOptSchedulerGroup * group)
{
  GstOptSchedulerChain *chain = NULL;
  GSList *links;

  GST_LOG ("checking if this group needs rechaining");

  /* follow all links */
  for (links = group->group_links; links; links = g_slist_next (links)) {
    GstOptSchedulerGroupLink *link = (GstOptSchedulerGroupLink *) links->data;
    GstOptSchedulerGroup *other;

    other = OTHER_GROUP_LINK (link, group);
    GST_LOG ("found link with other group %p with chain %p", other,
        other->chain);

    /* first time, take chain */
    if (chain == NULL) {
      chain = other->chain;
    }
    /* second time, chain should be the same */
    else if (other->chain != chain) {
      g_warning ("(internal error): chain inconsistency");
    }
  }
  if (!chain) {
    GST_LOG ("no new chain found, not rechaining");
  } else if (chain != group->chain) {
    GST_LOG ("need to move group %p to chain %p", group, chain);
    /* remove from old chain */
    remove_from_chain (group->chain, group);
    /* and move to new chain */
    add_to_chain (chain, group);
  } else {
    GST_LOG ("group %p is in correct chain %p", group, chain);
  }
}

/* make sure that the group does not contain only single element.
 * Only loop-based groups can contain a single element. */
static GstOptSchedulerGroup *
normalize_group (GstOptSchedulerGroup * group)
{
  gint num;
  gboolean have_decoupled = FALSE;

  if (group == NULL)
    return NULL;

  num = group->num_elements;
  /* decoupled elements are not added to the group but are
   * added as an entry */
  if (group->entry && GST_ELEMENT_IS_DECOUPLED (group->entry)) {
    num++;
    have_decoupled = TRUE;
  }

  if (num == 1 && group->type != GST_OPT_SCHEDULER_GROUP_LOOP) {
    GST_LOG ("removing last element from group %p", group);
    if (have_decoupled) {
      group->entry = NULL;
      if (group->chain) {
        GST_LOG ("removing group %p from its chain", group);
        chain_group_set_enabled (group->chain, group, FALSE);
        remove_from_chain (group->chain, group);
      }
      group = unref_group (group);
    } else {
      group = remove_from_group (group, GST_ELEMENT (group->elements->data));
    }
  }
  return group;
}

/* migrate the element and all connected elements to a new group without looking at
 * the brokenpad */
static GstOptSchedulerGroup *
group_migrate_connected (GstOptScheduler * osched, GstElement * element,
    GstOptSchedulerGroup * group, GstPad * brokenpad)
{
  GList *connected, *c;
  GstOptSchedulerGroup *new_group = NULL, *tst;
  GstOptSchedulerChain *chain;
  gint len;

  if (GST_ELEMENT_IS_DECOUPLED (element)) {
    GST_LOG ("element is decoupled and thus not in the group");
    /* the element is decoupled and is therefore not in the group */
    return NULL;
  }

  get_group (element, &tst);
  if (tst == NULL) {
    GST_LOG ("element has no group, not interesting");
    return NULL;
  }

  GST_LOG ("migrate connected elements to new group");
  connected = element_get_reachables (element, group, brokenpad);
  GST_LOG ("elements to move to new group:");
  g_list_foreach (connected, (GFunc) debug_element, NULL);

  len = g_list_length (connected);

  if (len == 0) {
    g_warning ("(internal error) found lost element %s",
        gst_element_get_name (element));
    return NULL;
  } else if (len == 1) {
    group = remove_from_group (group, GST_ELEMENT (connected->data));
    GST_LOG
        ("not migrating to new group as the group would only contain 1 element");
    g_list_free (connected);
    GST_LOG ("new group is old group now");
    new_group = group;
  } else {
    /* we create a new chain to hold the new group */
    chain = create_chain (osched);

    for (c = connected; c; c = g_list_next (c)) {
      GstElement *element = GST_ELEMENT (c->data);

      group = remove_from_group (group, element);
      if (new_group == NULL) {
        new_group =
            create_group (chain, element, GST_OPT_SCHEDULER_GROUP_UNKNOWN);
      } else {
        add_to_group (new_group, element, TRUE);
      }
    }
    g_list_free (connected);

    /* remove last element from the group if any. Make sure not to remove
     * the loop based entry point of a group as this always needs one group */
    if (group != NULL) {
      group = normalize_group (group);
    }
  }

  if (new_group != NULL) {
    new_group = normalize_group (new_group);
    if (new_group == NULL)
      return NULL;
    /* at this point the new group lives in its own chain but might
     * have to be merged with another chain, this happens when the new
     * group has a link with another group in another chain */
    rechain_group (new_group);
  }


  return new_group;
}

static void
gst_opt_scheduler_pad_unlink (GstScheduler * sched,
    GstPad * srcpad, GstPad * sinkpad)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  GstElement *src_element, *sink_element;
  GstOptSchedulerGroup *group1, *group2;

  GST_INFO ("unscheduling link between %s:%s and %s:%s",
      GST_DEBUG_PAD_NAME (srcpad), GST_DEBUG_PAD_NAME (sinkpad));

  src_element = GST_PAD_PARENT (srcpad);
  sink_element = GST_PAD_PARENT (sinkpad);

  GST_OPT_LOCK (sched);
  get_group (src_element, &group1);
  get_group (sink_element, &group2);

  /* for decoupled elements (that are never put into a group) we use the
   * group of the peer element for the remainder of the algorithm */
  if (GST_ELEMENT_IS_DECOUPLED (src_element)) {
    group1 = group2;
  }
  if (GST_ELEMENT_IS_DECOUPLED (sink_element)) {
    group2 = group1;
  }

  /* if one the elements has no group (anymore) we don't really care 
   * about the link */
  if (!group1 || !group2) {
    GST_LOG
        ("one (or both) of the elements is not in a group, not interesting");
    goto done;
  }

  /* easy part, groups are different */
  if (group1 != group2) {
    gboolean zero;

    GST_LOG ("elements are in different groups");

    /* we can remove the links between the groups now */
    zero = group_dec_link (group1, group2);

    /* if the groups are not directly connected anymore, we have to perform a
     * recursive check to see if they are really unlinked */
    if (zero) {
      gboolean still_link;
      GstOptSchedulerChain *chain;

      /* see if group1 and group2 are still connected in any indirect way */
      still_link = group_can_reach_group (group1, group2);

      GST_DEBUG ("group %p %s reach group %p", group1,
          (still_link ? "can" : "can't"), group2);
      if (!still_link) {
        /* groups are really disconnected, migrate one group to a new chain */
        chain = create_chain (osched);
        chain_recursively_migrate_group (chain, group1);

        GST_DEBUG ("migrated group %p to new chain %p", group1, chain);
      }
    } else {
      GST_DEBUG ("group %p still has direct link with group %p", group1,
          group2);
    }
  }
  /* hard part, groups are equal */
  else {
    GstOptSchedulerGroup *group;

    /* since group1 == group2, it doesn't matter which group we take */
    group = group1;

    GST_LOG ("elements are in the same group %p", group);

    if (group->entry == NULL) {
      /* it doesn't really matter, we just have to make sure that both 
       * elements end up in another group if they are not connected */
      GST_LOG ("group %p has no entry, moving source element to new group",
          group);
      group_migrate_connected (osched, src_element, group, srcpad);
    } else {
      GList *reachables;

      GST_LOG ("group %p has entry %p", group, group->entry);

      /* get of a list of all elements that are still managed by the old
       * entry element */
      reachables = element_get_reachables (group->entry, group, srcpad);
      GST_LOG ("elements still reachable from the entry:");
      g_list_foreach (reachables, (GFunc) debug_element, sched);

      /* if the source is reachable from the entry, we can leave it in the group */
      if (g_list_find (reachables, src_element)) {
        GST_LOG
            ("source element still reachable from the entry, leaving in group");
      } else {
        GST_LOG
            ("source element not reachable from the entry, moving to new group");
        group_migrate_connected (osched, src_element, group, srcpad);
      }

      /* if the sink is reachable from the entry, we can leave it in the group */
      if (g_list_find (reachables, sink_element)) {
        GST_LOG
            ("sink element still reachable from the entry, leaving in group");
      } else {
        GST_LOG
            ("sink element not reachable from the entry, moving to new group");
        group_migrate_connected (osched, sink_element, group, srcpad);
      }
      g_list_free (reachables);
    }
    /* at this point the group can be freed and gone, so don't touch */
  }
done:
  GST_OPT_UNLOCK (sched);
}

/* a scheduler iteration is done by looping and scheduling the active chains */
static GstSchedulerState
gst_opt_scheduler_iterate (GstScheduler * sched)
{
  GstSchedulerState state = GST_SCHEDULER_STATE_STOPPED;
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  gint iterations;

  GST_OPT_LOCK (sched);
  iterations = osched->iterations;

  osched->state = GST_OPT_SCHEDULER_STATE_RUNNING;

  GST_DEBUG_OBJECT (sched, "iterating");

  while (iterations) {
    gboolean scheduled = FALSE;
    GSList *chains;

    /* we have to schedule each of the scheduler chains now. 
     * FIXME handle the case where the chains change during iterations. */
    chains = osched->chains;
    while (chains) {
      GstOptSchedulerChain *chain = (GstOptSchedulerChain *) chains->data;

      ref_chain (chain);
      /* if the chain is not disabled, schedule it */
      if (!GST_OPT_SCHEDULER_CHAIN_IS_DISABLED (chain)) {
        GST_LOG ("scheduling chain %p", chain);
        scheduled = schedule_chain (chain);
        GST_LOG ("scheduled chain %p", chain);
      } else {
        GST_LOG ("not scheduling disabled chain %p", chain);
      }

      /* don't schedule any more chains when in error */
      if (osched->state == GST_OPT_SCHEDULER_STATE_ERROR) {
        GST_ERROR_OBJECT (sched, "in error state");
        /* unref the chain here as we move out of the while loop */
        unref_chain (chain);
        break;
      } else if (osched->state == GST_OPT_SCHEDULER_STATE_INTERRUPTED) {
        GST_DEBUG_OBJECT (osched, "got interrupted, continue with next chain");
        osched->state = GST_OPT_SCHEDULER_STATE_RUNNING;
      }

      chains = g_slist_next (chains);
      unref_chain (chain);
    }

    /* at this point it's possible that the scheduler state is
     * in error, we then return an error */
    if (osched->state == GST_OPT_SCHEDULER_STATE_ERROR) {
      state = GST_SCHEDULER_STATE_ERROR;
      break;
    } else {
      /* if chains were scheduled, return our current state */
      if (scheduled)
        state = GST_SCHEDULER_STATE (sched);
      /* if no chains were scheduled, we say we are stopped */
      else {
        state = GST_SCHEDULER_STATE_STOPPED;
        break;
      }
    }
    if (iterations > 0)
      iterations--;
  }
  GST_OPT_UNLOCK (sched);

  return state;
}


static void
gst_opt_scheduler_show (GstScheduler * sched)
{
  GstOptScheduler *osched = GST_OPT_SCHEDULER (sched);
  GSList *chains;

  GST_OPT_LOCK (sched);

  g_print ("iterations:    %d\n", osched->iterations);
  g_print ("max recursion: %d\n", osched->max_recursion);

  chains = osched->chains;
  while (chains) {
    GstOptSchedulerChain *chain = (GstOptSchedulerChain *) chains->data;
    GSList *groups = chain->groups;

    chains = g_slist_next (chains);

    g_print ("+- chain %p: refcount %d, %d groups, %d enabled, flags %d\n",
        chain, chain->refcount, chain->num_groups, chain->num_enabled,
        chain->flags);

    while (groups) {
      GstOptSchedulerGroup *group = (GstOptSchedulerGroup *) groups->data;
      GSList *elements = group->elements;
      GSList *group_links = group->group_links;

      groups = g_slist_next (groups);

      g_print
          (" +- group %p: refcount %d, %d elements, %d enabled, flags %d, entry %s, %s\n",
          group, group->refcount, group->num_elements, group->num_enabled,
          group->flags,
          (group->entry ? GST_ELEMENT_NAME (group->entry) : "(none)"),
          (group->type ==
              GST_OPT_SCHEDULER_GROUP_GET ? "get-based" : "loop-based"));

      while (elements) {
        GstElement *element = (GstElement *) elements->data;

        elements = g_slist_next (elements);

        g_print ("  +- element %s\n", GST_ELEMENT_NAME (element));
      }
      while (group_links) {
        GstOptSchedulerGroupLink *link =
            (GstOptSchedulerGroupLink *) group_links->data;

        group_links = g_slist_next (group_links);

        g_print ("group link %p between %p and %p, count %d\n",
            link, link->src, link->sink, link->count);
      }
    }
  }
  GST_OPT_UNLOCK (sched);
}

static void
gst_opt_scheduler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOptScheduler *osched;

  g_return_if_fail (GST_IS_OPT_SCHEDULER (object));

  osched = GST_OPT_SCHEDULER (object);

  switch (prop_id) {
    case ARG_ITERATIONS:
      g_value_set_int (value, osched->iterations);
      break;
    case ARG_MAX_RECURSION:
      g_value_set_int (value, osched->max_recursion);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opt_scheduler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOptScheduler *osched;

  g_return_if_fail (GST_IS_OPT_SCHEDULER (object));

  osched = GST_OPT_SCHEDULER (object);

  switch (prop_id) {
    case ARG_ITERATIONS:
      osched->iterations = g_value_get_int (value);
      break;
    case ARG_MAX_RECURSION:
      osched->max_recursion = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
