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

struct _JBTimers
{
  GObject object;

  GMutex lock;
  GCond cond;

  GstClock *clock;
  GstClockTime base_time;

  gboolean timer_thread_running;
  gboolean timer_thread_paused;

  GstPriQueue *timer_pq;
  GstPriQueue *expected_timers_pq;
  GHashTable *timer_seqmap;

  /* properties */
  gint rtx_delay_reorder;

};

//static guint jb_timers_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (JBTimers, jb_timers, G_TYPE_OBJECT);

/* GObject vmethods */
static void jb_timers_finalize (GObject * object);
static void jb_timers_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void jb_timers_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gint
compare_timeouts (GstClockTime a, GstClockTime b)
{
  gboolean a_is_immediate = (a == TIMEOUT_IMMEDIATE);
  gboolean b_is_immediate = (a == TIMEOUT_IMMEDIATE);

  if (a == b)
    return 0;

  if (a_is_immediate != b_is_immediate)
    return (gint) b_is_immediate - (gint) a_is_immediate;

  return a < b ? -1 : 1;
}

static gint
pq_compare_timers (const GstPriQueueElem * a, const GstPriQueueElem * b,
    G_GNUC_UNUSED gpointer user_data)
{
  const TimerData *timer_a = GST_CONTAINER_OF (a, TimerData, pq_elem);
  const TimerData *timer_b = GST_CONTAINER_OF (b, TimerData, pq_elem);
  gint timeout_cmp;

  timeout_cmp = compare_timeouts (timer_a->timeout, timer_b->timeout);
  if (timeout_cmp != 0)
    return timeout_cmp;

  return -gst_rtp_buffer_compare_seqnum (timer_a->seqnum, timer_b->seqnum);
}

static gint
pq_compare_timer_seqnums (const GstPriQueueElem * a, const GstPriQueueElem * b,
    G_GNUC_UNUSED gpointer user_data)
{
  const TimerData *timer_a = GST_CONTAINER_OF (a, TimerData, pq_expected_elem);
  const TimerData *timer_b = GST_CONTAINER_OF (b, TimerData, pq_expected_elem);

  return -gst_rtp_buffer_compare_seqnum (timer_a->seqnum, timer_b->seqnum);
}

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

#if 0

static inline GstClockID
new_single_shot_clock_id_for_timeout (JBTimers * jbtimers, GstClockTime timeout)
{
  GstClockTime sync_time;

  sync_time = timeout + jbtimers->base_time + jbtimers->peer_latency;
  return gst_clock_new_single_shot_id (jbtimers->clock, sync_time);
}

static inline gboolean
timer_thread_blocked (JBTimers * jbtimers)
{
  return GST_LIKELY (jbtimers->timer_thread_running) &&
      (jbtimers->timer_thread_paused ||
          !gst_pri_queue_size (jbtimers->timer_pq));
}

static gpointer
timer_thread (gpointer arg)
{
  JBTimers * jbtimers = arg;
  GstClockTime now = 0;
  GstClockTime timer_timeout;

  g_mutex_lock (&jbtimers->lock);

  while (TRUE) {
    if (timer_thread_blocked (jbtimers)) {
      g_cond_wait (&jbtimers->cond, &jbtimers->lock);
      continue;
    }

    if (GST_UNLIKELY (!jbtimers->timer_thread_running))
      break;

    timer = jb_timers_get_next_timer (jbtimers);
    if (timer->timeout != TIMEOUT_IMMEDIATE && timer->timeout > now) {
      if (jbtimers->clock)
        now = gst_clock_get_time (jbtimers->clock) - jbtimers->base_time;

      GST_DEBUG_OBJECT (jbtimers, "now %" GST_TIME_FORMAT, GST_TIME_ARGS (now));
    }

    // XXX: clear expired rtx-stats timers

    if (timer->timeout == TIMEOUT_IMMEDIATE || timer->timeout < now) {
      // XXX: invoke callback
      // XXX: what to do, we can't safely let go of timer mutex, but we can't
      // hold it either because of deadlock!
    }

  }

  g_mutex_unlock (&jbtimers->lock);
  return NULL;
}
#endif

/*
 * Public API
 */

