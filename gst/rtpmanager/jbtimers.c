#include "jbtimers.h"

#include <gst/rtp/gstrtpbuffer.h>

#include <gst/gstmacros.h>

GST_DEBUG_CATEGORY_STATIC (jb_timers_debug);
#define GST_CAT_DEFAULT jb_timers_debug

enum
{
  // XXX
  LAST_SIGNAL
};

#define DEFAULT_RTX_DELAY_REORDER   0

enum
{
  PROP_0,
  PROP_RTX_DELAY_REORDER,
};



//static guint jb_timers_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (JBTimers, jb_timers, G_TYPE_OBJECT);

static inline gboolean
timer_is_initial_expected (TimerData *timer)
{
  return (timer->type == TIMER_TYPE_EXPECTED &&
      timer->timeout != TIMEOUT_IMMEDIATE && timer->num_rtx_retry == 0);
}

static inline gboolean
reorder_limit_enabled (JBTimers * jbtimers)
{
  return jbtimers->rtx_delay_reorder > 0;
}

static TimerData *
alloc_timer (void)
{
  return g_slice_new (TimerData);
}

static void
free_timer (gpointer timer)
{
  g_slice_free (TimerData, timer);
}

static TimerQueue *
timer_queue_new (void)
{
  TimerQueue *queue;

  queue = g_slice_new (TimerQueue);
  queue->timers = g_queue_new ();
  queue->hashtable = g_hash_table_new (NULL, NULL);

  return queue;
}

static void
timer_queue_free (TimerQueue * queue)
{
  if (!queue)
    return;

  g_hash_table_destroy (queue->hashtable);
  g_queue_free_full (queue->timers, g_free);
  g_slice_free (TimerQueue, queue);
}


/*
 * Public API
 */

TimerData *
jb_timers_get_next_timer (JBTimers * jbtimers)
{
  // XXX
}

TimerData *
jb_timers_find_timer (JBTimers * jbtimers, guint16 seqnum)
{
  TimerData *timer;

  // timer = XXX;

  if (timer->seqnum != seqnum) {
    /* This is possible iff. timer->num > 1. However, we only accept exact
     * matches.
     */
    return NULL;
  }

  return timer;
}

/* Check if packet with seqnum is already considered definitely lost by being
 * part of a "lost timer" for multiple packets
 */
gboolean
jb_timers_seqnum_is_already_lost (JBTimers * jbtimers, guint16 seqnum)
{
  TimerData *timer;

  // timer = XXX;

  return (timer->num > 1 && timer->type == TIMER_TYPE_LOST);
}

/* Time out (reschedule as immediate) the timer of every packet that we are
 * still expecting, where the seqnum precedes the current seqnum by more than
 * the max reorder distance, and for which we have not yet sent an RTX request.
 */
void
jb_timers_time_out_reordered (JBTimers * jbtimers, guint16 current_seqnum)
{
  if (!reorder_limit_enabled (jbtimers))
    return;

  // XXX
}

TimerData *
jb_timers_add_timer (JBTimers * jbtimers, TimerType type, guint16 seqnum,
    guint num, GstClockTime timeout, GstClockTime delay, GstClockTime duration)
{
  TimerData *timer;
  guint16 i;

  timer = alloc_timer ();



  return timer;
}

void
jb_timers_reschedule_timer (JBTimers * jbtimers, TimerData * timer,
    TimerType type, guint16 seqnum, GstClockTime timeout, GstClockTime delay,
    gboolean reset)
{

}

void
jb_timers_remove_timer (JBTimers * jbtimers, TimerData * timer)
{


  free_timer (timer);
}

void
jb_timers_remove_all_timers (JBTimers * jbtimers)
{

}

void
jb_timers_set_clock_and_base_time (JBTimers * jbtimers, GstClock *clock,
    GstClockTime base_time)
{

}

#if 0
void
jb_timers_set_upstream_peer_latency (JBTimers * jbtimers, GstClockTime latency)
{
  // XXX: acquire mutex

  jbtimers->peer_latency = latency;

  // XXX: release mutex
}
#endif


JBTimers *
jb_timers_new (gint rtx_delay_reorder)
{
  JBTimers *jbtimers;

  jbtimers = g_object_new (JB_TYPE_TIMERS,
      "rtx-delay-reorder", rtx_delay_reorder,
      NULL);

  jbtimers->timers = g_array_new (FALSE, TRUE, sizeof (TimerData));
  jbtimers->rtx_stats_timers = timer_queue_new ();

  return jbtimers;
}

/*
 * GObject API
 */

static void
jb_timers_finalize (GObject * object)
{
  JBTimers *jbtimers = JB_TIMERS (object);

  g_array_free (jbtimers->timers, TRUE);
  timer_queue_free (jbtimers->rtx_stats_timers);

  g_mutex_clear (&jbtimers->lock);
  g_cond_clear (&jbtimers->cond);

  G_OBJECT_CLASS (jb_timers_parent_class)->finalize (object);
}

static void
jb_timers_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  JBTimers *jbtimers = JB_TIMERS (object);

  switch (prop_id) {
    case PROP_RTX_DELAY_REORDER:
      // XXX: acquire mutex
      jbtimers->rtx_delay_reorder = g_value_get_int (value);
      // XXX: release mutex
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
jb_timers_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  JBTimers *jbtimers = JB_TIMERS (object);

  switch (prop_id) {
    case PROP_RTX_DELAY_REORDER:
      // XXX: acquire mutex
      g_value_set_int (value, jbtimers->rtx_delay_reorder);
      // XXX: release mutex
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
jb_timers_init (JBTimers * jbtimers)
{
  g_mutex_init (&jbtimers->lock);
  g_cond_init (&jbtimers->cond);

  jbtimers->clock = NULL;

  // XXX
}

static void
jb_timers_class_init (JBTimersClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = jb_timers_finalize;
  gobject_class->set_property = jb_timers_set_property;
  gobject_class->get_property = jb_timers_get_property;

  // XXX: add properties
  g_object_class_install_property (gobject_class, PROP_RTX_DELAY_REORDER,
      g_param_spec_int ("rtx-delay-reorder", "RTX Delay Reorder",
          "Sending retransmission event when this much reordering "
          "(0 disable)",
          0, G_MAXINT, DEFAULT_RTX_DELAY_REORDER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  // XXX: add signals

  GST_DEBUG_CATEGORY_INIT (jb_timers_debug, "jbtimers", 0,
      "Jitter Buffer Timers");
}

