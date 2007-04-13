/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "stats.h"
#include "messages.h"

#include <string.h>

typedef struct _StatsCounter
{
  guint ref_cnt;
  StatsCounterType type;
  gchar *name;
  guint32 counter;
} StatsCounter;

GList *counters;

static gboolean
stats_cmp_name(StatsCounter *sc, const gchar *name)
{
  return strcmp(sc->name, name);
}

static GList *
stats_find_counter(const gchar *counter_name)
{
  return g_list_find_custom(counters, counter_name, (GCompareFunc) stats_cmp_name);
}

/**
 * stats_register_counter:
 * @type: counter type 
 * @counter_name: name to identify this stats counter
 * @counter: returned pointer to the counter
 * @shared: whether multiple sources will use the same counter
 *
 * This fuction registers a general purpose counter. Whenever multiple
 * objects touch the same counter all of these should register the counter
 * with the same name, specifying TRUE for the value of permit_dup,
 * internally the stats subsystem counts the number of users of the same
 * counter in this case, thus the counter will only be freed when all of
 * these uses are unregistered.
 **/
void
stats_register_counter(StatsCounterType type, const gchar *counter_name, guint32 **counter, gboolean shared)
{
  StatsCounter *sc;
  GList *l;
  
  /* FIXME: we should use a separate name-space for all different types,
   * however we only have a single type so far */
  g_assert(type < SC_TYPE_MAX);

  *counter = NULL;
  if (!counter_name)
    return;
  if ((l = stats_find_counter(counter_name)))
    {
      if (!shared)
        {
          msg_notice("Duplicate stats counter",  
                     evt_tag_str("counter", counter_name), 
                     NULL);
          *counter = NULL;
        }
      else
        {
          sc = (StatsCounter *) l->data;
          sc->ref_cnt++;
          *counter = &sc->counter;
        }
      return;
    }
  
  sc = g_new(StatsCounter, 1);
  
  sc->type = type;
  sc->name = g_strdup(counter_name);
  sc->counter = 0;
  sc->ref_cnt = 1;
  *counter = &sc->counter;
  counters = g_list_prepend(counters, sc);
}

void
stats_unregister_counter(const gchar *counter_name, guint32 **counter)
{
  StatsCounter *sc;
  GList *l;
  
  if (!counter_name)
    return;
    
  l = stats_find_counter(counter_name);
  if (!l)
    {
      msg_error("Internal error unregistering stats counter, counter not found",
                evt_tag_str("counter", counter_name),
                NULL);
      return;
    }
  sc = (StatsCounter *) l->data;
  if (&sc->counter != *counter)
    {
      msg_error("Internal error unregistering stats counter, counter mismatch",
                evt_tag_str("counter", counter_name),
                NULL);
    }
  sc->ref_cnt--;
  if (sc->ref_cnt == 0)
    {
      counters = g_list_delete_link(counters, l);
      g_free(sc->name);
      g_free(sc);
    }
  *counter = NULL;
}

void
stats_generate_log(void)
{
  EVTREC *e;
  GList *l;
  gchar *tag_names[SC_TYPE_MAX] =
  {
    [SC_TYPE_DROPPED] = "dropped",
  };
  
  e = msg_event_create(EVT_PRI_NOTICE, "Log statistics", NULL);
  for (l = counters; l; l = l->next)
    {
      StatsCounter *sc = (StatsCounter *) l->data;
      
      evt_rec_add_tag(e, evt_tag_printf(tag_names[sc->type], "%s=%u", sc->name, sc->counter));
    }
  msg_event_send(e);
}