TimerData *
jb_timers_get_next_timer (JBTimers * jbtimers)
{
  GstPriQueueElem *elem;

  elem = gst_pri_queue_get_min (jbtimers->timer_pq);
  if (!elem)
    return NULL;

  return GST_CONTAINER_OF (elem, TimerData, pq_elem);
}

TimerData *
jb_timers_find_timer (JBTimers * jbtimers, guint16 seqnum)
{
  gboolean found;
  gpointer value;
  TimerData *timer;

  found = g_hash_table_lookup_extended (jbtimers->timer_seqmap,
      GUINT_TO_POINTER (seqnum), NULL, &value);
  if (!found)
    return NULL;

  timer = value;

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
  gboolean found;
  gpointer value;
  TimerData *timer;

  found = g_hash_table_lookup_extended (jbtimers->timer_seqmap,
      GUINT_TO_POINTER (seqnum), NULL, &value);
  if (!found)
    return FALSE;

  timer = value;
  return (timer->num > 1 && timer->type == TIMER_TYPE_LOST);
}

/* Time out (reschedule as immediate) the timer of every packet that we are
 * still expecting, where the seqnum precedes the current seqnum by more than
 * the max reorder distance, and for which we have not yet sent an RTX request.
 */
void
jb_timers_time_out_reordered (JBTimers * jbtimers, guint16 current_seqnum)
{
  GstPriQueueElem *elem;
  TimerData *timer;
  gint gap;

  if (!reorder_limit_enabled (jbtimers))
    return;

  while ((elem = gst_pri_queue_get_min (jbtimers->expected_timers_pq))) {
    timer = GST_CONTAINER_OF (elem, TimerData, pq_expected_elem);

    gap = gst_rtp_buffer_compare_seqnum (timer->seqnum, current_seqnum);
    if (gap <= jbtimers->rtx_delay_reorder)
      break;

    jb_timers_reschedule_timer (jbtimers, timer, timer->type, timer->seqnum,
        TIMEOUT_IMMEDIATE, 0, FALSE);
  }
}

TimerData *
jb_timers_add_timer (JBTimers * jbtimers, TimerType type, guint16 seqnum,
    guint num, GstClockTime timeout, GstClockTime delay, GstClockTime duration)
{
  TimerData *timer;
  guint16 i;

  timer = alloc_timer ();

  timer->type = type;
  timer->seqnum = seqnum;
  timer->num = num;
  timer->timeout = timeout + delay;
  timer->duration = duration;

  if (type == TIMER_TYPE_EXPECTED) {
    // XXX: should these be set to 0 if not expected timer?
    timer->rtx_base = timeout;
    timer->rtx_delay = delay;
    timer->rtx_retry = 0;
  }

  timer->rtx_last = GST_CLOCK_TIME_NONE;
  timer->num_rtx_retry = 0;
  timer->num_rtx_received = 0;

  gst_pri_queue_insert (jbtimers->timer_pq, &timer->pq_elem);

  for (i = 0; i < num; i++) {
    gpointer seqkey_add = GUINT_TO_POINTER ((guint16) (seqnum + i));
    g_hash_table_insert (jbtimers->timer_seqmap, seqkey_add, timer);
  }

  timer->expected_inserted = FALSE;
  if (reorder_limit_enabled (jbtimers) && timer_is_initial_expected (timer)) {
    gst_pri_queue_insert (jbtimers->expected_timers_pq,
        &timer->pq_expected_elem);
    timer->expected_inserted = TRUE;
  }

  return timer;
}

