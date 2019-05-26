#ifndef __JB_TIMERS_H__
#define __JB_TIMERS_H__

#include <gst/base/gstpriqueue.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _JBTimers JBTimers;
typedef struct _JBTimersClass JBTimersClass;

#define JB_TYPE_TIMERS \
  (jb_timers_get_type())
#define JB_TIMERS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),JB_TYPE_TIMERS,JBTimers))
#define JB_TIMERS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),JB_TYPE_TIMERS,JBTimersClass))
#define JB_IS_TIMERS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),JB_TYPE_TIMERS))
#define JB_IS_TIMERS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),JB_TYPE_TIMERS))
#define JB_TIMERS_CAST(obj) \
  ((JBTimers *)(obj))

#define TIMEOUT_IMMEDIATE GST_CLOCK_TIME_NONE

struct _JBTimersClass
{
  GObjectClass parent_class;
};

typedef enum
{
  TIMER_TYPE_EXPECTED,
  TIMER_TYPE_LOST,
  TIMER_TYPE_DEADLINE,
  TIMER_TYPE_EOS
} TimerType;

typedef struct
{
  guint idx; /* REMOVEME ? */
  guint16 seqnum;
  guint num;
  TimerType type;
  GstClockTime lost_packet_pts;
  GstClockTime timeout;
  GstClockTime duration;
  GstClockTime rtx_base;
  GstClockTime rtx_delay;
  GstClockTime rtx_retry;
  GstClockTime rtx_last;
  guint num_rtx_retry;
  guint num_rtx_received;
} TimerData;


typedef struct
{
  GQueue *timers;
  GHashTable *hashtable;
} TimerQueue;

struct _JBTimers
{
  GObject object;

  GMutex lock;
  GCond cond;

  gboolean timer_running;
  GThread *timer_thread;

  GstClock *clock;
  GstClockID clock_id;
  GstClockTime base_time;

  gboolean timer_thread_running;
  gboolean timer_thread_paused;

  GArray *timers;
  TimerQueue *rtx_stats_timers;

  GstClockTime timer_timeout;
  guint16 timer_seqnum;

  /* properties */
  gint rtx_delay_reorder;

};

#define JBUF_WAIT_TIMER(priv)   G_STMT_START {            \
  GST_DEBUG ("waiting timer");                            \
  (priv)->waiting_timer++;                                \
  g_cond_wait (&(priv)->jbuf_timer, &(priv)->jbuf_lock);  \
  (priv)->waiting_timer--;                                \
  GST_DEBUG ("waiting timer done");                       \
} G_STMT_END
#define JBUF_SIGNAL_TIMER(priv) G_STMT_START {            \
  if (G_UNLIKELY ((priv)->waiting_timer)) {               \
    GST_DEBUG ("signal timer, %d waiters", (priv)->waiting_timer); \
    g_cond_signal (&(priv)->jbuf_timer);                  \
  }                                                       \
} G_STMT_END


GType jb_timers_get_type (void);

TimerData *
jb_timers_get_next_timer (JBTimers * jbtimers);

TimerData *
jb_timers_find_timer (JBTimers * jbtimers, guint16 seqnum);

gboolean
jb_timers_seqnum_is_already_lost (JBTimers * jbtimers, guint16 seqnum);

void
jb_timers_time_out_reordered (JBTimers * jbtimers, guint16 current_seqnum);

TimerData *
jb_timers_add_timer (JBTimers * jbtimers, TimerType type, guint16 seqnum,
    guint num, GstClockTime timeout, GstClockTime delay, GstClockTime duration);

void
jb_timers_reschedule_timer (JBTimers * jbtimers, TimerData * timer,
    TimerType type, guint16 seqnum, GstClockTime timeout, GstClockTime delay,
    gboolean reset);

void
jb_timers_remove_timer (JBTimers * jbtimers, TimerData * timer);

void
jb_timers_remove_all_timers (JBTimers * jbtimers);

void
jb_timers_set_clock_and_base_time (JBTimers * jbtimers, GstClock *clock,
    GstClockTime base_time);

JBTimers *
jb_timers_new (gint rtx_delay_reorder);

G_END_DECLS

#endif /* __JB_TIMERS_H__ */