void
jb_timers_reschedule_timer (JBTimers * jbtimers, TimerData * timer,
    TimerType type, guint16 seqnum, GstClockTime timeout, GstClockTime delay,
    gboolean reset)
{
  GstClockTime new_timeout;
  gboolean seqnum_change, timeout_change;
  guint16 old_seqnum;
  guint16 i;

  old_seqnum = timer->seqnum;
  new_timeout = timeout + delay;

  seqnum_change = old_seqnum != seqnum;
  timeout_change = timer->timeout != new_timeout;

  timer->type = type;
  timer->timeout = new_timeout;
  timer->seqnum = seqnum;

  if (reset) {
    /* XXX
    GST_DEBUG_OBJECT (jitterbuffer, "reset rtx delay %" GST_TIME_FORMAT
        "->%" GST_TIME_FORMAT, GST_TIME_ARGS (timer->rtx_delay),
        GST_TIME_ARGS (delay));
    */
    timer->rtx_base = timeout;
    timer->rtx_delay = delay;
    timer->rtx_retry = 0;
  }

  if (seqnum_change || timeout_change)
    gst_pri_queue_update (jbtimers->timer_pq, &timer->pq_elem);

  if (seqnum_change) {
    timer->num_rtx_retry = 0;
    timer->num_rtx_received = 0;

    for (i = 0; i < timer->num; i++) {
      gpointer seqkey_rem = GUINT_TO_POINTER ((guint16) (old_seqnum + i));
      gpointer seqkey_add = GUINT_TO_POINTER ((guint16) (seqnum + i));
      g_hash_table_remove (jbtimers->timer_seqmap, seqkey_rem);
      g_hash_table_insert (jbtimers->timer_seqmap, seqkey_add, timer);
    }
  }

  if (reorder_limit_enabled (jbtimers)) {
    if (timer_is_initial_expected (timer)) {
      if (!timer->expected_inserted) {
        gst_pri_queue_insert (jbtimers->expected_timers_pq,
            &timer->pq_expected_elem);
        timer->expected_inserted = TRUE;
      } else if (seqnum_change) {
        gst_pri_queue_update (jbtimers->expected_timers_pq,
            &timer->pq_expected_elem);
      }
    } else if (timer->expected_inserted) {
      gst_pri_queue_remove (jbtimers->expected_timers_pq,
          &timer->pq_expected_elem);
      timer->expected_inserted = FALSE;
    }
  }
}

void
jb_timers_remove_timer (JBTimers * jbtimers, TimerData * timer)
{
  guint16 i;

  gst_pri_queue_remove (jbtimers->timer_pq, &timer->pq_elem);

  for (i = 0; i < timer->num; i++) {
    gpointer seqkey_rem = GUINT_TO_POINTER ((guint16) (timer->seqnum + i));
    g_hash_table_remove (jbtimers->timer_seqmap, seqkey_rem);
  }

  if (reorder_limit_enabled (jbtimers) && timer->expected_inserted) {
    gst_pri_queue_remove (jbtimers->expected_timers_pq,
        &timer->pq_expected_elem);
  }

  free_timer (timer);
}

void
jb_timers_remove_all_timers (JBTimers * jbtimers)
{
  gst_pri_queue_destroy (jbtimers->expected_timers_pq, NULL);
  jbtimers->expected_timers_pq =
      gst_pri_queue_create (pq_compare_timer_seqnums, NULL);

  g_hash_table_destroy (jbtimers->timer_seqmap);
  jbtimers->timer_seqmap = g_hash_table_new (g_direct_hash, g_direct_equal);

  gst_pri_queue_destroy (jbtimers->timer_pq, free_timer);
  jbtimers->timer_pq = gst_pri_queue_create (pq_compare_timers, NULL);
}

void
jb_timers_set_clock_and_base_time (JBTimers * jbtimers, GstClock *clock,
    GstClockTime base_time)
{
  // ERLEND XXX: note: `clock` can never be NULL!

  // XXX: acquire mutex

  if (jbtimers->clock) {
    // XXX: unschedule wait before unref (gst_clock_new_single_shot_id does not take ref)!

    gst_object_unref (jbtimers->clock);
  }

  jbtimers->clock = gst_object_ref (clock);
  jbtimers->base_time = base_time;

  // XXX: release mutex
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

  return jbtimers;
}

/*
 * GObject API
 */

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

static void
jb_timers_init (JBTimers * jbtimers)
{
  g_mutex_init (&jbtimers->lock);
  g_cond_init (&jbtimers->cond);

  jbtimers->clock = NULL;

  // XXX

  jbtimers->timer_pq = gst_pri_queue_create (pq_compare_timers, NULL);
  jbtimers->expected_timers_pq =
      gst_pri_queue_create (pq_compare_timer_seqnums, NULL);
  jbtimers->timer_seqmap = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
jb_timers_finalize (GObject * object)
{
  JBTimers *jbtimers = JB_TIMERS (object);

  // XXX

  gst_pri_queue_destroy (jbtimers->expected_timers_pq, NULL);
  g_hash_table_destroy (jbtimers->timer_seqmap);
  gst_pri_queue_destroy (jbtimers->timer_pq, free_timer);

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
